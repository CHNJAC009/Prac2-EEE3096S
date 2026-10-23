# Practical 3 — Demo Cheat Sheet (Voltage Mirror: ADC → DAC)

The demo is worth **80 of the 100 marks** and takes 5–8 minutes. The tutor
picks one safe DC input, then watches the triangle. You must be able to explain
every register field in `voltage_mirror.c`.

This sheet is in the order the tutor marks:

0. Before the demo: checklist and the numbers to know by heart.
1. The big picture: what happens every 0.5 ms.
2. How our code works, line by line, with where every value comes from.
3. The supplied code (in case they ask how it fits together).
4. The demo itself: the four marked parts, with what to do and what to say.
5. Theory questions with model answers.
6. Curveballs ("what if you changed…").
7. Bench troubleshooting.

**Documents** (in the repo root):
- **RM0091**: the STM32F0 reference manual. ADC = chapter 13, DAC = chapter 14.
- **Datasheet**: `DS_stm32f051c4.pdf` (DocID022265 Rev 7). Pins = Table 13; ADC/DAC electrical specs = Tables 52–55.
- **Handout**: `Practical_3/docs/prac_03.md`.

Page numbers are the printed page numbers.

---

## 0. Before the demo

### Checklist

- [ ] Board, USB cable, laptop with the project built and **flashed**.
- [ ] **P2 loopback jumpers removed.**
- [ ] Generator output set to **High-Z** (not 50 Ω). In 50 Ω mode the
      generator outputs **double** the voltage it shows into our high-impedance pin.
- [ ] Scope probes on **10×** AND the scope channel menu on **10×**. Both
      channels **DC coupled**.
- [ ] All grounds (generator, scope clips, DMM COM) on **board GND**.
- [ ] Generator limits checked **on the scope before** connecting it to PA5.
- [ ] The three DC levels (≈0.8, 1.65, 2.5 V) already measured and in the report table.
- [ ] Know the scope's cursor menu (voltage cursors, ΔV readout).
- [ ] LCD blank? Turn **POT2** (contrast). Garbage? Hold **SW3** during reset.

### Where things are

| Signal | Pin | Where on the board |
|---|---|---|
| Input (generator + scope CH1) | **PA5** = ADC_IN5 | Expansion header **P1** |
| Output (DMM / scope CH2) | **PA4** = DAC_OUT1 | **HAT connector pin 1** (labelled DAC1) |
| Reference V_R | VDDA (MCU pin 9) | Tied to the board's **3V3** rail |
| Ground | GND | Any GND pin |
| Mode switch | SW0 = PA0 | Switches block |

Pins are from datasheet Table 13, p.32 (PA4 = ADC_IN4/DAC_OUT1, PA5 = ADC_IN5).

### Numbers to know by heart (our V_R = 3.2860 V)

| Quantity | Formula | Value |
|---|---|---|
| 1 LSB (ADC and DAC) | V_R / 4096 | **0.8022 mV** |
| Coarse step | 64 × V_R / 4096 = V_R / 64 | **51.34 mV** |
| Number of coarse levels | 4096 / 64 | **64** (0, 64, 128, …, 4032) |
| Highest coarse command | 63 × 64 = `0xFC0` | **4032** → 3.2347 V nominal |
| Sample rate | 48 MHz / (47+1) / (499+1) | **2 kHz** (every **0.5 ms**) |
| One ADC conversion | (239.5 + 12.5) / 14 MHz | **18 µs** |
| Buffered DAC output range | 0.2 V … V_DDA − 0.2 V | **0.2 … 3.086 V** (datasheet Table 55, p.80) |

### Expected codes (to spot problems fast)

| Input | C ≈ V_in × 4096 / V_R | FULL12: D = C → predicted PA4 | COARSE6: D = 64⌊C/64⌋ → predicted PA4 |
|---|---|---|---|
| 0.800 V | 997 | 997 → 0.7998 V | **960** → 0.7702 V |
| 1.650 V | 2057 | 2057 → 1.6502 V | **2048** → 1.6430 V |
| 2.500 V | 3116 | 3116 → 2.4998 V | **3072** → 2.4645 V |

Real codes will differ by a few LSB and flicker ±1–3. That's noise, not a bug.

