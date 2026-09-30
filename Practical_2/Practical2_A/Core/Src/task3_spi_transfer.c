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
     * Two conditions must BOTH be true before CS can go HIGH:
     *
     * 1. TXE (bit 1 of SPI_SR) = 1 → transmit buffer is empty
     *    This means software can write the next byte, but the shift
     *    register may still be clocking out the current byte.
     *
     * 2. BSY (bit 7 of SPI_SR) = 0 → shift register is finished
     *    This is the definitive "last bit has left the pin" flag.
     *    Raising CS before BSY clears will cut the end off the frame.
     * RM0091 Section 27.3.8 "Disabling the SPI" lists this exact sequence.
     */
    
    /* Wait for transmit buffer empty */
    while (!(EE_SPI->SR & SPI_SR_TXE))
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
     * Must read DR every transfer even if the value is not needed,
     * otherwise the receive buffer overflows and RXNE stays set,
     * blocking the next TXE wait.
     * RM0091 Section 27.7.3 SPI status register (SPIx_SR)
     */
    
    while (!(EE_SPI->SR & SPI_SR_RXNE))
    {
        /* spin until received byte is ready */
    }

    return (uint8_t)EE_SPI->DR;

    //(void)tx;
    //return 0u;
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
