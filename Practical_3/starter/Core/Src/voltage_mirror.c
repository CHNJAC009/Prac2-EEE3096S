/* EEE3096S Practical 3: complete the four TODOs in this file.
 * See ../../../docs/prac_03.md and ST RM0091, ADC and DAC chapters.
 * Use CMSIS register names; clock, GPIO and startup helpers are supplied.
 *
 * How this file is used (supplied main.c / sampling.c):
 *   main()  -> voltage_mirror_init() once, then sampling_start() starts TIM2.
 *   TIM2 interrupt every 0.5 ms (2 kHz, sampling.c):
 *       voltage_mirror_read(&C)  ->  D = voltage_mirror_map(C, coarse)
 *       ->  voltage_mirror_write(D)
 *   main() shows the latest C and D on the LCD every 200 ms; SW0 flips coarse.
 *   If voltage_mirror_read() returns false, sampling.c stops the timer and the
 *   LCD shows "ADC read failed".
 *
 * Both converters use V_DDA as their reference (RM0091 §14.1 p.268: "VDDA
 * (shared with ADC)"), so ideally V_in = C x V_DDA / 4096 and
 * V_out = D x V_DDA / 4096 (RM0091 §14.5.3 p.271). D = C therefore gives
 * nominally V_out = V_in.
 *
 * References: RM0091 Rev 9 (DocID018940), chapter 13 ADC and chapter 14 DAC;
 * STM32F051 datasheet DocID022265 Rev 7, Tables 52-55 (pp.76-81).
 * Page numbers are the printed page numbers in those PDFs.
 */
#include "voltage_mirror.h"
#include "board_io.h"
#include "main.h"
#include <stddef.h>

/* Bounded wait for one conversion (TODO 2).
 * One conversion takes t_CONV = sampling time + 12.5 ADC clock cycles
 * (RM0091 §13.4.6, p.235) = 239.5 + 12.5 = 252 cycles. The helper selects the
 * dedicated 14 MHz ADC clock (CKMODE = 00, RM0091 §13.12.5 p.263), so
 * t_CONV = 252 / 14 MHz = 18 us = 864 CPU cycles at 48 MHz (the datasheet
 * agrees: t_CONV max 18 us, Table 52 p.77).
 * Each poll of the loop below takes several CPU cycles (estimate, not from a
 * document), so 1000 polls gives well over 18 us, yet stays far below the
 * 500 us between timer samples (2 kHz, sampling.c). The point of the bound:
 * if the ADC ever stops responding, the interrupt gives up instead of hanging
 * the whole program in an endless while loop. */
#define ADC_EOC_WAIT_POLLS 1000u