---

## 1. The big picture

```
 generator ──► PA5 ──► ADC (12-bit) ──► C ──► map() ──► D ──► DAC (12-bit) ──► PA4 ──► DMM / scope
                         ▲                     ▲                    ▲
                    V_DDA = V_R          SW0: FULL12 / COARSE6   V_DDA = V_R
```

Every **0.5 ms** the TIM2 interrupt (supplied `sampling.c`) runs our three functions:

1. `voltage_mirror_read(&C)`: start one conversion of PA5, wait for it (bounded) and return the code C (0–4095).
2. `D = voltage_mirror_map(C, coarse)`: FULL12 gives `D = C`. COARSE6 gives `D = C & 0xFC0`.
3. `voltage_mirror_write(D)`: write D to the DAC. PA4 becomes ≈ D × V_R / 4096.

Separately, `main()` shows the latest C and D on the LCD every 200 ms and
toggles the mode when SW0 is pressed.

**Why the output equals the input.** Both converters use the **same reference
V_DDA** (RM0091 §14.1 p.268: "VDDA (shared with ADC)"):

- ADC: C ≈ V_in × 4096 / V_R
- DAC: V_out = D × V_R / 4096 (RM0091 §14.5.3 p.271)
- With D = C: V_out = (V_in × 4096 / V_R) × V_R / 4096 = **V_in**

---

## 2. Our code, line by line (`starter/Core/Src/voltage_mirror.c`)

### TODO 1: `voltage_mirror_init()`

**Order matters.**

1. The supplied `board_converters_prepare()`:
   - clocks GPIOA, the ADC and the DAC
   - puts PA4/PA5 in analog mode
   - resets both converters
   - starts the 14 MHz HSI14 ADC clock
   - **calibrates** the ADC
   - leaves the ADC **disabled (ADEN = 0)**
2. **We configure the ADC while ADEN = 0.** RES may *only* be written when ADEN = 0 (RM0091 p.261). The other fields only need ADSTART = 0, which is also true while disabled.
3. We set up the DAC.
4. The supplied `board_adc_enable()` sets ADEN and waits (bounded) for ADRDY.

**ADC lines**

| Line | Field | Value | Meaning | Source |
|---|---|---|---|---|
| `ADC1->CFGR1 &= ~(3UL << 3);` | RES[1:0], bits 4:3 | 00 | **12-bit** resolution | RM0091 §13.12.4 p.261 |
| `ADC1->CFGR1 &= ~(1UL << 5);` | ALIGN, bit 5 | 0 | **Right-aligned**: result in DR[11:0] | p.261, Fig. 37 p.244 |
| `ADC1->CFGR1 &= ~(3UL << 10);` | EXTEN[1:0], bits 11:10 | 00 | **No hardware trigger**: software starts each conversion with ADSTART | p.260 |
| `ADC1->CFGR1 &= ~(1UL << 13);` | CONT, bit 13 | 0 | **Single** conversion: one result per ADSTART | p.260, §13.4.7 p.236 |
| `ADC1->CHSELR = (1UL << 5);` | CHSEL5 | 1 | Convert **channel 5 = PA5** only. A plain `=` makes it the *only* channel (sequence of length 1). | §13.12.8 p.265, §13.4.7 p.236 |
| `ADC1->SMPR \|= (7UL << 0);` | SMP[2:0] | 111 | **239.5 ADC-clock sampling time** | §13.12.6 p.264 |

**Why clear bits that are already 0?** The helper just reset the ADC, so
CFGR1 is already 0. Writing each choice explicitly documents it and makes the
setup correct regardless of earlier state.

**Why 239.5 cycles (the longest)?**
- During sampling, the input charges the ADC's internal 8 pF sample-and-hold capacitor (datasheet Table 52 p.76; RM0091 §13.4.6 p.235).
- A longer time lets it settle closer to the true voltage, even through the source resistance.
- 239.5 / 14 MHz = 17.1 µs of sampling (datasheet Table 53 p.78). Total conversion is (239.5 + 12.5) / 14 MHz = **18 µs**, tiny compared with the 500 µs between samples.

**DAC lines**

