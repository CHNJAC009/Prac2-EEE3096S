/**
  ******************************************************************************
  * @file    task6_fsm.c
  * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
  *
  * Restructure the Task 4 transaction so the main loop never stops:
  *
  *   request -> write-enable -> write -> wait for EEPROM
  *           -> read-back -> verify -> result
  *
  * Rules (handout, Task 6):
  *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
  *     write cycle. You may use HAL_GetTick() to decide when the next status
  *     check is due.
  *   - Each call does a small amount of work, updates the state, and returns.
  *   - Do not put the whole transaction inside one blocking function called
  *     from the loop.
  *   - While a transaction is in progress the loop must still respond to PA3.
  *
  * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
  * (green) a successful verification, PB10 (red) a failed one.
  ******************************************************************************
  */

#include "prac2a.h"

/* TODO 6.1  Design your states. You need enough of them to tell apart at
 *           least: idle, write preparation, write transaction, EEPROM busy /
 *           status checking, read transaction, verification, and success or
 *           failure.
 *
 *           Draw the diagram first. It must show the initial state, the
 *           condition on every transition, and the success, failure and
 *           abort paths.
 *
 *           typedef enum { ... } ee_state_t;
 *
 * DESIGN: one SPI transaction (one CS-low ... CS-high frame) per state, so
 * every call does at most a few hundred microseconds of work and returns.
 * Values are fixed so the number in ee_state (Live Expressions) can be read
 * straight off this list.
 *
 *   IDLE --PA0--> WRITE_PREP --ready, WREN sent--> WRITE --WEL=1, WRITE sent-->
 *   WAIT_BUSY --RDY=0--> READ --byte read--> VERIFY --match--> SUCCESS
 *                                                   --no match--> FAIL
 *   Failure paths: bad address or EEPROM still busy after the timeout
 *   (WRITE_PREP), WEL not set after WREN (WRITE), EEPROM never ready
 *   (WAIT_BUSY), byte read back differs (VERIFY) -> FAIL.
 *   Abort path: PA3 in any busy state (1..5) -> IDLE.
 *   SUCCESS / FAIL hold the result until the next PA0 starts again.
 */
typedef enum
{
    EE_IDLE       = 0,  /* waiting for PA0; nothing in progress                */
    EE_WRITE_PREP = 1,  /* write preparation: EEPROM ready? then send WREN     */
    EE_WRITE      = 2,  /* write transaction: check WEL, send WRITE + addr + B */
    EE_WAIT_BUSY  = 3,  /* EEPROM busy: one RDSR per poll interval until RDY=0 */
    EE_READ       = 4,  /* read transaction: READ + addr + dummy               */
    EE_VERIFY     = 5,  /* compare the byte read with the byte written         */
    EE_SUCCESS    = 6,  /* result: verified (green)                            */
    EE_FAIL       = 7   /* result: failed / timed out (red)                    */
} ee_state_t;

/* Time between status checks while the EEPROM is busy. HAL_GetTick() counts
 * in 1 ms steps, so 1 ms is the shortest interval it can schedule. */
#define EE_POLL_MS      1u

volatile uint8_t ee_state       = 0u;
volatile uint8_t ee_last_read   = 0u;
volatile uint8_t ee_use_fsm     = 1u;

/* Demo aid (writable in Live Expressions): minimum time each busy state is
 * held before it does its work. 0 = full speed (a whole transaction takes
 * ~4 ms, too fast to watch). Set e.g. 1000 to see ee_state step 1,2,3,4,5,6
 * once a second and to have time to press PA3. It only delays WHEN the next
 * step runs (checked with HAL_GetTick, never a wait loop); completion is still
 * decided by the status register. */
volatile uint32_t ee_step_ms    = 0u;

/* Number of RDSR status checks made while the EEPROM was busy, last run. */
volatile uint32_t ee_poll_count = 0u;

/* Number of main-loop passes during the last transaction (PA0 to result).
 * Many passes (far more than ee_poll_count) during a ~4 ms transaction = the
 * loop never stopped. */
volatile uint32_t ee_busy_passes = 0u;

static uint16_t     ee_addr;          /* address/byte latched when PA0 starts, */
static uint8_t      ee_data;          /* so Live Expression edits mid-run are safe */
static uint32_t     ee_state_entry;   /* tick when the current state was entered */
static uint32_t     ee_last_check;    /* tick of the last status check         */
static uint32_t     ee_write_start;   /* tick when CS rose after the WRITE     */
static status_led_t ee_status_led = STATUS_OFF;

