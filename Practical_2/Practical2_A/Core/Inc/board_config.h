/**
  ******************************************************************************
  * @file    board_config.h
  * @brief   Board facts, and the values you must look up.
  *
  * TODO 0 - START HERE.
  *
  * Values marked TODO have to be FOUND in the document named next to them.
  * The handout is explicit: "Where a task asks for a register field,
  * peripheral address, alternate function, command value, timing requirement,
  * or status bit, do not guess it."
  *
  * The project compiles with the placeholders, but the board will not work
  * until they are correct. That is deliberate.
  *
  * Two things below are GIVEN rather than left as TODOs: the SPI pin mapping
  * and the EEPROM address format. The handout is wrong about both on this
  * board, so you could not find the right answer in your documents. README
  * section 5 explains how to confirm them on the bench - do that.
  ******************************************************************************
  */

#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "stm32f0xx.h"

/* ==========================================================================
 * 1. CLOCK
 * ========================================================================== */

/* TODO 2.1  The kernel clock frequency of the SPI peripheral in THIS build.
 *           Read SystemClock_Config() in main.c, then follow the clock tree in
 *           the RCC chapter of RM0091 to the bus the SPI peripheral sits on.
 *           Do not assume it. */

/* Derivation (references are to RM0091 Rev 9, DocID018940, Jan 2017):
 *  1. SystemClock_Config() (main.c) selects HSI as SYSCLK, PLL off.
 *  2. HSI = 8 MHz internal RC oscillator
 *       - RM0091 s6.2   "Clocks", p.95 (first bullet)
 *       - RM0091 s6.2.2 "HSI clock", p.100 (factory-calibrated to 1% at 25 C)
 *  3. AHBCLKDivider = RCC_SYSCLK_DIV1 -> HPRE = 0xxx, "SYSCLK not divided"
 *       - RM0091 s6.4.2 RCC_CFGR, bits 7:4 HPRE, p.112   => HCLK = 8 MHz
 *  4. APB1CLKDivider = RCC_HCLK_DIV1  -> PPRE = 0xx, "HCLK not divided"
 *       - RM0091 s6.4.2 RCC_CFGR, bits 10:8 PPRE, p.111  => PCLK = 8 MHz
 *  5. SPI2 is an APB peripheral, so its kernel clock is PCLK
 *       - RM0091 s2.2.2 Table 1, p.48: SPI2 at 0x4000 3800 is on the APB bus
 *       - RM0091 s6.2, p.96: "All the peripheral clocks are derived from their
 *         bus clock (HCLK for AHB or PCLK for APB)" (SPI is not an exception)
 *       - RM0091 s6.2, Figure 10 "Clock tree (STM32F03x and STM32F05x)", p.97
 *  Note: the F0 has a single APB bus. Its clock is called PCLK in RM0091; the
 *  CMSIS/HAL names (PCLK1, APB1ENR, RCC_HCLK_DIV1) are the same thing. */
#define PCLK1_HZ                8000000UL   /* HSI 8 MHz / AHB 1 / APB 1    */

/* ==========================================================================
 * 2. SPI PINS - GIVEN (see README section 5, and verify by continuity)
 * ========================================================================== */
#define EE_SPI_GPIO             GPIOB
#define EE_PIN_CS               12u         /* EEPROM pin 1 (CS#)           */
#define EE_PIN_SCK              13u         /* EEPROM pin 6 (SCK)           */
#define EE_PIN_MISO             14u         /* EEPROM pin 2 (SO)            */
#define EE_PIN_MOSI             15u         /* EEPROM pin 5 (SI)            */
#define EE_CS_MASK              (1UL << EE_PIN_CS)

/* TODO 2.2  Which SPI peripheral do these pins belong to, and which
 *           alternate-function NUMBER connects it to them? Use the STM32F051
 *           datasheet's alternate-function table for port B, and name that
 *           table in your report. */
/* Peripheral and AF number: STM32F051x4/x6/x8 DATASHEET (DocID022265 Rev 7,
 *   Jan 2017 - not RM0091), Table 15 "Alternate functions selected through
 *   GPIOB_AFR registers for port B", p.38. Column AF0 lists
 *   PB12 = SPI2_NSS, PB13 = SPI2_SCK, PB14 = SPI2_MISO, PB15 = SPI2_MOSI.
 *   So the peripheral is SPI2 and the AF number is 0.
 *   (Table 13 "Pin definitions", p.34, lists the same SPI2 functions for
 *   PB12-PB15 but without AF numbers.)
 *   (RM0091 does not contain the pin-to-AF mapping; it only describes the
 *   AFRH register that selects it: RM0091 s8.4.10 GPIOx_AFRH.)
 * Base address of SPI2: RM0091 s2.2.2 Table 1, p.48:
 *   0x4000 3800 - 0x4000 3BFF, SPI2, APB bus (= SPI2_BASE in stm32f051x8.h). */
#define EE_SPI                  ((SPI_TypeDef *)0x40003800UL) /* SPI2       */
#define EE_SPI_AF               0u          /* AF0 on PB13/PB14/PB15        */

/* ==========================================================================
 * 3. SPI BAUD RATE
 * ========================================================================== */