| Line | Field | Value | Meaning | Source |
|---|---|---|---|---|
| `DAC->DHR12R1 = 2048UL;` | DACC1DHR[11:0] | 2048 | Start at mid-scale ≈ V_R/2 | §14.10.3 p.285 |
| `DAC->CR &= ~(1UL << 1);` | BOFF1, bit 1 | 0 | **Output buffer ON** | §14.3 p.269, bit p.284 |
| `DAC->CR &= ~(1UL << 2);` | TEN1, bit 2 | 0 | **No trigger**: each DHR12R1 write reaches the output 1 APB clock later ("software writes") | §14.5.2 p.270, bit p.284 |
| `DAC->CR \|= (1UL << 0);` | EN1, bit 0 | 1 | **Turn DAC channel 1 on.** PA4 connects to DAC_OUT1. | p.284, note p.269 |

- **Buffer ON:** low output impedance, so the DMM and scope don't load PA4. The cost is that it only swings 0.2 V to V_DDA − 0.2 V (datasheet Table 55 p.80). That's one reason the practical uses voltages away from the rails.
- **Trigger OFF:** the write itself is the update. The 2 kHz timing comes from the **TIM2 interrupt**, not from a DAC hardware trigger. TSEL1 is ignored when TEN1 = 0 (p.284).

### TODO 2: `voltage_mirror_read()`

1. **Null check:** if `code == NULL` there's nowhere to store the result, so return false.
2. **Clear old flags:** `ADC1->ISR = EOC | EOSEQ | OVR;` (bits 2, 3, 4).
   - ISR flags are **rc_w1 = write-1-to-clear** (RM0091 §13.12.1 p.254–255). Writing 1 clears a flag; writing 0 does nothing.
   - **Why plain `=` and not `|=`:** `|=` reads ISR and writes back every flag that is currently 1, so it would clear flags we didn't intend to clear (e.g. ADRDY).
   - Clearing first guarantees the EOC we wait for comes from **this** conversion.
3. **Start:** `ADC1->CR |= (1UL << 2);` sets ADSTART (p.258).
   - EXTEN = 00, so the conversion starts immediately (§13.4.9 p.237).
   - CONT = 0, so hardware clears ADSTART when it finishes.
4. **Bounded wait:** a `for` loop polls EOC (bit 2) at most 1000 times.
   - EOC = "new result in ADC_DR" (§13.5.3 p.241).
5. **Read:** `*code = ADC1->DR & 0xFFF;`
   - Right-aligned 12-bit data is in DR[11:0]. The mask guarantees 0–4095.
   - Reading DR also **clears EOC automatically** (§13.5.3 p.241).
6. **Timeout:** set ADSTP (bit 4, p.257) to abort the conversion and return false.
   - `*code` is **not written**, so no fake value is ever used.
   - `sampling.c` then stops TIM2 and the LCD shows `ADC read failed`.

**Why bounded?**
- `while (!(ADC1->ISR & EOC));` would hang **forever** inside the interrupt if the ADC ever stopped responding (no clock, not enabled, wrong configuration).
- With the bound we give up after ~1000 polls, report a fault and leave the DAC at its last good value.
- The normal wait is 18 µs = 864 CPU cycles at 48 MHz. Each poll costs several cycles, so 1000 polls is comfortably longer, yet far below the 500 µs sample period.

### TODO 3: `voltage_mirror_write()`

```c
if (code > 4095) code = 4095;   // saturate (clamp)
DAC->DHR12R1 = code;            // 12-bit right-aligned write
```

- DHR12R1 only uses bits 11:0 (§14.10.3 p.285).
- **Why saturate?** 4096 = `0x1000`. Bit 12 would be dropped, so it would output **0 V**: a full-scale input would suddenly give the lowest output. Clamping gives the nearest valid value, 4095.
- With TEN1 = 0 the value moves to DOR one APB clock later (§14.5.2 p.270). PA4 settles in a few µs (t_SETTLING 3–4 µs, datasheet Table 55 p.81).

### TODO 4: `voltage_mirror_map()`

```c
if (code > 4095) code = 4095;          // same clamp, so both modes see 0..4095
if (!coarse) return code;              // FULL12:  D = C
return code & (0x3FUL << 6);           // COARSE6: D = C & 0x0FC0
```

