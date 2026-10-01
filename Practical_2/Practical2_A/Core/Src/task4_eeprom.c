/**
  ******************************************************************************
  * @file    task4_eeprom.c
  * @brief   TASK 4 : EEPROM DRIVER
  *
  * Build the driver on spi_transfer() - no pre-written EEPROM library.
  * Command values, transaction order and status bits come from the EEPROM
  * datasheet; the address format for this board is in board_config.h.
  *
  * Write completion must be decided from the EEPROM's status register. A fixed
  * delay is not acceptable as proof that a write finished. Blocking while you
  * poll is allowed in Task 4.
  ******************************************************************************
  */

#include "prac2a.h"

volatile uint16_t eeprom_test_addr        = (uint16_t)EEPROM_ADDR_A;
volatile uint8_t  eeprom_test_byte        = (uint8_t)TEST_BYTE_B;

volatile uint8_t  eeprom_status_before    = 0u;
volatile uint8_t  eeprom_status_after     = 0u;
volatile uint8_t  eeprom_read_value       = 0u;
volatile uint8_t  eeprom_verify_ok        = 0u;
volatile uint32_t eeprom_write_wait_ms    = 0u;
volatile uint32_t eeprom_timeout_count    = 0u;

/* ==========================================================================
 * Driver
 * ========================================================================== */

uint8_t eeprom_read_status(void)
{
    /* TODO 4.3  Select the device, send the read-status instruction, clock one
     *           more byte to receive the status, deselect, and return it.
     *           Be ready to explain why you have to SEND a byte in order to
     *           RECEIVE one. 
     * Transaction (datasheet Figure 10 - RDSR timing):
     *   1. Assert CS LOW
     *   2. Send RDSR opcode (0x05)
     *   3. Clock one dummy byte to receive the status byte back
     *   4. Deassert CS HIGH
     *
     * WHY send a byte to receive one?
     * SPI is full-duplex. The master must clock SCK to receive data.
     * Every byte clocked out shifts a byte in simultaneously.
     * Sending 0x00 dummy keeps SCK running so EEPROM can shift out status.
     *
     */
    uint8_t status;

    eeprom_cs_low();
    spi_transfer(EEPROM_CMD_RDSR);   /* Send RDSR opcode              */
    status = spi_transfer(0x00u);    /* Clock in the status register  */
    eeprom_cs_high();

    return status;
}

void eeprom_write_enable(void)
{
    /* TODO 4.4  Send the write-enable instruction as one complete
     *           transaction. Check the datasheet: at what point does the write
     *           enable latch actually get set? That decides whether this could
     *           share a transaction with the write itself.
     * Transaction (datasheet Figure 3 - WREN timing):
     *   1. Assert CS LOW
     *   2. Send WREN opcode (0x06)
     *   3. Deassert CS HIGH
     *
     * CRITICAL from datasheet:
     * "Care must be taken to take the CS input HIGH after the WREN
     * instruction, as otherwise the Write Enable Latch will not be
     * properly set."
     *
     * The WEL bit is SET on the RISING EDGE of CS after the WREN command.
     * Therefore WREN MUST be its own separate transaction — it cannot be
     * combined with the WRITE command in the same CS assertion. 
     */
    eeprom_cs_low();
    spi_transfer(EEPROM_CMD_WREN);   /* Send WREN opcode  */
    eeprom_cs_high();                /* Rising CS edge sets WEL bit  */
}