bool voltage_mirror_init(void)
{
    /* Supplied helper: clocks GPIOA/ADC/DAC, sets PA4/PA5 analog, resets both
     * converters, starts HSI14 and calibrates the ADC. It leaves ADEN = 0, which
     * is when the ADC configuration below must be written: RES may only be
     * changed while ADEN = 0 (RM0091 §13.12.4, p.261) and the other fields
     * only while ADSTART = 0 (same register notes, p.260-261). */
    if (!board_converters_prepare()) return false;

    /* ---------------- ADC1 (RM0091 §13.12.4 ADC_CFGR1, p.259-262) ---------------- */
    /* The helper has just reset the ADC, so CFGR1 is already 0x0000 0000. The
     * four clears below still write each choice down explicitly, so the setup
     * is correct whatever state the register was in and every field we rely on
     * is visible here. Fields not touched keep their reset value 0, e.g.
     * OVRMOD = 0 (keep old data on overrun, p.260), DISCEN = 0, AUTOFF = 0. */
    ADC1->CFGR1 &= ~(3UL << 3);    /* RES[1:0] bits 4:3 -> 00 = 12-bit resolution (p.261), (3UL << 3) same as ADC_CFGR1_RES mask */
    ADC1->CFGR1 &= ~(1UL << 5);    /* ALIGN bit 5 -> 0 = right aligned, result in DR[11:0] (p.261, Figure 37 p.244), (1UL << 5) same as ADC_CFGR1_ALIGN mask */
    ADC1->CFGR1 &= ~(3UL << 10);   /* EXTEN[1:0] bits 11:10 -> 00 = hardware trigger off, conversions started by software ADSTART (p.260), (3UL << 10) same as ADC_CFGR1_EXTEN mask */
    ADC1->CFGR1 &= ~(1UL << 13);   /* CONT bit 13 -> 0 = single conversion mode: one conversion per ADSTART (p.260, §13.4.7 p.236), (1UL << 13) same as ADC_CFGR1_CONT mask */

    /* Channel selection (RM0091 §13.12.8 ADC_CHSELR, p.265). Whole-register
     * write on purpose: channel 5 is the ONLY channel in the sequence, so a
     * sequence of length 1 = convert just PA5 (§13.4.5 p.235, note §13.4.7 p.236). */
    ADC1->CHSELR = (1UL << 5);     /* CHSEL5 bit 5 = ADC_IN5 = PA5, (1UL << 5) same as ADC_CHSELR_CHSEL5 mask */

    /* Sampling time (RM0091 §13.12.6 ADC_SMPR, p.263-264). 239.5 cycles is
     * SMP = 111, i.e. all three bits set, so no clear is needed first.
     * Why the longest setting: during sampling the input charges the ADC's
     * internal sample-and-hold capacitor (C_ADC 8 pF, datasheet Table 52 p.76;
     * RM0091 §13.4.6 p.235). More time = the capacitor settles closer to the
     * real input voltage, even through the generator's and board's resistance.
     * 239.5 / 14 MHz = 17.1 us sampling (datasheet Table 53 p.78), and the
     * whole 18 us conversion still fits easily in the 500 us sample period. */
    ADC1->SMPR |= (7UL << 0);      /* SMP[2:0] bits 2:0 -> 111 = 239.5 ADC clock cycles (p.264), (7UL << 0) same as ADC_SMPR_SMP mask */

    /* ---------------- DAC channel 1 (RM0091 chapter 14) ---------------- */
    /* Initial command first: with TEN1 = 0 the value moves DHR -> DOR one APB
     * cycle after the write (§14.5.2, p.270) and the digital interface works
     * even before EN1 is set (§14.4, p.270), so PA4 starts at mid-scale. */
    DAC->DHR12R1 = 2048UL;         /* DACC1DHR[11:0] = 2048 = mid-scale ~ V_R/2 (§14.10.3 p.285, output = V_DDA x DOR / 4096 §14.5.3 p.271) */

    /* Buffer ON (BOFF1 = 0): the internal amplifier gives PA4 a low output
     * impedance, so the DMM/scope probe does not pull the voltage down. Cost:
     * the buffered output only swings from 0.2 V to V_DDA - 0.2 V (datasheet
     * Table 55 p.80), which is why the practical keeps inputs away from the
     * rails. Buffer OFF would reach the rails but has ~15 kOhm output
     * impedance (same table).
     * Trigger OFF (TEN1 = 0) = "software writes": every write to DHR12R1 is
     * the trigger itself. No TSEL1/SWTRIGR setup is needed (TSEL1 is only used
     * when TEN1 = 1, p.284). The 2 kHz timing comes from the TIM2 interrupt
     * that calls voltage_mirror_write(), not from a DAC hardware trigger. */

    DAC->CR &= ~(1UL << 1);        /* BOFF1 bit 1 -> 0 = output buffer ENABLED (low output impedance, §14.3 p.269; bit p.284), (1UL << 1) same as DAC_CR_BOFF1 mask */
    DAC->CR &= ~(1UL << 2);        /* TEN1 bit 2 -> 0 = trigger off: each DHR12R1 write reaches the output after one APB cycle (p.284), (1UL << 2) same as DAC_CR_TEN1 mask */
    DAC->CR |= (1UL << 0);         /* EN1 bit 0 -> 1 = DAC channel 1 on, PA4 connected to DAC_OUT1 (p.284, note p.269), (1UL << 0) same as DAC_CR_EN1 mask */

    /* Supplied helper: clears ADRDY, sets ADEN and waits (bounded) for ADRDY
     * (RM0091 §13.12.1 p.255, §13.12.3 p.258). Configuration is complete. */
    return board_adc_enable();
}

