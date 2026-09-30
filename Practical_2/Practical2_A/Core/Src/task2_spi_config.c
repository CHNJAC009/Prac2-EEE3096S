/**
  ******************************************************************************
  * @file    task2_spi_config.c
  * @brief   TASK 2 : CONFIGURE THE HARDWARE SPI PERIPHERAL
  *
  * Configure the SPI peripheral entirely through its registers. Not allowed:
  * HAL_SPI_*, LL SPI transfer functions, CubeMX-generated SPI configuration,
  * GPIO bit-banging. Every register write you add should cite the RM0091
  * register it comes from.
  ******************************************************************************
  */

#include "prac2a.h"

volatile uint32_t dbg_gpiob_moder      = 0u;
volatile uint32_t dbg_gpiob_afrh       = 0u;
volatile uint32_t dbg_spi_cr1          = 0u;
volatile uint32_t dbg_spi_cr2          = 0u;
volatile uint32_t dbg_spi_sr           = 0u;
volatile uint32_t dbg_sck_hz_predicted = 0u;

void eeprom_spi_init(void)
{
    /* TODO 2.4  Enable the clocks this needs: the GPIO port that carries the
     *           pins AND the SPI peripheral itself. They are in different RCC
     *           enable registers - find both in RM0091. */
	RCC->AHBENR  |= (1UL << 18);   /* RM0091 §6.4.6 RCC_AHBENR, bit 18 IOPBEN, (1UL << 18) same as RCC_AHBENR_GPIOBEN mask */
	RCC->APB1ENR |= (1UL << 14);   /* RM0091 §6.4.8 RCC_APB1ENR, bit 14 SPI2EN, (1UL << 14) same as RCC_APB1ENR_SPI2EN mask */

    /* TODO 2.5  Chip select: make EE_PIN_CS a general purpose output driven
     *           HIGH. Think about the ORDER of those two steps. Be ready to
     *           explain why CS must start high. */

    /* Step 1: set the output level FIRST. BSRR bits 15:0 set the matching
     * ODR bit; writing 0 to the other bits does nothing, so plain '=' is safe
     * (RM0091 s8.4.7 GPIOx_BSRR, p.159). */
    EE_SPI_GPIO->BSRR = EE_CS_MASK;                     /* ODR12 = 1 (CS high) */

    /* Step 2: only now switch PB12 to output. MODER12 = 01 "General purpose
     * output mode" (RM0091 s8.4.1 GPIOx_MODER, p.157). Clear-then-set keeps
     * every other pin's mode (read-modify-write).
     * Output type is left at its reset value, push-pull
     * (RM0091 s8.4.2 GPIOx_OTYPER, p.157), so no OTYPER write is needed. */
    EE_SPI_GPIO->MODER &= ~MODER2_MASK(EE_PIN_CS);     /* clear MODER12 -> 00 */
    EE_SPI_GPIO->MODER |=  MODER2(EE_PIN_CS, 1u);      /* set   MODER12 -> 01 = output */

    /* EXPLANATION (order): ODR resets to 0 (RM0091 s8.4.6, p.159). If PB12
     * were made an output first, it would drive LOW for the moment between
     * the two writes, briefly selecting the EEPROM. Setting ODR12 while the
     * pin is still an input means it comes up already high - no glitch.
     *
     * EXPLANATION (why CS starts high): CS# is active low. While it is low the
     * EEPROM treats every SCK edge as part of a command. Holding it high keeps
     * the EEPROM deselected, so the SPI pins can be configured, and the Task 3
     * test byte sent, without the EEPROM reacting. A transaction only starts
     * when our driver deliberately pulls CS low. */

    /* TODO 2.6  SCK, MISO and MOSI: put them in alternate-function mode
     *           (MODER), and select the alternate function number EE_SPI_AF
     *           (the AF register for pins 8..15). Both registers are needed. */

    /* MODER = 10 "Alternate function mode" (RM0091 s8.4.1, p.157), and
     * AFSELy = EE_SPI_AF in AFRH (RM0091 s8.4.10 GPIOx_AFRH, p.162:
     * "0000: AF0"). AFRH is AFR[1] in the CMSIS GPIO_TypeDef.
     * The pins are consecutive (PB13 SCK, PB14 MISO, PB15 MOSI), so loop. */
    {
        uint32_t moder = EE_SPI_GPIO->MODER;     /* take a copy of the register */
        uint32_t afrh  = EE_SPI_GPIO->AFR[1];

        /* PB13 - SCK */
        moder &= ~MODER2_MASK(EE_PIN_SCK);   moder |= MODER2(EE_PIN_SCK, 2u);        /* bits 27:26 = 10 */
        afrh  &= ~AFRH4_MASK(EE_PIN_SCK);    afrh  |= AFRH4(EE_PIN_SCK, EE_SPI_AF);  /* bits 23:20 = 0000 */
        /* PB14 - MISO */
        moder &= ~MODER2_MASK(EE_PIN_MISO);  moder |= MODER2(EE_PIN_MISO, 2u);       /* bits 29:28 = 10 */
        afrh  &= ~AFRH4_MASK(EE_PIN_MISO);   afrh  |= AFRH4(EE_PIN_MISO, EE_SPI_AF); /* bits 27:24 = 0000 */
        /* PB15 - MOSI */
        moder &= ~MODER2_MASK(EE_PIN_MOSI);  moder |= MODER2(EE_PIN_MOSI, 2u);       /* bits 31:30 = 10 */
        afrh  &= ~AFRH4_MASK(EE_PIN_MOSI);   afrh  |= AFRH4(EE_PIN_MOSI, EE_SPI_AF); /* bits 31:28 = 0000 */

        EE_SPI_GPIO->AFR[1] = afrh;              /* write each register back once */
        EE_SPI_GPIO->MODER  = moder;
     }
    /* EXPLANATION: MODER only says "this pin belongs to a peripheral"; AFRH
     * says WHICH peripheral (Datasheet Table 15, p.38: AF0 = SPI2 on
     * PB13-15). AF0 is also the reset value, so the AFRH write changes nothing
     * here - it is kept so the configuration does not rely on reset state and
     * can be shown in the debugger. AFRH is written before MODER so that the
     * pins never connect to a different AF for an instant. */

    /* TODO 2.7  Recommended: high output speed on SCK and MOSI, and a pull-up
     *           on MISO. In your report, explain what the EEPROM does with its
     *           output pin while CS is high, and why a pull-up helps. */

    /* OSPEEDRy = 11 "High speed" on the two outputs we drive
     * (RM0091 s8.4.3 GPIOx_OSPEEDR, p.158). Same 2-bit layout as MODER, so the
     * MODER2 helpers give the bit positions. */
    EE_SPI_GPIO->OSPEEDR &= ~MODER2_MASK(EE_PIN_SCK);   /* clear OSPEEDR13 -> 00              */
    EE_SPI_GPIO->OSPEEDR |=  MODER2(EE_PIN_SCK, 3u);    /* set   OSPEEDR13 -> 11 = high speed */
    EE_SPI_GPIO->OSPEEDR &= ~MODER2_MASK(EE_PIN_MOSI);  /* clear OSPEEDR15 -> 00              */
    EE_SPI_GPIO->OSPEEDR |=  MODER2(EE_PIN_MOSI, 3u);   /* set   OSPEEDR15 -> 11 = high speed */

    /* PUPDR14 = 01 "Pull-up" on MISO (RM0091 s8.4.4 GPIOx_PUPDR, p.158). */
    EE_SPI_GPIO->PUPDR &= ~MODER2_MASK(EE_PIN_MISO);    /* clear PUPDR14 -> 00           */
    EE_SPI_GPIO->PUPDR |=  MODER2(EE_PIN_MISO, 1u);     /* set   PUPDR14 -> 01 = pull-up */

    /* EXPLANATION (report): while CS is high the EEPROM puts its SO pin in
     * high impedance - it neither drives high nor low, so the bus can be
     * shared. EEPROM datasheet (onsemi CAT25010/D Rev 25), "Pin Description",
     * p.5: "When CS is high, the SO output is tri-stated (high impedance) and
     * the device is in Standby Mode"; Figure 2 "Synchronous Data Timing", p.5,
     * shows SO as HI-Z outside the CS-low window. Without a pull-up, MISO would float and
     * the STM32 would read noise; the pull-up gives a defined idle level of 1,
     * which is why bytes read while the EEPROM is not driving come back 0xFF.
     * High speed on SCK/MOSI gives sharper edges; at 250 kHz it is not
     * critical, but it keeps edges clean if the clock is raised later. */

    /* TODO 2.8  SPI_CR2: 8-bit data frames, and a receive FIFO threshold that
     *           reports a received byte after 8 bits. Read the RM0091
     *           description of the FIFO threshold carefully - on the STM32F0
     *           the reset value does not suit single-byte transfers.
     *           Configure CR2 BEFORE enabling the peripheral. */

    /* RM0091 s28.9.2 SPIx_CR2, p.803-804 (reset value 0x0700). */
    EE_SPI->CR2 = (1UL << 12)   /* FRXTH = 1: RXNE at 1/4 FIFO (8 bit), (1UL << 12) same as SPI_CR2_FRXTH */
                | (7UL << 8);   /* DS[3:0] = 0111: 8-bit frames, (7UL << 8) same as SPI_CR2_DS_2|SPI_CR2_DS_1|SPI_CR2_DS_0 */
    /* = 0x1700. All other CR2 bits (interrupts, DMA, SSOE, FRF, NSSP) = 0.
     *
     * EXPLANATION (FRXTH): FRXTH resets to 0, which means "RXNE event is
     * generated if the FIFO level is greater than or equal to 1/2 (16-bit)"
     * (p.804). We transfer ONE 8-bit frame at a time, so the RX FIFO would
     * only ever hold 8 bits and RXNE would never set - spi_transfer() in Task 3
     * would wait for it forever. FRXTH = 1 raises RXNE after 8 bits. RM0091
     * s28.5.9, p.766 also says the read access must match this threshold,
     * which is why Task 3 reads DR 8 bits wide.
     * DS = 0111 is already the reset value (0x0700); it is written anyway so
     * the frame size is explicit.
     * Plain '=' (not |=) is deliberate: this code owns SPI2, and writing the
     * whole register guarantees no leftover bits. SPE is still 0, as required
     * for configuration (RM0091 s28.5.7 "Configuration of SPI", p.765). */

    /* TODO 2.9  SPI_CR1: master mode, your baud-rate divider, the clock
     *           polarity and phase the EEPROM supports (EEPROM datasheet), and
     *           the bit order.
     *           You are driving CS yourself on a GPIO. Read RM0091 on
     *           slave-select (NSS) management in master mode: if the
     *           peripheral believes its NSS input is low it will leave master
     *           mode on its own, and you will see no clock at all. */

    /* RM0091 s28.9.1 SPIx_CR1, p.801-802 (reset value 0x0000). SPE (bit 6) is
     * deliberately left 0 here; it is set in TODO 2.10. */
    EE_SPI->CR1 = (1UL << 9)             /* SSM = 1: software NSS, (1UL << 9) same as SPI_CR1_SSM */
                | (1UL << 8)             /* SSI = 1: internal NSS held high, (1UL << 8) same as SPI_CR1_SSI */
                | (0UL << 7)             /* LSBFIRST = 0: MSB first, (1UL << 7) would be SPI_CR1_LSBFIRST */
                | (EE_SPI_BR << 3)       /* BR[2:0] = 100: fPCLK/32 = 250 kHz, (EE_SPI_BR << 3) same as (EE_SPI_BR << SPI_CR1_BR_Pos) */
                | (1UL << 2)             /* MSTR = 1: master, (1UL << 2) same as SPI_CR1_MSTR */
                | (0UL << 1)             /* CPOL = 0: SCK idles low, (1UL << 1) would be SPI_CR1_CPOL */
                | (0UL << 0);            /* CPHA = 0: data captured on 1st edge, (1UL << 0) would be SPI_CR1_CPHA */
    /* = 0x0324 before enabling (0x0364 once SPE is set below).
     *
     * EXPLANATION (SSM/SSI): RM0091 s28.5.5 "Slave select (NSS) pin
     * management", p.762: with SSM = 1 "slave select information is driven
     * internally by the SSI bit value" and the NSS pin is free. We drive CS
     * ourselves on PB12 as a plain GPIO, so the peripheral's NSS input must
     * not see a low level: with hardware NSS and the pin low, "the SPI enters
     * master mode fault state and the device is automatically reconfigured in
     * slave mode" (MODF) - MSTR clears and no clock is produced. SSI = 1 ties
     * the internal NSS high, so the SPI stays master.
     *
     * EXPLANATION (CPOL/CPHA = mode 0, MSB first): RM0091 bit meanings, p.802:
     * CPOL 0 = "CK to 0 when idle"; CPHA 0 = "The first clock transition is
     * the first data capture edge". With CPOL = 0 the first transition is the
     * rising edge, so both devices sample on SCK rising edges and change data
     * on falling edges (RM0091 s28.5.6 "Communication formats", p.763).
     * Why mode 0 and MSB first - EEPROM datasheet (onsemi CAT25010/D Rev 25):
     *   - p.1 "Features": "SPI Modes (0,0) & (1,1)"; p.5 "Functional
     *     Description": "support the SPI bus protocol, modes (0,0) and (1,1)".
     *     (0,0) = (CPOL, CPHA) = (0, 0) is one of the two supported modes.
     *   - p.5 "Pin Description": "SI ... input data is latched on the rising
     *     edge of the SCK clock"; "SO ... data is shifted out on the falling
     *     edge". Mode 0 samples on the rising (first) edge - a match.
     *   - MSB first: Figure 3 "WREN Timing", p.6, shows the opcode as
     *     0000 0110 left to right, and Figure 5 "Byte WRITE Timing", p.7,
     *     sends D7 first and ends with D0 (address A7 ... A0 likewise).
     * Mode (1,1) would also work with this EEPROM; the other two modes
     * (CPOL != CPHA) are not supported.
     * Note: the fitted part is addressed differently from the CAT25040 (README
     * s5.2: two address bytes, 8192 bytes), but the handout supplies this
     * datasheet for the SPI mode and bit order.
     * Order note: RM0091 s28.5.7 (p.765) lists CR1 before CR2; the order does
     * not matter as long as both are written while SPE = 0. */

    /* Task 5 fault case. Leave this call exactly here: after your CR1 and CR2
     * configuration, before the peripheral is enabled. It does nothing unless
     * RUN_TASK is 5. */
    task5_fault_hook();

    /* TODO 2.10  Enable the peripheral. */

    /* SPE = 1 (RM0091 s28.9.1 SPIx_CR1, bit 6, p.802). Read-modify-write so
     * every CR1 bit already configured is kept.
     * (1UL << 6) same as SPI_CR1_SPE */
    EE_SPI->CR1 |= (1UL << 6);
    /* EXPLANATION: nothing is transmitted yet. In full-duplex master mode the
     * SPI "starts to communicate when the SPI is enabled and TXFIFO is not
     * empty, or with the next write to TXFIFO" (RM0091 s28.5.8, p.766). The TX
     * FIFO is empty, so SCK stays at its idle level until Task 3 writes DR.
     *
     * Expected debugger values with RUN_TASK 2 (compute these yourself first):
     *   dbg_gpiob_moder  = 0xA9000000  (PB15..PB12 = 10 10 10 01)
     *   dbg_gpiob_afrh   = 0x00000000  (AF0 on PB13-15)
     *   dbg_spi_cr2      = 0x00001700  (FRXTH, DS = 0111)
     *   dbg_spi_cr1      = 0x00000364  (0x0324 + SPE)
     *   dbg_spi_sr       : TXE (bit 1) = 1, BSY (bit 7) = 0, MODF (bit 5) = 0
     *   (RM0091 s28.9.3 SPIx_SR, p.806) */

    dbg_gpiob_moder      = EE_SPI_GPIO->MODER;
    dbg_gpiob_afrh       = EE_SPI_GPIO->AFR[1];
    dbg_spi_cr1          = EE_SPI->CR1;
    dbg_spi_cr2          = EE_SPI->CR2;
    dbg_spi_sr           = EE_SPI->SR;
    dbg_sck_hz_predicted = EE_SCK_HZ_PREDICTED;
}

/* ==========================================================================
 * RUN_TASK 2 - given
 * Configures SPI and then does nothing else, so you can inspect the selected
 * SPI peripheral and GPIOB in the SFR view (or the dbg_* variables) while the
 * program runs. PC13 keeps toggling so you can see the program has not hung.
 * ========================================================================== */

void task2_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
}

void task2_loop(uint32_t now)
{
    task1_gpio_update(now);
}