/* TODO 2.3  Choose BR[2:0] for an SCK of approximately 250 kHz from
 *           PCLK1_HZ, using the BR field description of SPI_CR1 in RM0091.
 *           Show the arithmetic in your report. Do not tune it until the
 *           waveform "looks right". */
/* RM0091 s28.9.1 SPIx_CR1, bits 5:3 BR[2:0] "Baud rate control", p.802:
 *   000 fPCLK/2, 001 /4, 010 /8, 011 /16, 100 /32, 101 /64, 110 /128, 111 /256
 *   i.e. f_SCK = fPCLK / 2^(BR+1), which is the shift in EE_SCK_HZ_PREDICTED.
 * Required divider = 8 000 000 Hz / 250 000 Hz = 32 = 2^5
 *   => BR + 1 = 5  => BR = 4 = 0b100  => f_SCK = 8 MHz / 32 = 250 kHz exactly.
 * Checked at build time by the TODO 2.11 _Static_assert in main.c. */
#define EE_SPI_BR               4UL         /* 0b100: fPCLK/32 = 250 kHz    */
#define EE_SCK_HZ_PREDICTED     (PCLK1_HZ >> (EE_SPI_BR + 1U))

/* ==========================================================================
 * 4. EEPROM
 * ========================================================================== */

/* GIVEN (see README section 5): the fitted part is addressed differently
 * from the CAT25040 named in the handout. Confirm both values on the bench. */
#define EEPROM_ADDR_BYTES       2u          /* address bytes, MSB first     */
#define EEPROM_SIZE_BYTES       8192u

/* TODO 4.1  Instruction opcodes, from the EEPROM datasheet's instruction set
 *           table. */
#define EEPROM_CMD_WREN         0x6u       /* <- TODO */
#define EEPROM_CMD_WRDI         0x4u       /* <- TODO */
#define EEPROM_CMD_RDSR         0x5u       /* <- TODO */
#define EEPROM_CMD_WRSR         0x1u       /* <- TODO */
#define EEPROM_CMD_READ         0x3u       /* <- TODO */
#define EEPROM_CMD_WRITE        0x2u       /* <- TODO */

/* TODO 4.2  Status register bit MASKS, from the datasheet's status register
 *           table. Which bit says a write is in progress - and is it 1 or 0
 *           while the device is busy? Which bit is the write enable latch? */
#define EEPROM_SR_RDY           0x01u       /* <- TODO: busy bit mask       */
#define EEPROM_SR_WEL           0x02u       /* <- TODO                      */

/* Upper bound on waiting for a write. A hang guard only: completion must be
 * decided from the status register, never from elapsed time. */
#define EEPROM_WRITE_TIMEOUT_MS 50u

/* ==========================================================================
 * 5. BOARD I/O - GIVEN (UCT board: Board.md)
 * ========================================================================== */
#define LED_BYTE_GPIO           GPIOB
#define LED_BYTE_MASK           0x00FFu     /* PB0..PB7                     */
#define LED_RED_PIN             10u         /* PB10                         */
#define LED_GREEN_PIN           11u         /* PB11                         */

#define BTN_GPIO                GPIOA
#define BTN_START_PIN           0u          /* PA0 / SW0, active low        */
#define BTN_ABORT_PIN           3u          /* PA3 / SW3, active low        */

#define SCOPE_GPIO              GPIOC
#define SCOPE_PIN               13u         /* PC13, header P1              */

/* ==========================================================================
 * 6. GROUP VALUES
 * ========================================================================== */

/* TODO 3.1  The last three decimal digits of each student number.
 *           Write them WITHOUT leading zeros: in C, 010 is octal, i.e. 8.
 *           Then work out B and A by hand for your report, and check them
 *           against the build (TODO 3.2 in main.c). */
#define STUDENT_N1              14u          /* <- TODO */
#define STUDENT_N2              9u          /* <- TODO */

/* The formulas from the handout. */
#define TEST_BYTE_B_RAW         ((((STUDENT_N1 ^ STUDENT_N2) + 0x3Du)) % 256u)
#define TEST_BYTE_B             (((TEST_BYTE_B_RAW == 0x00u) ||   \
                                  (TEST_BYTE_B_RAW == 0xFFu))     \
                                 ? (TEST_BYTE_B_RAW ^ 0x5Au)      \
                                 :  TEST_BYTE_B_RAW)
#define EEPROM_ADDR_A           ((STUDENT_N1 + 3u * STUDENT_N2) % 256u)

/* ==========================================================================
 * Helpers for fields that are 2 bits per pin (MODER, OSPEEDR, PUPDR) and
 * 4 bits per pin (AFRH, pins 8..15). They only do the bit positions - you
 * still have to know what value goes in each field.
 * ========================================================================== */
#define MODER2(pin, val)        ((uint32_t)(val) << ((pin) * 2u))
#define MODER2_MASK(pin)        (3UL << ((pin) * 2u))
#define AFRH4(pin, af)          ((uint32_t)(af) << (((pin) - 8u) * 4u))
#define AFRH4_MASK(pin)         (0xFUL << (((pin) - 8u) * 4u))

#endif /* __BOARD_CONFIG_H */