- `0x3F << 6 = 0x0FC0 = 0b1111_1100_0000`. This **keeps bits 11:6** (the top 6 bits) and **clears bits 5:0** (the bottom 6).
- Same as `64 × floor(C / 64)`, `(C >> 6) << 6` or `(C / 64) * 64`.
- D is **still on the 12-bit scale**: 0, 64, 128, …, 4032. The DAC still receives a 12-bit number.
- **Worked example:** C = 2057 = `0x809` = `1000 0000 1001`. Clearing the low 6 bits gives `1000 0000 0000` = 2048. 2057 − 2048 = 9 LSB lost ≈ 7.2 mV.
- **Why 4032 and not 4095:** 4095 = `0xFFF`, and `0xFFF & 0xFC0 = 0xFC0` = 4032 = 63 × 64. The next multiple of 64 would be 4096, which doesn't fit in 12 bits.
- **Clearing bits rounds DOWN,** so D ≤ C and the COARSE6 output sits **0 to 63 LSB (0 to ~51 mV) below** the input.

---

## 3. The supplied code (you didn't write it, but they may ask)

| File | What it does |
|---|---|
| `main.c` | Sets the clock (8 MHz crystal → PLL ×6 → **48 MHz**), calls `voltage_mirror_init()`, starts sampling. The loop polls buttons (SW0 toggles `coarse`) and updates the LCD every 200 ms. Shows `Check TODOs` if init fails. |
| `sampling.c` | Sets up **TIM2**. PSC = 47 → 48 MHz / 48 = 1 MHz tick. ARR = 499 → 500 ticks = **0.5 ms** → 2 kHz. The interrupt runs read → map → write and stores the latest C/D/mode. |
| `board_io.c` | LCD driver, 20 ms button debounce, converter clocks/GPIO, ADC calibration and enable. |

Questions they might ask about it:

- **Why sample in an interrupt and not in `main()`?** The timer gives an exact, steady 2 kHz rate. Writing to the LCD takes milliseconds, which would make the sample rate uneven if sampling were in the main loop.
- **Why does `sampling_snapshot()` disable interrupts briefly?** So the main loop copies C, D and the mode as one consistent set. Otherwise the ISR could update C between reading C and reading D, and the LCD could show a C and D from different samples.
- **Why are the shared variables `volatile`?** They are changed inside the interrupt. `volatile` forces the main loop to really re-read them from memory every time instead of reusing an old copy held in a register.
- **What is calibration?** The ADC measures and stores its own offset correction. It must run while ADEN = 0 (RM0091 §13.4.1 p.230). The datasheet accuracy figures assume it has been done (Table 54 note 1, p.78).
- **What clock does the ADC use?** HSI14, a dedicated 14 MHz oscillator (CKMODE = 00, RM0091 §13.12.5 p.263). 14 MHz is the ADC's maximum f_ADC (datasheet Table 52 p.76).
- **How does the code know the init failed?** `voltage_mirror_init()` returns false, so `main()` lights the red LED and shows `Check TODOs`.

---

## 4. The demo, part by part

### Part 1: Setup and switching (10 marks)

**Do:**
1. Point to PA5 (P1 header), PA4 (HAT pin 1) and GND.
2. Show the generator trace on the scope **before** connecting it to PA5: minimum and maximum both inside 0 … 3.286 V, comfortably away from the rails.
3. Connect, then press SW0 and show the LCD change between `FULL12` and `COARSE6`.

**Say:** "Generator in High-Z mode. I checked the actual limits on the scope
first, so PA5 never goes below 0 V or above V_R = 3.286 V. All grounds are
common at board GND."

**Why the input must stay inside 0 … V_DDA:** the ADC can only convert
0 to V_DDA (datasheet Table 52 p.76). Outside that, the pin's protection diodes
conduct, which can disturb readings or damage the pin.

### Part 2: DC mirror (25 marks)