/* Move to a new state and remember when we got there. */
static void ee_goto(ee_state_t next, uint32_t now)
{
    ee_state       = (uint8_t)next;
    ee_state_entry = now;
}

/* The WRITE frame on its own: no WREN, no polling - unlike
 * eeprom_write_byte(), which blocks until the write cycle ends.
 * CAT25010 Figure 5 "Byte WRITE Timing", p.7, with the 2-byte address of the
 * fitted part (README §5.2). Internal programming starts when CS goes HIGH
 * (CAT25010 "Byte Write", p.7). */
static void ee_send_write(uint16_t address, uint8_t value)
{
    eeprom_cs_low();
    spi_transfer(EEPROM_CMD_WRITE);                      /* 0x02             */
    spi_transfer((uint8_t)((address >> 8) & 0xFFu));     /* A15..A8          */
    spi_transfer((uint8_t)(address & 0xFFu));            /* A7..A0           */
    spi_transfer(value);                                 /* data             */
    eeprom_cs_high();                                    /* write cycle starts */
}

void update_eeprom_state_machine(uint32_t now)
{
    /* TODO 6.2  One step of your state machine.
     *
     *   - btn_start_edge (PA0) starts a transaction from idle.
     *   - btn_abort_edge (PA3) abandons the transaction in progress and returns
     *     to idle. Leave the SPI bus in a state the next transaction can use.
     *   - While the EEPROM is busy writing, do NOT wait in here. Work out when
     *     the next status check is due, remember it, and return.
     *   - Decide that the write has finished from the status register, never
     *     from elapsed time alone. Also decide what happens if the EEPROM never
     *     reports ready.
     *   - Store the byte read back in ee_last_read.
     *   - Clear each button edge once you have acted on it. */

    uint8_t busy = (ee_state >= EE_WRITE_PREP) && (ee_state <= EE_VERIFY);
    uint8_t status;

    /* ABORT (PA3) - checked first, on every pass, so it is never delayed by
     * the transaction. Each state runs whole SPI frames (CS low ... CS high)
     * inside a single call, so between calls CS is always HIGH and the SPI
     * idle: the bus is ready for the next transaction. If the EEPROM was
     * mid-write it finishes on its own; WRITE_PREP checks RDY before the next
     * WREN, because only RDSR is accepted while it is busy (CAT25010 p.7). */
    if (btn_abort_edge)
    {
        btn_abort_edge = 0u;
        if (busy)
        {
            ee_status_led = STATUS_OFF;
            ee_goto(EE_IDLE, now);
            return;
        }
    }

    /* START (PA0) - only when no transaction is running. A press while busy is
     * ignored, so a second press cannot start a write on top of the first. */
    if (btn_start_edge)
    {
        btn_start_edge = 0u;
        if (!busy)
        {
            ee_addr       = eeprom_test_addr;
            ee_data       = eeprom_test_byte;
            ee_poll_count  = 0u;
            ee_busy_passes = 0u;
            ee_status_led  = STATUS_OFF;             /* both off = in progress */
            ee_last_check = now;
            ee_goto(EE_WRITE_PREP, now);
            return;
        }
    }

    if (busy)
    {
        ee_busy_passes++;
    }

    /* Demo slow-step: hold a busy state until ee_step_ms has passed. */
    if (busy && ((uint32_t)(now - ee_state_entry) < ee_step_ms))
    {
        return;
    }

    switch ((ee_state_t)ee_state)
    {
    case EE_WRITE_PREP:
        /* Write preparation: the address must exist (0..8191, README §5.2)
         * and the EEPROM must be ready (RDY = 0) before WREN, e.g. after an
         * abort during a write cycle. One RDSR per poll. */
        if (ee_addr >= EEPROM_SIZE_BYTES)
        {
            ee_status_led = STATUS_FAIL;
            ee_goto(EE_FAIL, now);
            break;
        }
        if ((uint32_t)(now - ee_last_check) < EE_POLL_MS)
        {
            return;                                  /* next check not due */
        }
        ee_last_check = now;
        status = eeprom_read_status();
        if (status & EEPROM_SR_RDY)                  /* still busy          */
        {
            if ((uint32_t)(now - ee_state_entry) >= EEPROM_WRITE_TIMEOUT_MS)
            {
                eeprom_timeout_count++;
                ee_status_led = STATUS_FAIL;
                ee_goto(EE_FAIL, now);
            }
            return;
        }
        eeprom_status_before = status;
        eeprom_write_enable();                       /* WREN: own frame, WEL sets
                                                        on CS rising (CAT25010 p.6) */
        ee_goto(EE_WRITE, now);
        break;

    case EE_WRITE:
        /* Write transaction. First confirm WREN worked (WEL = 1, CAT25010
         * Table 10 p.6), then send the WRITE frame. */
        status = eeprom_read_status();
        if ((status & EEPROM_SR_WEL) == 0u)
        {
            ee_status_led = STATUS_FAIL;             /* write not enabled   */
            ee_goto(EE_FAIL, now);
            break;
        }
        ee_send_write(ee_addr, ee_data);
        ee_write_start = now;
        ee_last_check  = now;
        ee_goto(EE_WAIT_BUSY, now);
        break;

    case EE_WAIT_BUSY:
        /* EEPROM busy: do NOT wait here. If the next check is not due yet,
         * return at once; otherwise make ONE status read and return. The
         * main loop (and PA3) keeps running between checks. */
        if ((uint32_t)(now - ee_last_check) < EE_POLL_MS)
        {
            return;
        }
        ee_last_check = now;
        status = eeprom_read_status();
        ee_poll_count++;
        if ((status & EEPROM_SR_RDY) == 0u)
        {
            /* Done - decided by RDY = 0 in the status register (CAT25010
             * p.5, p.7), not by elapsed time. Checked BEFORE the timeout so a
             * late check (e.g. slow-step) still sees a finished write. */
            eeprom_status_after  = status;
            eeprom_write_wait_ms = (uint32_t)(now - ee_write_start);
            ee_goto(EE_READ, now);
        }
        else if ((uint32_t)(now - ee_write_start) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            /* Never reported ready: hang guard only (tWC max 5 ms, CAT25010
             * Table 7 p.4), so the FSM cannot stay busy forever. */
            eeprom_timeout_count++;
            ee_status_led = STATUS_FAIL;
            ee_goto(EE_FAIL, now);
        }
        break;

    case EE_READ:
        /* Read transaction: READ + 2 address bytes + dummy, one frame. */
        ee_last_read      = eeprom_read_byte(ee_addr);
        eeprom_read_value = ee_last_read;
        ee_goto(EE_VERIFY, now);
        break;

    case EE_VERIFY:
        eeprom_verify_ok = (ee_last_read == ee_data) ? 1u : 0u;
        if (eeprom_verify_ok)
        {
            ee_status_led = STATUS_PASS;
            ee_goto(EE_SUCCESS, now);
        }
        else
        {
            ee_status_led = STATUS_FAIL;
            ee_goto(EE_FAIL, now);
        }
        break;

    case EE_IDLE:
    case EE_SUCCESS:
    case EE_FAIL:
    default:
        break;                                       /* wait for PA0        */
    }
}

