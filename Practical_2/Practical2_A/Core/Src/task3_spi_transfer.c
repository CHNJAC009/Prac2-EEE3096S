/**
  ******************************************************************************
  * @file    task3_spi_transfer.c
  * @brief   TASK 3 : SEND ONE BYTE AND PROVE THE WAVEFORM
  ******************************************************************************
  */

#include "prac2a.h"

/* ==========================================================================
 * Chip select
 * ========================================================================== */

void eeprom_cs_low(void)
{
    EE_SPI_GPIO->BRR = EE_CS_MASK;          /* RM0091 GPIOx_BRR             */
}

void eeprom_cs_high(void)
{
    /* TODO 3.3  Raise CS - but only once the peripheral has completely
     *           finished transmitting. A received byte does not mean the
     *           last bits have left the shift register. RM0091 lists the
     *           status flags to wait for (see its procedure for disabling the
     *           SPI). Raising CS early cuts the end off a command.
     * 
     * RM0091 §28.5.9 "Procedure for disabling the SPI", p.767-768:
     *   "1. Wait until FTLVL[1:0] = 00 (no more data to transmit).
     *    2. Wait until BSY=0 (the last data frame is processed)."
     * Both must be true before CS can go HIGH:
     *
     * 1. FTLVL[1:0] (bits 12:11 of SPI_SR) = 00 → TX FIFO is empty
     *    Nothing is left queued to send. NOT the same as TXE: TXE = 1
     *    whenever the FIFO is at most half full (RM0091 §28.5.10 p.776),
     *    so TXE can be 1 with 1-2 bytes still waiting.
     *
     * 2. BSY (bit 7 of SPI_SR) = 0 → shift register is finished
     *    The last bit has left the pin. Checked AFTER FTLVL, as the SR
     *    description warns "The BSY flag must be used with caution".
     *    Raising CS before BSY clears will cut the end off the frame.
     * RM0091 §28.9.3 SPI status register (SPIx_SR), p.806
     */

    /* Wait for TX FIFO empty (FTLVL = 00) */
    while (EE_SPI->SR & (3UL << 11))    /* FTLVL[1:0] bits 12:11, (3UL << 11) same as SPI_SR_FTLVL mask */
    {
        /* spin */
    }

    /* Wait for shift register to finish (BSY goes LOW) */
    while (EE_SPI->SR & SPI_SR_BSY)
    {
        /* spin */
    }

    /* Now safe to raise CS — last bit has left the shift register */
    EE_SPI_GPIO->BSRR = EE_CS_MASK;         /* RM0091 GPIOx_BSRR */
}

/* ==========================================================================
 * TASK 3
 * ========================================================================== */

uint8_t spi_transfer(uint8_t tx)
{
    /* TODO 3.4  Wait until the transmit buffer has room (SPI_SR).
    *
    * SPI_SR bit 1 = TXE (Transmit buffer Empty)
    * Must be 1 before writing to DR, otherwise we overwrite
    * a byte that hasn't been sent yet.
    * RM0091 Section 27.3.9 and Section 27.7.3 SPI status register (SPIx_SR)
    */

    while (!(EE_SPI->SR & SPI_SR_TXE))
    {
        /* spin until transmit buffer is empty */
    }

    /* TODO 3.5  Write tx to the data register.
     *           HINT: SPI_DR is declared 16 bits wide in the CMSIS header,
     *           and on the STM32F0 the WIDTH of the write matters. Count the
     *           clock pulses per call on your scope: 16 instead of 8 means
     *           this is the problem.
     * 
     * CRITICAL: Must cast to volatile uint8_t pointer before writing.
     * The CMSIS header declares SPI_DR as uint16_t. If you write a
     * 16-bit value, the STM32 sends 16 clock pulses instead of 8.
     * A uint8_t pointer forces an 8-bit bus write → exactly 8 clocks.
     * RM0091 Section 27.7.4 SPI data register (SPIx_DR)
     */
    *((volatile uint8_t *)&EE_SPI->DR) = tx;

    /* TODO 3.6  Wait for the received byte (SPI_SR), then read it from the
     *           data register and return it. Read it every time, even when
     *           you do not need the value.
     * 
     * SPI_SR bit 0 = RXNE (Receive buffer Not Empty)
     * The SPI peripheral shifts in a byte for every byte it shifts out.
     * With FRXTH = 1 (TODO 2.8), RXNE goes high once the RX FIFO holds
     * 1 byte - RM0091 §28.5.10 "SPI status flags", p.776.
     * Must read DR every transfer even if the value is not needed,
     * otherwise the RX FIFO fills up and the next received byte is
     * discarded with OVR set - RM0091 §28.5.11 "Overrun flag", p.777.
     * RM0091 §28.9.3 SPI status register (SPIx_SR), p.806
     */

    while (!(EE_SPI->SR & SPI_SR_RXNE))
    {
        /* spin until received byte is ready */
    }

    /* Read DR as an 8-bit access, same reason as the write in TODO 3.5.
     * A 16-bit read triggers data packing: "data packing is used
     * automatically when any read or write 16-bit access is performed on the
     * SPIx_DR register" - it tries to pop TWO bytes from the RX FIFO.
     * RM0091 §28.5.9 "Data packing", p.768; §28.9.4 SPIx_DR, p.807 */
    return *((volatile uint8_t *)&EE_SPI->DR);
}

/* ==========================================================================
 * RUN_TASK 3 - given
 * Sends task3_test_byte every 100 ms with CS held HIGH, so the EEPROM ignores
 * it and your oscilloscope has a repeating burst to trigger on.
 * task3_test_byte can be changed from Live Expressions without a rebuild.
 * ========================================================================== */

#define TASK3_TX_PERIOD_MS  100u

volatile uint8_t  task3_test_byte = (uint8_t)TEST_BYTE_B;
volatile uint8_t  task3_rx_byte   = 0u;
volatile uint32_t task3_tx_count  = 0u;

static uint32_t t3_last = 0u;

void task3_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
}

void task3_loop(uint32_t now)
{
    task1_gpio_update(now);

    if ((uint32_t)(now - t3_last) < TASK3_TX_PERIOD_MS)
    {
        return;
    }
    t3_last = now;

    task3_rx_byte = spi_transfer(task3_test_byte);
    task3_tx_count++;
}