**Do (for the tutor's chosen level):**
1. DMM on **PA5**: read V_in.
2. In FULL12, read C and D off the LCD and point out that D = C.
3. DMM on **PA4**: read V_out.
4. Press SW0 for COARSE6. Read C and D: D is a multiple of 64 and ≤ C. DMM on PA4 again.

**Say it out loud like this** (example for 1.65 V):

> "PA5 measures 1.650 V. The ADC estimate is C × V_R / 4096 = 2057 × 3.286 / 4096 = 1.650 V,
> which agrees. In FULL12, D = C = 2057, so the predicted output is the same 1.650 V,
> and I measure ___ V at PA4.
> In COARSE6, D = 2048 = 32 × 64. The predicted output is 2048 × 3.286 / 4096 = 1.643 V,
> and I measure ___ V. That's about 7 mV below the input, because COARSE6 rounds down
> to the next multiple of 64."

**Quick check that D is a multiple of 64:** divide by 64 and you should get a whole number (e.g. 2048 / 64 = 32).

### Part 3: Triangle and steps (25 marks)

**Generator:** 2 Hz triangle, 1.7 Vpp, 1.65 V offset, High-Z. The limits should be about 0.8 to 2.5 V; verify on the scope.

**Do:**
1. **FULL12:** CH1 = PA5, CH2 = PA4, **50 ms/div**, both about 500 mV/div. The two traces overlap: the output follows the input.
2. Press SW0 for **COARSE6.** The output becomes a visible staircase.
3. **Zoom in:** **2–5 ms/div**, CH2 at **20–50 mV/div**, and use CH2's vertical position knob to bring the plateaus on screen.
4. Put **voltage cursors** on two **adjacent, settled** plateaus. Read ΔV.

**Numbers to expect:**
- Input slope = 2 × 1.7 V × 2 Hz = **6.8 V/s**.
- Each plateau lasts 51.34 mV ÷ 6.8 V/s ≈ **7.5 ms**, about 15 samples.
- About **33 steps per ramp** (codes 960 → 3072).

**Say:** "Predicted step = V_R/64 = 3.286/64 = 51.34 mV. The cursors measure ___ mV,
a difference of ___ %. The difference comes from DAC DNL/INL (individual
steps aren't perfectly equal), noise, and the scope's vertical resolution: an
8-bit scope at 50 mV/div has about 1.6 mV per level."

**If the edges of a plateau flicker:** the input is sitting right at a boundary
between two coarse codes, so noise makes C jump back and forth across it.
Measure on plateaus away from the edges.

### Part 4: Understanding (20 marks)

These are the six topics listed in the handout, with model answers.

| They ask about… | Answer |
|---|---|
| **ADC channel** | "CHSELR bit 5 selects ADC_IN5, which is PA5. I wrote the whole register with `=` so channel 5 is the only one in the sequence; one ADSTART converts just PA5." |
| **ADC resolution** | "RES = 00 in CFGR1 gives 12 bits, so 4096 codes. RES can only be written while ADEN = 0, so I configure before the helper enables the ADC." |
| **Acquisition (sampling) time** | "SMPR = 111 gives 239.5 ADC clocks to charge the sample-and-hold capacitor. That's the most accurate setting. The conversion takes 18 µs in total, far shorter than the 500 µs between samples." |
| **DAC enable / buffer / trigger** | "EN1 = 1 turns channel 1 on and connects PA4. BOFF1 = 0 keeps the output buffer on for low output impedance. TEN1 = 0 disables triggers, so every DHR12R1 write appears at the output one APB clock later. That's the software-write mode." |
| **Conversion start / result** | "Set ADSTART to start one conversion. EOC goes high when the result is in DR. Reading DR gives the 12-bit right-aligned code and also clears EOC." |
| **Bounded wait** | "I poll EOC at most 1000 times instead of `while(!EOC)`. A broken ADC then can't freeze the program inside the interrupt; the function returns false, sampling stops and the LCD shows `ADC read failed`." |
| **Write-one-to-clear flags** | "ISR flags clear when you write a **1** to them. I clear EOC, EOSEQ and OVR with a plain `=`. Using `\|=` would read the register and write back a 1 for every flag already set, clearing flags by accident." |
| **Software quantisation** | "In COARSE6, `C & 0xFC0` throws away the 6 least significant bits, leaving only 64 output levels, 51.34 mV apart. This happens only in software." |
| **12-bit hardware vs 6-bit effective** | "Neither converter changed: the ADC still converts 12 bits and the DAC still accepts 12-bit commands. Only 64 of the DAC's 4096 commands are ever used, so the output has 6-bit *effective* resolution." |
| **Resolution vs accuracy** | "Resolution is the smallest step the system can represent: 0.80 mV in FULL12, 51.34 mV in COARSE6. Accuracy is how close the real voltage is to the true value. Offset, gain error, nonlinearity, noise and reference error all hurt accuracy no matter how many bits there are." |

---

## 5. Theory questions

**Why V_R/4096 and not V_R/4095?**
- There are 4096 codes, which divide the 0 … V_R range into 4096 equal steps.
- RM0091 gives DAC output = V_DDA × DOR / 4096 (§14.5.3 p.271).
- The highest code, 4095, gives V_R − 1 LSB, not exactly V_R.

**How many distinct outputs in each mode?**
FULL12: 4096 (0–4095). COARSE6: 64 (0, 64, …, 4032).

**Why does the FULL12 trace look smooth? Is it really continuous?**
- No. It is still a staircase of discrete codes. The DAC only updates every 0.5 ms.
- In that time the 2 Hz triangle moves about 3.4 mV ≈ 4 LSB, so the steps are only ~3 mV tall and 0.5 ms wide.
- That's too small for the scope to show at 50 mV/div–500 mV/div, so it *looks* smooth.
- **A smooth-looking trace doesn't prove continuous resolution.** It only means the steps are smaller than what the scope can show.

**Why doesn't copying the code guarantee identical voltages?**
D = C is an exact *digital* copy, but each converter adds its own **analog** errors:
- **ADC** (datasheet Table 54 p.78, 25 °C): offset ±1.5 LSB max, gain ±1.5 LSB max, total unadjusted error ±2 LSB max.
- **DAC** (datasheet Table 55 p.80–81): offset ±10 mV max, gain ±0.5 %, INL ±4 LSB, DNL ±2 LSB.
- **Noise:** codes flicker a few LSB.
- **Buffer limits:** the DAC output can't go below 0.2 V or above V_DDA − 0.2 V.

So the FULL12 output can be off by **up to roughly 10–20 mV** even with perfect code.

**Why can't the mirror prove either converter's absolute accuracy?**
- Both use the same reference. If V_DDA were wrong by 1 %, C would come out 1 % different, the DAC would scale it back by the same 1 %, and V_out would still ≈ V_in.
- The reference error **cancels** (the system is *ratiometric*), so matching input and output doesn't mean either converter is accurate on its own.
- To check absolute accuracy you'd compare against a known, accurate external reference.

**What limits the measured coarse step?**
- V_R measurement uncertainty: 51.34 mV scales with V_R.
- DAC DNL: steps aren't all exactly 64 LSB.
- Noise on the plateaus.
- Scope vertical resolution and cursor placement.
- Probe/channel gain error.

**Why is the coarse output always *below* the input?**
Clearing bits rounds down (floor). The error ranges from 0 to −63 LSB, averaging about −32 LSB ≈ −26 mV.

**What's the maximum input frequency?**
- The sample rate is 2 kHz, so the Nyquist limit is **1 kHz**. Above that the output aliases: it shows the wrong frequency.
- In practice you'd want many samples per cycle. At 2 Hz there are 1000 samples per cycle, which is plenty.

**What is quantisation error?**
- The difference between the true input and the voltage of the code that represents it.
- In FULL12 it's below 1 LSB (0.8 mV). In COARSE6 it's up to 64 LSB (51 mV).

---

## 6. Curveballs: "what if you changed…"

| Change | What happens / what to edit |
|---|---|
| **Make it 4-bit coarse instead** | `return code & (0xFUL << 8);` (= `0xF00`). That gives 16 levels, step = V_R/16 = **205 mV**, maximum 3840. |
| **Make it 8-bit** | `code & (0xFFUL << 4)` (= `0xFF0`). 256 levels, step = V_R/256 = 12.8 mV. |
| **Round to nearest instead of down** | Add half a step first: `code + 32`, then mask. You **must clamp**, because 4064 + 32 = 4096 = `0x1000`, and `& 0xFC0` gives **0**. So: `if (code >= 4064) return 4032; return (code + 32) & 0xFC0;`. The error becomes ±32 LSB, centred on zero. |
| **Do the 6-bit in hardware instead (RES = 11)** | Set `ADC1->CFGR1 \|= (3UL << 3);` **before** enabling (ADEN = 0). The ADC itself gives a 6-bit result. Where the 6 bits land in DR depends on ALIGN (RM0091 Figure 37 p.244); with right alignment it's 0–63, so you'd shift it `<< 6` before writing the DAC. Conversion is faster: t_SAR = 7.5 cycles instead of 12.5 (Table 46, p.240). The output is the same staircase, but now the *ADC* is 6-bit; the practical wants it done in software. |
| **Use PA6 instead of PA5** | PA6 = ADC_IN6 (datasheet Table 13 p.33). Set `CHSELR = (1UL << 6)` and put PA6 in analog mode: `GPIOA->MODER \|= (3UL << 12)`. |
| **Use the on-board pot POT0** | POT0 connects to **PB0 = ADC_IN8** (datasheet p.33) through the 2×2 jumper. Enable the GPIOB clock, set PB0 analog (`GPIOB->MODER \|= (3UL << 0)`), `CHSELR = (1UL << 8)`. Note PB0 also drives LED D1. |
| **Turn the buffer off (BOFF1 = 1)** | The output can reach closer to 0 V and V_DDA, but output impedance is ~15 kΩ (datasheet Table 55 p.80). A 10 MΩ DMM or probe barely loads it, but any real load would pull the voltage down. |
| **Forget EN1** | The DAC stays powered down. PA4 isn't driven and reads ~0 V or floats, even though the LCD shows a correct D. |
| **Use continuous mode (CONT = 1)** | The ADC converts nonstop. Between our 0.5 ms reads, many results arrive unread, so **OVR** sets. With OVRMOD = 0 (our setting), DR **keeps the old value** after an overrun (RM0091 §13.6.2 p.244), so we'd read stale data. |
| **Left-align (ALIGN = 1)** | DR holds C << 4 (data in bits 15:4). Our `& 0xFFF` mask would take the wrong bits, giving nonsense codes. |
| **`while (!(ISR & EOC));` with no bound** | Works while the ADC is healthy. If the ADC fails, the interrupt never returns: the LCD freezes, the buttons stop working and the program is stuck. |
| **`ISR \|= …` instead of `ISR = …`** | Reads ISR and writes back a 1 for every flag that was set, e.g. ADRDY. Those flags get cleared without us meaning to. |
| **No clamp in `write()`** | 4096 would output 0 V (bit 12 dropped). It can't happen with our ADC, but the function would be unsafe if called with bad input. |
| **Faster triangle, e.g. 200 Hz** | 5 ms period means only 10 samples per cycle. Even FULL12 shows a visible 0.5 ms staircase. Near 1 kHz it aliases. |
| **Input above V_R** | ADC saturates at 4095. FULL12 output tops out around V_DDA − 0.2 V because of the buffer limit. Don't do it: the pin protection diodes conduct. |
| **Change the sample rate** | Change TIM2 ARR in `sampling.c`: f = 1 MHz / (ARR + 1). The conversion must fit: 18 µs ≪ period. |

---

## 7. Bench troubleshooting

| Symptom | Likely cause |
|---|---|
| LCD blank | Contrast: turn POT2. Hold SW3 during reset if the text is garbled. |
| `Check TODOs` / red LED | `voltage_mirror_init()` returned false: the helper or ADC enable timed out. Re-flash, power-cycle. |
| `ADC read failed` | EOC never set: the ADC isn't converting. |
| C stuck at 0 or 4095 | Nothing on PA5, wrong pin, generator off, or input outside 0 … V_R. |
| PA4 doesn't follow C/D | Probing the wrong pin (PA4 is HAT pin 1), or no common ground. |
| Readings twice as large as expected | Generator in **50 Ω** mode: switch to High-Z. |
| Scope amplitude ×10 wrong | Probe on 10× but channel menu on 1× (or the other way round). |
| Coarse steps look uneven | Normal DAC DNL. Measure adjacent *settled* plateaus. |
| Noisy codes (±several LSB) | Long leads or a ground loop. Shorten the wires and use one ground point. |