void update_outputs(void)
{
    /* TODO 6.3  PB0..PB7 show ee_last_read (leds_write_byte). Green after a
     *           successful verification, red after a failed one
     *           (status_leds_show). Decide what the status LEDs should show
     *           while a transaction is in progress, and after an abort. */

    /* PB0..PB7 = last byte read. One BSRR write, PB8..PB15 untouched. */
    leds_write_byte(ee_last_read);

    /* Green = verified, red = failed, BOTH OFF = transaction in progress or
     * aborted (ee_status_led is set on each transition above). */
    status_leds_show(ee_status_led);
}

/* ==========================================================================
 * RUN_TASK 6 - given
 *
 * The main loop has exactly the shape the handout asks for:
 *     read_inputs();  update_eeprom_state_machine();  update_outputs();
 *
 * Set ee_use_fsm = 0 in Live Expressions to run the Task 4 blocking path on
 * PA0 instead - useful when you explain why yours is non-blocking. PC13 keeps
 * toggling as a heartbeat; watch it on the scope in both modes.
 * ========================================================================== */

void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;

    /* TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test. */
    ee_status_led = eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL;
    ee_goto(EE_IDLE, HAL_GetTick());
}

void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (ee_use_fsm)
    {
        update_eeprom_state_machine(now);
        update_outputs();
    }
    else
    {
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}