bool voltage_mirror_read(uint16_t *code)
{
    /* Reject a null pointer: there would be nowhere to store the result. */
    if (code == NULL) return false;

    /* Clear old flags first, so the EOC we wait for below can only come from
     * THIS conversion and never from an old, already-handled one.
     * ADC_ISR flags are rc_w1 = "write 1 to clear"
     * (RM0091 §13.12.1, p.254-255). Plain '=' on purpose: writing 0 to the
     * other bits has no effect, while '|=' would read the register and write
     * back (and so clear) every flag that happened to be set, e.g. ADRDY.
     * OVR (a new result arrived while EOC was still set, §13.6.2 p.244) cannot
     * really happen here: single mode does one conversion per ADSTART and we
     * read DR every time. It is cleared anyway so no stale flag is left. */
    ADC1->ISR = (1UL << 2)         /* EOC   bit 2 (p.255), (1UL << 2) same as ADC_ISR_EOC mask */
              | (1UL << 3)         /* EOSEQ bit 3 (p.254), (1UL << 3) same as ADC_ISR_EOSEQ (= ADC_ISR_EOS) mask */
              | (1UL << 4);        /* OVR   bit 4 (p.254), (1UL << 4) same as ADC_ISR_OVR mask */

    /* Start one conversion. EXTEN = 00, so it starts immediately (§13.4.9
     * p.236-237); CONT = 0, so hardware clears ADSTART when the sequence
     * (just channel 5) ends (p.237, bit description p.258). */
    ADC1->CR |= (1UL << 2);        /* ADSTART bit 2 -> 1 = start conversion (§13.12.3 p.258), (1UL << 2) same as ADC_CR_ADSTART mask */

    /* Bounded wait for EOC: set by hardware when a new result is in ADC_DR
     * (§13.5.3 p.241, bit p.255). Never waits forever. */
    for (uint32_t remaining = ADC_EOC_WAIT_POLLS; remaining != 0u; --remaining)
    {
        if ((ADC1->ISR & (1UL << 2)) != 0u)   /* EOC bit 2 set? (p.255), (1UL << 2) same as ADC_ISR_EOC mask */
        {
            /* Reading ADC_DR also clears EOC (§13.5.3 p.241). Right-aligned
             * 12-bit data is in DR[11:0] (Figure 37 p.244, DR p.265); the mask
             * guarantees the returned code is 0..4095. */
            *code = (uint16_t)(ADC1->DR & (0xFFFUL << 0));   /* DATA[11:0], (0xFFFUL << 0) = 12-bit result field */
            return true;
        }
    }

    /* Timeout: no result. Request ADSTP to abort and discard the conversion
     * so the ADC returns to idle (§13.4.11 p.238, bit p.257); *code is not
     * written, so no made-up value is ever returned. ADSTP only acts while
     * ADSTART = 1 (p.257), which is exactly the stuck case. Returning false
     * makes sampling.c stop TIM2 and show "ADC read failed", so no new
     * ADSTART is issued afterwards; the DAC keeps its last good value. */
    ADC1->CR |= (1UL << 4);        /* ADSTP bit 4 -> 1 = stop ongoing conversion (§13.12.3 p.257), (1UL << 4) same as ADC_CR_ADSTP mask */
    return false;
}

void voltage_mirror_write(uint16_t code)
{
    /* Saturate to the 12-bit DAC range: only bits 11:0 of DAC_DHR12R1 are
     * data (§14.10.3, p.285). 4096 and above become 4095 instead of wrapping.
     * Without this, 4096 (0x1000) would lose bit 12 and output 0 V: a
     * full-scale input would suddenly give the lowest output. */
    if (code > MIRROR_MAX_CODE) code = MIRROR_MAX_CODE;

    /* 12-bit right-aligned write. TEN1 = 0, so it is moved to DOR and the
     * output one APB cycle later (§14.5.2 p.270); PA4 = V_DDA x D / 4096
     * (§14.5.3 p.271). */
    DAC->DHR12R1 = code;           /* DACC1DHR[11:0] = D (p.285) */
}

uint16_t voltage_mirror_map(uint16_t code, bool coarse)
{
    /* Same saturation as the DAC write, so both modes see a valid 0..4095 code. */
    if (code > MIRROR_MAX_CODE) code = MIRROR_MAX_CODE;

    /* FULL12: D = C, every one of the 4096 codes passes straight through. */
    if (!coarse) return code;

    /* COARSE6: keep bits 11:6 (the six most significant bits) and clear bits
     * 5:0. This equals D = 64 x floor(C / 64), so D is still on the 12-bit DAC
     * scale: 64 levels 0, 64, ..., 4032 (4095 & 0xFC0 = 4032). The hardware
     * stays 12-bit; only software discards the six LSBs (prac_03.md).
     * Same result as (code >> 6) << 6 or (code / 64) * 64. Clearing bits
     * rounds DOWN, so D <= C and the output sits 0 to 63 LSB (0 to ~51 mV)
     * below the input. One coarse step = 64 LSB = 64 x V_DDA / 4096 = V_DDA / 64. */
    return (uint16_t)(code & (0x3FUL << 6));   /* (0x3FUL << 6) = 0x0FC0 = bits 11:6 */
}
