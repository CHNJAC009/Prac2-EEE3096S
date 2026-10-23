# Practical 3 — Demo Cheat Sheet (Voltage Mirror: ADC → DAC)

**Gradescope demo marking: 40 pts, four equal parts of 10.**

| Part | What's marked | Section |
|---|---|---|
| 2.1 | Setup and switching | [Part 1](#part-1--setup-and-switching-10) |
| 2.2 | DC mirror and actual codes | [Part 2](#part-2--dc-mirror-and-actual-codes-10) |
| 2.3 | Triangle and step measurement | [Part 3](#part-3--triangle-and-step-measurement-10) |
| 2.4 | **Two** oral questions: understanding | [Part 4](#part-4--two-oral-questions-10) |

The tutor picks **one** DC level, then watches the triangle, then asks **two**
questions. Parts 1–3 are just doing things at the bench and saying the right
sentence, so rehearse them until they're smooth.

---

## Numbers to know (our V_R = 3.2946 V)

| Quantity | Value |
|---|---|
| V_R (measured 3V3 / VDDA) | **3.2946 V** |
| 1 LSB = V_R / 4096 | **0.8043 mV** |
| Coarse step = V_R / 64 | **51.48 mV** |
| Coarse levels | **64**: 0, 64, …, **4032** |
| Predicted voltage | **V = D × 0.8043 mV**, i.e. D × 3.2946 / 4096 |
| Sample rate | **2 kHz** (every 0.5 ms) |
| One ADC conversion | **18 µs** |

**Our DC results** (from the report, to compare against live readings):

| Level | Mode | PA5 | C | D | Predicted PA4 | Measured PA4 |
|---|---|---|---|---|---|---|
| 0.8 V | FULL12 | 0.79955 | 992 | 992 | 0.7979 | 0.80163 |
| 0.8 V | COARSE6 | 0.79955 | 992 | 960 | 0.7722 | 0.77765 |
| 1.65 V | FULL12 | 1.6528 | 2045 | 2045 | 1.6449 | 1.6536 |
| 1.65 V | COARSE6 | 1.6528 | 2053 | 2048 | 1.6473 | 1.6536 |
| 2.5 V | FULL12 | 2.5040 | 3113 | 3113 | 2.5039 | 2.4753 ⚠ |
| 2.5 V | COARSE6 | 2.5040 | 3116 | 3072 | 2.4710 | 2.4753 |

⚠ The two 2.5 V rows have the same PA4 reading, which can't both be right
(their D values differ by 41 codes ≈ 33 mV). If the tutor picks 2.5 V, just
measure it live.

**Normal behaviour:** the output reads about **4–9 mV above** the prediction
(DAC offset), and the codes flicker by ±1–3.

---

## Part 1 — Setup and switching (10)

**Before the tutor arrives:**
- [ ] Board flashed and running: the LCD shows `FULL12` and `Cxxxx Dxxxx`.
- [ ] **P2 loopback jumpers removed.**
- [ ] Generator on **High-Z** (50 Ω mode doubles the real voltage).
- [ ] Probes on **10×** and the scope channel menu on **10×**. Both channels **DC** coupled.
- [ ] All grounds (generator, both scope clips, DMM COM) on **board GND**.

**Point to the pins:**

| Signal | Pin | Where |
|---|---|---|
| Input (generator, CH1) | **PA5** = ADC channel 5 | Expansion header **P1** |
| Output (DMM, CH2) | **PA4** = DAC channel 1 | **HAT connector pin 1** (DAC1) |
| Common ground | GND | Every instrument's ground goes here |

**Show safe limits:** display the generator signal on the scope **before**
connecting it to PA5, using Max/Min measurements. It must stay inside
0 … 3.29 V, away from both rails (our triangle measured 0.90–2.67 V).

**Switch modes:** press **SW0**. The LCD toggles between `FULL12` and `COARSE6`.

**Say:** "Generator in High-Z mode, and I checked its real limits on the scope
first, so PA5 stays between 0 V and V_R = 3.29 V. Everything shares board
ground. SW0 switches between FULL12 and COARSE6."

---

## Part 2 — DC mirror and actual codes (10)

**Do:**
1. DMM on **PA5**, then read V_in.
2. In **FULL12**, read C and D off the LCD. **D = C.**
3. DMM on **PA4**, then read V_out.
4. Press **SW0** for **COARSE6.** Read C and D: **D is a multiple of 64 and ≤ C.** DMM on PA4 again.

**Quick checks in your head:**
- D ÷ 64 must be a whole number (e.g. 2048 ÷ 64 = 32).
- Predicted V = D × 0.8043 mV (e.g. 2048 → 1.647 V).
- COARSE6 output should be **0 to 51 mV below** the input.

**Say it like this** (1.65 V example, using our numbers):

> "PA5 is 1.653 V. In FULL12 the LCD shows C = 2045 and D = 2045, so D equals C.
> The predicted output is D × V_R / 4096 = 1.645 V and I measure 1.654 V at PA4,
> about 9 mV high, which is the DAC's offset error.
> In COARSE6, C = 2053 and D = 2048, which is 32 × 64, a multiple of 64 below C.
> Predicted 1.647 V. COARSE6 drops the six lowest bits, so the output can only
> sit on 64 levels, 51.5 mV apart, and it always rounds down."

---

## Part 3 — Triangle and step measurement (10)

**Generator:** 2 Hz triangle, 1.7 Vpp, 1.65 V offset, High-Z.

**Do:**
1. **FULL12:** CH1 = PA5, CH2 = PA4, **50 ms/div**, **500 mV/div** on both. The traces lie on top of each other.
2. Press **SW0** for **COARSE6.** The output becomes a staircase.
3. **Zoom in:** **10 ms/div**, both channels **50 mV/div**. Use the vertical position knob to bring a ramp section on screen.
4. **Cursors:** open the Cursors menu and choose Y (voltage) cursors. Put Y1 on one **settled** plateau and Y2 on the next. Read **ΔY**. Measure a few steps and average them; last time we got **51.25 mV over 6 steps**.

**Say:** "Predicted step = V_R / 64 = 3.2946 / 64 = 51.48 mV. I measure ___ mV.
The difference is within measurement uncertainty: noise on the plateaus,
cursor placement and the scope's vertical resolution."

**Why FULL12 looks smooth:** its steps are only ~0.8 mV per code, updated every
0.5 ms. That's far too small to see at 500 mV/div, but the output is still made
of discrete steps.

**If a plateau edge flickers:** the input is right on a boundary between two
coarse codes and noise makes C jump across it. Put the cursors on plateaus away
from the edges.

---

## Part 4 — Two oral questions (10)

They ask **two** questions. The handout says they come from: ADC fields, DAC
fields, conversion start/result, bounded wait, write-1-to-clear, software
quantisation, 12-bit vs 6-bit effective, and resolution vs accuracy.

Learn the **bold first sentence** of each answer; the rest is backup detail.

### The code you'll be asked about (`starter/Core/Src/voltage_mirror.c`)

| Line | Meaning |
|---|---|
| `ADC1->CFGR1 &= ~(3UL << 3);` | **RES = 00 → 12-bit** resolution. Only writable while the ADC is off (ADEN = 0). |
| `ADC1->CFGR1 &= ~(1UL << 5);` | **ALIGN = 0 → right-aligned**: result in DR bits 11:0. |
| `ADC1->CFGR1 &= ~(3UL << 10);` | **EXTEN = 00 → software start** (no hardware trigger). |
| `ADC1->CFGR1 &= ~(1UL << 13);` | **CONT = 0 → single** conversion per start. |
| `ADC1->CHSELR = (1UL << 5);` | **Channel 5 = PA5**, and only that channel. |
| `ADC1->SMPR \|= (7UL << 0);` | **SMP = 111 → 239.5-cycle** sampling time (the longest). |
| `DAC->CR &= ~(1UL << 1);` | **BOFF1 = 0 → output buffer ON.** |
| `DAC->CR &= ~(1UL << 2);` | **TEN1 = 0 → no trigger**: each write appears at the output immediately. |
| `DAC->CR \|= (1UL << 0);` | **EN1 = 1 → DAC channel 1 on**, driving PA4. |
| `ADC1->ISR = EOC \| EOSEQ \| OVR;` | **Clear old flags** (write-1-to-clear). |
| `ADC1->CR \|= (1UL << 2);` | **ADSTART → start one conversion.** |
| `for (…1000…) if (ISR & EOC)` | **Bounded wait** for end of conversion. |
| `ADC1->DR & 0xFFF` | **Read the 12-bit result** (this also clears EOC). |
| `ADC1->CR \|= (1UL << 4);` | On timeout, **ADSTP aborts** the conversion; return false. |
| `if (code > 4095) code = 4095;` then `DAC->DHR12R1 = code;` | **Clamp, then write** the 12-bit DAC value. |
| `return code & (0x3FUL << 6);` | **COARSE6**: keep the top 6 bits (`0xFC0`), clear the bottom 6. |

### Question bank

**1. How did you configure the ADC (channel, resolution, acquisition time)?**
**"Channel 5 for PA5, 12-bit resolution, and the longest sampling time, 239.5 cycles."**
CHSELR bit 5 selects ADC_IN5; I wrote it with `=` so it's the only channel.
RES = 00 in CFGR1 gives 12 bits, written before the ADC is enabled because RES
can only change while ADEN = 0. SMPR = 111 gives 239.5 ADC clocks for the
sample capacitor to charge fully. The conversion takes 18 µs, far less than the
0.5 ms between samples. Also right-aligned, software start, single mode.

**2. How did you configure the DAC (enable, buffer, trigger)?**
**"EN1 turns channel 1 on, BOFF1 = 0 keeps the output buffer on, and TEN1 = 0 means no trigger."**
The buffer gives PA4 a low output impedance, so the DMM and probe don't pull
it down; the cost is it only swings from about 0.2 V to V_DDA − 0.2 V. With no
trigger, every write to DHR12R1 reaches the output one clock later. The 2 kHz
timing comes from the timer interrupt, not a DAC trigger.

**3. How do you start a conversion and get the result?**
**"Clear the flags, set ADSTART, wait for EOC, then read DR."**
EOC means a new result is in DR. Reading DR gives the 12-bit code and also
clears EOC automatically. I mask with `0xFFF` so the code is always 0–4095.

**4. What is the bounded wait and why use it?**
**"I poll EOC at most 1000 times instead of waiting forever."**
A plain `while (!EOC);` would hang inside the interrupt forever if the ADC ever
stopped responding: the LCD would freeze and the buttons would stop working.
With the limit, the function gives up, sets ADSTP, returns false, and the LCD
shows "ADC read failed". A normal conversion (18 µs) finishes well inside the limit.

**5. What does write-1-to-clear mean?**
**"Status flags in ADC_ISR are cleared by writing a 1 to them; writing 0 does nothing."**
That's why I use plain `=`: it clears exactly EOC, EOSEQ and OVR. `|=` would
read the register and write back a 1 for every flag already set, clearing
flags I didn't mean to, such as ADRDY. I clear before starting, so the EOC I
wait for must come from this conversion.

**6. Explain the COARSE6 mapping / software quantisation.**
**"`C & 0xFC0` keeps the top 6 bits and clears the bottom 6, giving D = 64 × floor(C/64)."**
That leaves only 64 output levels (0, 64, …, 4032), 64 codes = V_R/64 =
51.48 mV apart. It always rounds down, so the output sits 0–51 mV below the
input. Example: C = 2053 → D = 2048.

**7. Why is the highest coarse value 4032 and not 4095?**
**"4095 & 0xFC0 = 4032 = 63 × 64; the next multiple of 64 is 4096, which needs 13 bits."**

**8. Is the hardware 6-bit in COARSE6?**
**"No. Both converters stay 12-bit; only software throws away 6 bits."**
The ADC still converts 12 bits and the DAC still accepts 12-bit commands. We
just only ever send 64 of the 4096 possible commands, so the output has 6-bit
*effective* resolution.

**9. What's the difference between resolution and accuracy?**
**"Resolution is the smallest step the output can make; accuracy is how close it is to the correct voltage."**
FULL12 resolution is 0.80 mV, but our outputs were 4–9 mV off the prediction
(about 0.5 %) because of the converters' offset, gain and linearity errors and
noise. More bits give finer steps, not a more correct voltage.

**10. The FULL12 trace looks smooth. Is the output continuous?**
**"No, it's still a staircase; the steps are just too small for the scope to show."**
The DAC updates every 0.5 ms in steps of a few millivolts, much smaller than
what's visible at 500 mV/div.

**11. Why doesn't copying C to D give exactly the same voltage?**
**"D = C is an exact digital copy, but each converter adds its own analog error."**
ADC and DAC each have offset, gain and linearity errors, and noise makes C
flicker. They share V_R, so an error in V_R cancels: the mirror can look
right even if neither converter is accurate on its own.

**12. Why V_R / 4096 and not V_R / 4095?**
**"4096 codes divide the range into 4096 equal steps."**
The datasheet formula is V_out = V_DDA × D / 4096, so the top code 4095 gives
V_R − 1 LSB, not exactly V_R.

### Likely "what if" follow-ups

| They ask | Answer |
|---|---|
| Make it 4-bit coarse? | `code & 0xF00`: 16 levels, step V_R/16 ≈ 206 mV. |
| What if EN1 wasn't set? | DAC stays off; PA4 isn't driven even though the LCD shows the right D. |
| What if you forgot the clamp in `write()`? | 4096 = `0x1000` loses bit 12 and becomes 0, so a full-scale input would output 0 V. |
| Why sample in a timer interrupt? | Gives an exact steady 2 kHz; LCD updates take milliseconds and would make the rate uneven. |

---

## If something goes wrong at the bench

| Symptom | Fix |
|---|---|
| LCD blank | Turn **POT2** (contrast). Garbled text: hold **SW3** during reset. |
| `Check TODOs` / red LED | Init failed: re-flash, power-cycle. |
| `ADC read failed` | The ADC stopped converting: reset the board. |
| C stuck at 0 or 4095 | Wrong pin, generator off, or input outside 0 … V_R. |
| Readings twice the expected size | Generator in **50 Ω** mode: switch it to High-Z. |
| Scope amplitude off by ×10 | Probe 10× but channel menu 1× (or the other way round). |
| Codes jump around a lot | Long leads or loose ground: shorten them, use one ground point. |