void eeprom_write_byte(uint16_t address, uint8_t value)
{
    uint32_t start;

    if (address >= EEPROM_SIZE_BYTES)
    {
        return;
    }

    /* TODO 4.5  Enable writes. Then send the write instruction, the address
     *           (EEPROM_ADDR_BYTES bytes, most significant byte first) and the
     *           data byte, and deselect. From the datasheet: at what moment
     *           does the EEPROM actually start writing? 
     * Step 1: Send WREN as a separate transaction (sets WEL bit on CS rise)
     * Step 2: Write transaction (datasheet Figure 5 - Byte WRITE timing):
     *   1. Assert CS LOW
     *   2. Send WRITE opcode (0x02)
     *      For CAT25040: bit 3 of opcode = A8 (9th address bit)
     *      address >> 8 gives A8 for addresses > 255
     *   3. Send 8-bit address (A7..A0)
     *   4. Send data byte
     *   5. Deassert CS HIGH
     *
     * FROM DATASHEET:
     * "Internal programming will start after the LOW to HIGH CS transition."
     * The EEPROM starts writing internally the moment CS goes HIGH.
     */
    /* Step 1: Enable writes (separate transaction) */
    eeprom_write_enable();

    /* Step 2: Send WRITE command with address and data */
    eeprom_cs_low();

    /* Opcode: for CAT25040, bit 3 carries A8 (9th address bit)
     * WRITE opcode = 0x02, OR with (A8 << 3) if address > 255 */
    spi_transfer(EEPROM_CMD_WRITE | (uint8_t)((address >> 5u) & 0x08u));

    /* Send 8-bit address (A7..A0) */
    spi_transfer((uint8_t)(address & 0xFFu));

    /* Send data byte */
    spi_transfer(value);

    /* CS HIGH starts the internal write cycle */
    eeprom_cs_high();

    /* TODO 4.6  Wait for the write to finish by POLLING THE STATUS REGISTER.
     *           Bound the loop with EEPROM_WRITE_TIMEOUT_MS so a missing
     *           device cannot hang the board (count timeouts in
     *           eeprom_timeout_count), and record how long the write took in
     *           eeprom_write_wait_ms - your report needs that figure.
     * Poll the status register until write completes (RDY bit = 0)
     * Datasheet: tWC (write cycle time) = max 5ms
     * Use EEPROM_WRITE_TIMEOUT_MS to bound the loop — prevents infinite
     * hang if EEPROM is missing or not responding.
     * Record actual write time in eeprom_write_wait_ms for the report.
     *
     * NOTE: RDSR is the ONLY command accepted during an internal write cycle.
     * All other commands are ignored until RDY = 0. 
     */
    start = HAL_GetTick();

    while (eeprom_read_status() & EEPROM_SR_RDY)
    {
        /* RDY = 1 means write still in progress */
        if ((uint32_t)(HAL_GetTick() - start) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            eeprom_timeout_count++;
            break;
        }
    }

    eeprom_write_wait_ms = (uint32_t)(HAL_GetTick() - start);
}

uint8_t eeprom_read_byte(uint16_t address)
{
    if (address >= EEPROM_SIZE_BYTES)
    {
        return 0u;
    }

    /* TODO 4.7  Select, send the read instruction and the address, clock one
     *           more byte to receive the data, deselect, and return it.
     * Transaction (datasheet Figure 9 - READ timing):
     *   1. Assert CS LOW
     *   2. Send READ opcode (0x03)
     *      For CAT25040: bit 3 of opcode = A8 (9th address bit)
     *   3. Send 8-bit address (A7..A0)
     *   4. Clock one dummy byte to receive the data byte
     *   5. Deassert CS HIGH
     *
     * After the last address bit, EEPROM immediately shifts data out on SO. 
     */
    uint8_t data;

    if (address >= EEPROM_SIZE_BYTES)
    {
       return 0u;
    }

    eeprom_cs_low();

    /* Opcode with A8 for CAT25040 */
    spi_transfer(EEPROM_CMD_READ | (uint8_t)((address >> 5u) & 0x08u));

    /* Send 8-bit address */
    spi_transfer((uint8_t)(address & 0xFFu));

    /* Clock in data byte (send dummy 0x00 to generate SCK pulses) */
    data = spi_transfer(0x00u);

    eeprom_cs_high();

    return data;
}

/* ==========================================================================
 * LEDs
 * ========================================================================== */

void leds_write_byte(uint8_t v)
{
    /* TODO 4.8  Show v on PB0..PB7 (LED7..LED0 = bit 7..bit 0).
     *           Do it without disturbing PB10..PB15 on the same port - those
     *           carry the status LEDs, CS and the SPI pins. One register lets
     *           you set some pins and reset others in a single write. 
     * Show v on PB0..PB7 without disturbing PB10..PB15
     * (PB10=red LED, PB11=green LED, PB12=CS, PB13-15=SPI pins)
     *
     * BSRR strategy (one atomic write, no read-modify-write needed):
     *   Lower 16 bits = SET bits   (pins to drive HIGH)
     *   Upper 16 bits = RESET bits (pins to drive LOW)
     *
     * SET   bits where v = 1:  (v       ) in lower 16
     * RESET bits where v = 0:  (~v & 0xFF) in upper 16
     * This leaves PB8..PB15 completely untouched.
     */
    GPIOB->BSRR = ((uint32_t)(v))                       /* SET   high bits */
                | ((uint32_t)(~v & 0xFFu) << 16u);      /* RESET low  bits */
}

/* ==========================================================================
 * High-level paths
 * ========================================================================== */

void eeprom_read_only_path(void)
{
    /* Given. Reads, never writes - this is what makes the persistence test
     * meaningful, and it protects the EEPROM's finite write endurance. */
    eeprom_status_before = eeprom_read_status();
    eeprom_read_value    = eeprom_read_byte(eeprom_test_addr);
    eeprom_verify_ok     = (eeprom_read_value == eeprom_test_byte) ? 1u : 0u;
    leds_write_byte(eeprom_read_value);
    status_leds_show(eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL);
}

void eeprom_write_verify_path(void)
{
    /* TODO 4.9  The complete Task 4 sequence, using eeprom_test_addr and
     *           eeprom_test_byte:
     *             - read the status register     -> eeprom_status_before
     *             - write the byte (waits until the write has completed)
     *             - read the status register     -> eeprom_status_after
     *             - read the byte back           -> eeprom_read_value
     *             - compare with what you wrote  -> eeprom_verify_ok
     *             - show the byte read on the LEDs
     *             - optional: status_leds_show(STATUS_PASS or STATUS_FAIL) */
     /* Step 1: Read status BEFORE write (for report) */
    eeprom_status_before = eeprom_read_status();

    /* Step 2: Write test byte to test address
     *         (eeprom_write_byte handles WREN + polling internally) */
    eeprom_write_byte(eeprom_test_addr, eeprom_test_byte);

    /* Step 3: Read status AFTER write (WEL should be 0 - auto-cleared) */
    eeprom_status_after = eeprom_read_status();

    /* Step 4: Read back the byte we just wrote */
    eeprom_read_value = eeprom_read_byte(eeprom_test_addr);

    /* Step 5: Verify read value matches what we wrote */
    eeprom_verify_ok = (eeprom_read_value == eeprom_test_byte) ? 1u : 0u;

    /* Step 6: Display result on LEDs */
    leds_write_byte(eeprom_read_value);

    /* Step 7: Show pass/fail on status LEDs */
    status_leds_show(eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL);
}

/* ==========================================================================
 * RUN_TASK 4 (and 5) - given
 *
 *   reset / power-up : read-only path - shows the stored byte, writes nothing,
 *                      so a power-cycle proves the byte persisted
 *   PA0              : one complete eeprom_write_verify_path()
 *   eeprom_read_loop_enable = 1 (Live Expressions): repeat a READ-ONLY
 *                      transaction every 50 ms for a steady scope trace.
 *                      It never writes.
 * ========================================================================== */

#define EEPROM_READ_LOOP_MS  50u

volatile uint8_t eeprom_read_loop_enable = 0u;

static uint32_t rd_last = 0u;

void task4_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();
    eeprom_read_only_path();
}

void task4_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (btn_start_edge)
    {
        btn_start_edge = 0u;
        eeprom_write_verify_path();     /* one press, one write */
    }
    btn_abort_edge = 0u;                /* PA3 is not used in Task 4 */

    if (eeprom_read_loop_enable && ((uint32_t)(now - rd_last) >= EEPROM_READ_LOOP_MS))
    {
        rd_last = now;
        eeprom_read_only_path();
    }
}
