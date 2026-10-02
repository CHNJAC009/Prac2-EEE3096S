# Practical 2A — Demo Guide (Tasks 1–6)

For **Sibongokuhle (Student 1, SBYSIB014)** and **Jacob (Student 2, CHNJAC009)**.

The demo is worth 80 of the 100 marks. The handout says you must understand
**every register write and every significant line** of the code, and that a
result in the PDF you can't reproduce or explain at the bench earns nothing.
This guide goes task by task:

1. **What the task proves.** One sentence.
2. **How the code works.** Line by line, with where each value comes from.
3. **What to show.** Which `RUN_TASK`, Live Expressions and scope setup.
4. **What they will ask.** The handout's demo checkpoint, with answers in our own words.
5. **Curveballs.** Changes the tutor may ask you to make or predict.

**Documents referenced** (all in the repo root):
- **RM0091**: the STM32F0 reference manual (registers).
- **Datasheet**: `DS_stm32f051c4.pdf` (pins and alternate functions).
- **CAT25010**: `CAT25010.PDF` (the EEPROM datasheet).
- **README**: `Practical_2/Practical2_A/README.md`.

Page numbers are the printed page numbers.

---

## 0. Before the demo — checklist

- [ ] Bring the board, the USB cable and a laptop that builds and debugs the project.
- [ ] **P2 loopback jumpers removed** (README §4).
- [ ] Scope probes on **10×** AND the scope channel menu on **10×**.
- [ ] Scope ground clips on **board GND** only.
- [ ] Know how to switch tasks:
  1. Edit `#define RUN_TASK n` at the top of `Core/Src/main.c`.
  2. **Rebuild**, then start a **new** debug session and press **Resume**.
  3. Add `run_task` to Live Expressions and check it shows the task you expect.
- [ ] Live Expressions to have ready:

| Task | Variables |
|---|---|
| all | `run_task` |
| 1 | `task1_toggle_count`, `task1_half_period_ms` |
| 2 | `dbg_gpiob_moder`, `dbg_gpiob_afrh`, `dbg_spi_cr1`, `dbg_spi_cr2`, `dbg_spi_sr`, `dbg_sck_hz_predicted` |
| 3 | `task3_test_byte` (writable), `task3_rx_byte`, `task3_tx_count` |
| 4 / 5 | `eeprom_test_addr`, `eeprom_test_byte` (both writable), `eeprom_status_before`, `eeprom_status_after`, `eeprom_read_value`, `eeprom_verify_ok`, `eeprom_write_wait_ms`, `eeprom_timeout_count`, `eeprom_read_loop_enable` (writable) |
| 6 | `ee_state`, `ee_step_ms` (writable), `ee_poll_count`, `ee_busy_passes`, `ee_last_read`, `ee_use_fsm` (writable), plus `eeprom_verify_ok`, `eeprom_write_wait_ms` |

**If every Live Expression reads 0**, including `eeprom_test_addr`, which should
be 41, the debugger isn't looking at the running program. Usually you haven't
pressed Resume yet, or the session is stale. Stop the session, start a new one,
and press Resume.

**Where the signals are:**

| Signal | Pin | Where to probe |
|---|---|---|
| Task 1 output | PC13 | Header P1 |
| CS | PB12 | **Not on any header.** Probe EEPROM **pin 1** directly with a hook, and don't bridge it to pin 2. |
| SCK | PB13 | EEPROM pin 6, or its P2 header pin |
| MISO (data out of the EEPROM) | PB14 | EEPROM pin 2 |
| MOSI (data into the EEPROM) | PB15 | EEPROM pin 5 |

---

## SPI in one page (you need this for Tasks 2–5)

SPI is a 4-wire bus with one **master** (our STM32) and one **slave** (the EEPROM):

| Wire | Driven by | Job |
|---|---|---|
| **CS** (chip select, active LOW) | master | LOW = "EEPROM, listen". HIGH = "ignore the bus". |
| **SCK** (clock) | master | One clock pulse = one bit. 8 pulses = 1 byte. |
| **MOSI** (master out, slave in) | master | Bits we send |
| **MISO** (master in, slave out) | EEPROM | Bits the EEPROM sends back |

- **Full duplex.** On every clock pulse, one bit goes out on MOSI *and* one
  bit comes in on MISO. You can't receive without sending, so to read a byte
  we send a dummy byte (0x00) just to make the 8 clock pulses.
- **Two edges per pulse.** Each SCK pulse has a rising edge and a falling edge.
  One edge is where the receiver **samples** (reads) the data line. The other
  edge is where the sender **changes** the data line. The data must be
  **stable** at the sampling edge.
- **CPOL and CPHA** (in `SPI_CR1`) pick which is which:
  - **CPOL** = the level SCK sits at when idle (0 = low, 1 = high).
  - **CPHA** = which clock edge samples the data (0 = first edge, 1 = second edge).
- **What we use:** CPOL = 0, CPHA = 0, called **mode 0**. SCK idles low, data
  is sampled on each **rising** edge and changes on each **falling** edge:

```
CS    ‾‾\_______________________________________/‾‾
SCK   ____/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\________
          ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑      ↑ = rising edge: EEPROM SAMPLES MOSI here
MOSI  ===X b7 X b6 X b5 X b4 X b3 X b2 X b1 X b0 X===   X = data changes (on falling edges)
```

- **Why mode 0:** CAT25010 p.5 says the EEPROM *"latches input data on the
  rising edge of SCK"* (SI pin) and shifts its output out on the falling edge.
  It supports modes (0,0) and (1,1) (p.1, p.5). Mode 0 matches.
- **MSB first:** bit 7 goes out first (CAT25010 Figures 3 and 5, p.6–7).

---

## Task 1 — Memory-mapped GPIO (PC13 square wave)

**What it proves:** we can drive a pin by writing directly to hardware
registers through pointers, with no HAL.

### How the code works (`Core/Src/task1_gpio.c`)

**1. Addresses = base + offset (TODO 1.1/1.2)**

| Register | Base | Offset | Address | Source |
|---|---|---|---|---|
| RCC_AHBENR | RCC = 0x4002 1000 | 0x14 | 0x4002 1014 | RM0091 Table 1 p.48; §6.4.6 p.120 |
| GPIOC_MODER | GPIOC = 0x4800 0800 | 0x00 | 0x4800 0800 | RM0091 Table 1 p.48; §8.4.1 p.157 |
| GPIOC_ODR | GPIOC | 0x14 | 0x4800 0814 | RM0091 §8.4.6 p.159 |
| GPIOC_BSRR | GPIOC | 0x18 | 0x4800 0818 | RM0091 §8.4.7 p.159 |
| GPIOC_BRR | GPIOC | 0x28 | 0x4800 0828 | RM0091 §8.4.11 p.162 |

The `_Static_assert` lines compare our addresses with ST's header file. If one
is wrong, the build fails.

**2. Pointers (TODO 1.3)**

```c
static volatile uint32_t * const pGPIO_ODR = (volatile uint32_t *)(GPIOC_BASE_ADDR + GPIO_ODR_OFFSET);
```

This is a pointer to a 32-bit hardware register. `*pGPIO_ODR = x` writes to
the pin hardware.

**3. `task1_gpio_init()`**

| Line | What it does |
|---|---|
| `*pRCC_AHBENR \|= (1UL << 19);` | Turns on the clock for GPIOC (bit 19 = IOPCEN, RM0091 §6.4.6 p.120). `\|=` only sets that one bit and keeps the others. That's a read-modify-write. |
| `*pGPIO_MODER &= ~(3 << 26);` | Clears PC13's two mode bits. Each pin has 2 bits in MODER: pin 13 → bits 27:26, because 13 × 2 = 26. |
| `*pGPIO_MODER \|= (1UL << 26);` | Sets them to `01` = general-purpose output (RM0091 §8.4.1). |
| `*pGPIO_BSRR = (1UL << 13);` | Drives PC13 HIGH as the starting level. Writing 1 to BSRR bit 13 *sets* pin 13. The 0s written to the other bits do nothing, so a plain `=` is safe. |

**4. `task1_gpio_update()`.** Every `task1_half_period_ms` (5 ms) it toggles
PC13 in two steps:

```c
if (*pGPIO_ODR & (1UL << 13))      // read: is PC13 HIGH right now?
    *pGPIO_BRR  = (1UL << 13);     // yes -> BR13: drive it LOW
else
    *pGPIO_BSRR = (1UL << 13);     // no  -> BS13: drive it HIGH
```

| Step | What it does |
|---|---|
| `*pGPIO_ODR & (1UL << 13)` | **Reads** ODR and keeps only bit 13. Non-zero = the pin is being driven high (RM0091 §8.4.6 p.159). It only reads; nothing is written to ODR. |
| `*pGPIO_BRR = (1UL << 13)` | BRR is **reset-only**: a 1 in bit 13 drives PC13 LOW. 0s do nothing (RM0091 §8.4.11 p.162). |
| `*pGPIO_BSRR = (1UL << 13)` | BSRR bits 15:0 are **set-only**: a 1 in bit 13 drives PC13 HIGH. 0s do nothing (RM0091 §8.4.7 p.159). |

Because BSRR and BRR ignore 0 bits, a plain `=` changes **only** PC13. No
read-modify-write of the output register is needed, so the other GPIOC pins
can't be disturbed.

**Measured:** period 9.93 ms (≈ 100.7 Hz), because each half period is 5 ms.

### What to show
1. Set `RUN_TASK 1`, build, debug, Resume.
2. Put the scope on PC13 (header P1). You should see a square wave.
3. In Live Expressions, `task1_toggle_count` keeps increasing.
4. You can change `task1_half_period_ms` live and the frequency changes on the scope.

### What they will ask (handout Task 1 checkpoint)

| Question | Answer |
|---|---|
| **Show the register writes** | Point to the four lines in `task1_gpio_init()` and the ODR-read → BRR/BSRR toggle above. |
| **Derive an address from base + offset** | "GPIOC is at 0x4800 0800 in the memory map (RM0091 Table 1, p.48). ODR is at offset 0x14 in the GPIO register map (§8.4.6). 0x4800 0800 + 0x14 = 0x4800 0814." |
| **What if the GPIO clock were disabled?** | "GPIOC gets no clock, so its registers don't work: our MODER, BSRR and BRR writes are ignored and reading ODR gives 0. RM0091 §6.4.6 p.120 says register values *'may not be readable by software and the returned value is always 0x0'*. PC13 stays in its reset state (input), so there is no square wave." |
| **What does `volatile` do?** | "It tells the compiler this memory can change outside the program (it's hardware), so it must really read or write it every time the code says so. Without it, the compiler could remove 'pointless' writes or keep an old value in a CPU register. For example, a loop polling a status flag could read the flag only once and spin forever." |
| **Why read ODR, then write BSRR or BRR?** (TODO 1.8 asks why we chose that register) | "To toggle, I need the current level, so I **read** ODR bit 13. Then I write the opposite level with BRR (reset) or BSRR (set). Those registers only act on bits written as 1, so one `=` write changes PC13 and nothing else. The alternative, `ODR ^= (1UL << 13)`, is a read-modify-write of the whole output register. That works here, but if something else (e.g. an interrupt) changed another GPIOC pin between the read and the write, ODR would overwrite that change. BSRR and BRR can't do that." |
| **Why BSRR for the start level?** | "I just want 'high', so one BSRR write sets PC13 without touching the other pins." |

### Curveball: "move the output to another free pin"

Say pin *n* on port X:
1. **Clock:** set that port's enable bit in RCC_AHBENR (RM0091 §6.4.6 p.120):
   - IOPAEN bit 17
   - IOPBEN bit 18
   - IOPCEN bit 19
   - IOPDEN bit 20
   - IOPFEN bit 22
2. **Base address:** use that port's base (RM0091 Table 1, p.48):
   - GPIOA 0x4800 0000
   - GPIOB 0x4800 0400
   - GPIOC 0x4800 0800
   - GPIOD 0x4800 0C00
   - GPIOF 0x4800 1400

   The offsets stay the same, because every GPIO port has the same register map.
3. **MODER:** clear `(3UL << 2n)`, then set `(1UL << 2n)`.
4. **ODR / BSRR / BRR:** use bit `n`, i.e. `(1UL << n)`.

Example on the same port, PC13 → PC14: MODER bits 29:28 (`3UL << 28` /
`1UL << 28`), ODR/BSRR/BRR bit 14. Check `Board.md` that the pin is actually free first.

---

## Task 2 — Configure the hardware SPI

**What it proves:** we set up the SPI peripheral ourselves, register by
register. No HAL, no CubeMX.

### The values and where they came from (`Core/Inc/board_config.h`, Step 0)

| Item | Value | Source |
|---|---|---|
| SPI instance | **SPI2** at 0x4000 3800 | Datasheet Table 15 p.38 (PB12–15 = SPI2 on AF0); RM0091 Table 1 p.48 |
| Pin mapping | PB12 CS, PB13 SCK, PB14 MISO, PB15 MOSI | README §5.1 (the handout's table is wrong). **We confirmed it with a continuity test.** |
| Alternate function | **AF0** on PB13/14/15 | Datasheet Table 15 p.38 |
| Peripheral clock | **PCLK = 8 MHz** | `SystemClock_Config()` uses HSI 8 MHz with no PLL, AHB ÷1, APB ÷1 (RM0091 §6.2 p.95–96, §6.4.2) |
| Divider | **BR = 100 (4)** → ÷32 | RM0091 §28.9.1 p.802 |
| Predicted SCK | **8 MHz / 32 = 250 kHz** | f_SCK = f_PCLK / 2^(BR+1) = 8 000 000 / 2^5 |
| CPOL / CPHA | **0 / 0** (mode 0) | CAT25010 p.5 (see "SPI in one page") |
| Bit order | **MSB first** | CAT25010 Figures 3 and 5, p.6–7 |

**Why SPI1 isn't used:** SPI1 is on other pins (PA4–7, PB3–5). Only SPI2 sits
on PB12–15 (Datasheet Table 15).

### How the code works (`Core/Src/task2_spi_config.c`, `eeprom_spi_init()`)

| Step | Code | Why |
|---|---|---|
| 2.4 clocks | `RCC->AHBENR \|= (1UL << 18)`, `RCC->APB1ENR \|= (1UL << 14)` | Turns on GPIOB (IOPBEN, §6.4.6) and SPI2 (SPI2EN, §6.4.8 p.123). They're on different buses, so two different registers. |
| 2.5 CS | `BSRR = EE_CS_MASK` **first**, then MODER12 = `01` (output) | Sets the level to HIGH *before* making the pin an output, so CS never glitches low. CS is active-low, so high = EEPROM ignores the bus. |
| 2.6 AF pins | MODER = `10` (alternate function) on PB13/14/15; AFRH = 0 (AF0) | MODER says "a peripheral drives this pin"; AFRH says *which* one. `AFR[1]` = AFRH, the register for pins 8–15 (AFR[0] is pins 0–7). 4 bits per pin. |
| 2.7 speed / pull-up | OSPEEDR = `11` on SCK and MOSI; PUPDR = `01` pull-up on MISO | While CS is high the EEPROM's output is **tri-stated** (not driving at all; CAT25010 p.5). The pull-up holds MISO at 1 instead of floating, so an idle read gives **0xFF**. |
| 2.8 CR2 = **0x1700** | FRXTH = 1 (bit 12), DS = 0111 (bits 11:8) = 8-bit frames | FRXTH makes RXNE ("byte received") go high after **8** bits. The default waits for 16 bits, so a single-byte transfer would wait forever. |
| 2.9 CR1 = **0x0324** | SSM = 1, SSI = 1, LSBFIRST = 0, BR = 100, MSTR = 1, CPOL = 0, CPHA = 0 | See the field table below |
| 2.10 enable | `CR1 \|= (1UL << 6)` (SPE) → CR1 = **0x0364** | Configure first, enable last (RM0091 §28.5.7–28.5.8 p.765–766). Nothing is sent until DR is written. |

**The SPI_CR1 fields** (RM0091 §28.9.1 p.801–802):

| Bit(s) | Field | Our value | Meaning |
|---|---|---|---|
| 9 | SSM | 1 | Software slave management: ignore the hardware NSS pin |
| 8 | SSI | 1 | Pretend NSS is high, so we stay master. **Without SSM + SSI → MODF fault → the SPI drops out of master mode and there's no clock.** (§28.5.5 p.762) |
| 7 | LSBFIRST | 0 | MSB first |
| 6 | SPE | 1 (set last) | SPI enable |
| 5:3 | BR | 100 | ÷32 → 250 kHz |
| 2 | MSTR | 1 | We are the master |
| 1 | CPOL | 0 | SCK idles low |
| 0 | CPHA | 0 | Sample on the first (rising) edge |

### What to show
1. Set `RUN_TASK 2`, build, debug, Resume.
2. Live Expressions (or the **SFRs** view) should show:
   - `dbg_gpiob_moder` = **0xA9000000**: PB15..PB12 = `10 10 10 01`
   - `dbg_gpiob_afrh` = **0x00000000** (AF0)
   - `dbg_spi_cr2` = **0x1700**
   - `dbg_spi_cr1` = **0x0364**
   - `dbg_spi_sr`: TXE (bit 1) = 1, BSY (bit 7) = 0, MODF (bit 5) = 0
   - `dbg_sck_hz_predicted` = **250000**
3. SCK stays idle (low) at this stage, because nothing has been sent yet.

### What they will ask (handout Task 2 checkpoint)

| Question | Answer |
|---|---|
| **Show the config registers** | Use the dbg values above, or the SFRs view. |
| **Explain each SPI field you changed** | Walk through the CR1 and CR2 tables above. |
| **Calculate SCK** | "8 MHz / 2^(4+1) = 8 MHz / 32 = 250 kHz (RM0091 §28.9.1)." |
| **Which GPIO AF fields?** | "MODER13/14/15 = `10`, and AFSEL13/14/15 in AFRH = `0000` (AF0). AF0 is SPI2 on these pins in Datasheet Table 15, p.38." |
| **Why is PB12 held HIGH?** | "CS is active low. While it's high the EEPROM ignores SCK and MOSI, so configuring the pins and sending the Task 3 test byte can't trigger a command. A transaction only starts when our driver pulls CS low." |

### Curveball: "predict what changing X does"

| Change | Effect on the scope |
|---|---|
| BR = 011 (÷16) | SCK = 500 kHz (period halves) |
| BR = 101 (÷64) | SCK = 125 kHz |
| CPOL = 1 | SCK idles **high**. With CPHA still 0 the first edge of each pulse is now *falling*, so sampling moves to the falling edges. |
| CPHA = 1 | Data is sampled on the **second** edge (falling, when CPOL = 0), so MOSI changes on the rising edges instead |
| LSBFIRST = 1 | MOSI bit order reversed: b0 first |
| DS = 1111 (16-bit) | 16 clocks per write |
| SSI = 0 (with SSM = 1) | MODF fault: the SPI drops out of master mode, so **no clock** |
| FRXTH = 0 | RXNE never sets after 1 byte, so `spi_transfer()` hangs |

---

## Task 3 — Prove the waveform

**What it proves:** the hardware really sends the byte we claim, in the
format we configured. The **waveform is the evidence**, not the code.

### Our group byte
n1 = 14 (SBYSIB014, Student 1), n2 = 9 (CHNJAC009, Student 2):

```
n1 XOR n2 = 0000 1110 XOR 0000 1001 = 0000 0111 = 7
B = (7 + 0x3D) mod 256 = (7 + 61) = 68 = 0x44 = 0100 0100
```

B isn't 0x00 or 0xFF, so no 0x5A adjustment is needed.

### How the code works (`Core/Src/task3_spi_transfer.c`)

**`eeprom_cs_low()`:** `BRR = EE_CS_MASK`. BRR is reset-only, so writing bit
12 drives PB12 low.

**`eeprom_cs_high()` (TODO 3.3).** CS must only go high after the last bit
has physically left the pin. RM0091's disable procedure (§28.5.9 p.767–768):
1. Wait until **FTLVL = 00**: the TX FIFO is empty, nothing is queued.
2. Wait until **BSY = 0**: the shift register has finished the last bit.
3. Then `BSRR = EE_CS_MASK` drives CS high.

> Why not just TXE? TXE = 1 means "there is **room** to write". The FIFO
> holds 4 bytes, so TXE can be 1 with bytes still waiting (§28.5.10 p.776).
> Raising CS too early cuts the end off a command, and the EEPROM throws it away.

**`spi_transfer(tx)`:** sends one byte and returns the byte received at the same time.

| Step | Code | Why |
|---|---|---|
| 3.4 | `while (!(SR & TXE))` | Wait until there's room to write |
| 3.5 | `*((volatile uint8_t *)&EE_SPI->DR) = tx;` | **8-bit write.** DR is declared 16-bit; a 16-bit write makes the STM32 send **two** bytes (16 clocks), called data packing (RM0091 §28.5.9 p.768). Casting to `uint8_t *` forces 8 clocks. |
| 3.6 | `while (!(SR & RXNE))`, then `return *((volatile uint8_t *)&EE_SPI->DR);` | Wait for the received byte, then read it with an **8-bit read** (same reason). Always read it, even if unused, or the RX FIFO fills up and later bytes are lost (overrun, §28.5.11 p.777). |

**RUN_TASK 3** sends `task3_test_byte` (= B) every 100 ms with **CS held high**,
so the EEPROM ignores it and the scope gets a repeating burst.
`task3_rx_byte` = **0xFF**, because the EEPROM isn't driving MISO and the
pull-up gives 1s.

### What to show
1. Set `RUN_TASK 3`, build, debug, Resume.
2. Scope:
   - CH1 = SCK, CH2 = MOSI
   - trigger on CH1 rising, Normal mode
   - 5 µs/div
3. Place the cursors **yourself**:
   - Measure from the 1st to the 8th rising edge and divide by 7, or use one period.
   - **Our measurement:** T = 3.99 µs → f = 250.6 kHz → error = |250.6 − 250| / 250 = **0.24 %**.
   - The small error is because the HSI oscillator is only factory-trimmed to ±1 % (Datasheet Table 37, p.64).
4. Decode: at each **rising** SCK edge, read MOSI (high = 1). First bit = b7.
   We read **0 1 0 0 0 1 0 0 = 0x44** ✔.

### What they will ask (handout Task 3 checkpoint)

| Question | Answer |
|---|---|
| **Measure SCK** | Cursors as above |
| **Identify the 8 bits** | Count 8 clock pulses; read MOSI at each rising edge |
| **Decode the value** | Write the bits b7…b0, then convert to hex |
| **Where is the sampling edge?** | "Mode 0 (CPOL = 0, CPHA = 0): data is sampled on the **rising** edge of SCK and changes on the falling edge. The EEPROM also latches SI on the rising edge (CAT25010 p.5)." |
| **Clock idle state** | Low (CPOL = 0). It's visible before the first pulse. |

### Curveball: "predict the MOSI pattern for byte X"
1. Write X in binary, **MSB first**. Example: 0xA5 = **1010 0101**, so the MOSI levels at the 8 rising edges are H L H L L H L H.
2. Prove it: change `task3_test_byte` in Live Expressions (no rebuild needed) and check the scope.

---

## Task 4 — EEPROM driver

**What it proves:** we can talk to a real SPI device using its datasheet:
status read, write enable, write, write-completion detection, read-back.

### Our address
```
A = (n1 + 3·n2) mod 256 = (14 + 27) mod 256 = 41 = 0x29
```

### The EEPROM facts (CAT25010)

| Item | Value | Where |
|---|---|---|
| Opcodes | WREN 0x06, WRDI 0x04, RDSR 0x05, WRSR 0x01, READ 0x03, WRITE 0x02 | Table 9, p.5 |
| Status bit 0 = **RDY** | **1 = busy writing**, 0 = ready | Table 10 p.6; text p.5, p.7 |
| Status bit 1 = **WEL** | 1 = writes enabled | Table 10 p.6 |
| Write cycle tWC | ≤ 5 ms, starting when CS rises after the WRITE | Table 7 p.4, "Byte Write" p.7 |
| Address format on **our board** | **2 address bytes, MSB first**, 8192 bytes | README §5.2 (the handout's CAT25040 uses 1 byte; the fitted part is different) |

### How the code works (`Core/Src/task4_eeprom.c`)

Every transaction is: CS low → bytes → CS high.

| Function | Bytes sent (MOSI) | Clocks | Notes |
|---|---|---|---|
| `eeprom_read_status()` (4.3) | 0x05, 0x00 (dummy) | 16 | The status comes back during the dummy byte. **You must send to receive** because SPI is full duplex. |
| `eeprom_write_enable()` (4.4) | 0x06 | 8 | **Its own transaction**, because WEL is only set when CS goes **high** after WREN (CAT25010 p.6). |
| `eeprom_write_byte()` (4.5) | WREN first, then 0x02, A[15:8], A[7:0], data | 8 + 32 | The EEPROM starts writing internally **when CS goes high** (p.7). |
| write-completion (4.6) | RDSR repeated | 16 each | Loop until RDY = 0. Bounded by a 50 ms timeout, but the timeout is **never** used as proof of completion. Records `eeprom_write_wait_ms`. |
| `eeprom_read_byte()` (4.7) | 0x03, A[15:8], A[7:0], 0x00 | 32 | The data comes back during the dummy byte |
| `leds_write_byte()` (4.8) | n/a | n/a | One BSRR write: low 16 bits = pins to set (`v`), high 16 bits = pins to reset (`~v`). PB0–7 show the byte and **PB8–15 are untouched** (CS and SPI are on PB12–15!). |
| `eeprom_write_verify_path()` (4.9) | n/a | n/a | status → write (+ wait) → status → read → compare → LEDs |

On **reset** the board runs `eeprom_read_only_path()`: it **reads only**,
never writes. That's what makes the persistence test meaningful.

### Our results (RUN_TASK 4, PA0)
- Status before **0x00**, after **0x00** (WEL cleared automatically after the write, p.7)
- Write completed after **3 ms** (≤ 5 ms tWC)
- Read back **0x44**, verify **1**, green LED on, LEDs = `0100 0100` (PB6 and PB2 on)
- Scope with CH1 = CS and CH2 = SCK, on a PA0 press:
  - **RDSR 16 → WREN 8 → WRITE 32 → RDSR 16, 16, 16…** (polling) → RDSR 16 → READ 32
  - 32 clocks for WRITE = 8 opcode + **16 address** + 8 data. That proves the 2-byte address on the wire.

### What to show
1. Set `RUN_TASK 4`, build, debug, Resume.
2. Press **PA0** once and point to the Live Expressions listed above.
3. Scope: CH1 = CS (EEPROM pin 1), CH2 = SCK, trigger on CH1 falling.
   - For a steady trace **that never writes**: set `eeprom_read_loop_enable = 1`.

### What they will ask (handout Task 4 checkpoint)

| They want to see | How | Say |
|---|---|---|
| Status-register access | `eeprom_status_before` | "RDSR 0x05 plus a dummy byte to clock the status back." |
| Write-enable behaviour | Status before has WEL = 0; the write succeeds, which needs WEL = 1; WEL is 0 again after | "WREN is its own transaction because WEL only sets on the CS rising edge. It clears itself after the write." |
| One write | PA0 | "WRITE 0x02, 2 address bytes MSB first, the data. Writing starts when CS goes high." |
| Write-completion detection | `eeprom_write_wait_ms` ≈ 3 | "I poll RDY in the status register until it's 0. The 50 ms timeout only stops a dead EEPROM hanging the board." |
| Read-back | `eeprom_read_value` = 0x44, `verify_ok` = 1 | |
| LEDs | `0100 0100` | "One BSRR write, so PB8–15 aren't disturbed." |
| Persistence | **Unplug the USB → plug it back in → no PA0** | LEDs show the stored byte straight away, because reset only reads. |

**Optional extra:** to show WEL = 1 directly, set a breakpoint on the line after
`eeprom_write_enable();` in `eeprom_write_byte`. When it stops, add
`eeprom_read_status()` in the Expressions view; it should give 0x02. Test this
before the demo.

### Curveball: "write a different byte / address"
1. Click `eeprom_test_byte` in Live Expressions and type the new value (e.g. `0xA5`). Change `eeprom_test_addr` if asked; valid addresses are 0–8191.
2. Press **PA0**. The LEDs show the new byte and `verify_ok` = 1.
3. **Persistence with the new byte:**
   - Keep `eeprom_test_addr` at **41** (0x29).
   - Unplug and replug the USB. The LEDs show 0xA5 straight away.
   - **The red LED is expected:** after a reset the RAM defaults come back (`eeprom_test_byte` = 0x44), so the startup check compares 0xA5 with 0x44.
   - The LEDs show what was actually stored, and that's the proof.
4. Afterwards, set `eeprom_test_byte` back to 0x44 and press PA0 again.

### "How did you confirm the 2-byte address format?" (README §5.2)
- "RDSR and WREN (no address) worked straight away.
- A WRITE with the 2-byte address was accepted: the write cycle ran for about 3 ms, WEL cleared afterwards, and the byte read back correctly.
- On the scope, WRITE and READ each show **32** clocks: 8 opcode + 16 address + 8 data. With a 1-byte address it would be 24, and the README says a write with too few address bytes is thrown away with WEL left set."

**Our board story** (if asked why we needed a different board at first):
- Jacob's EEPROM read status **0x8C** at power-up. Bits 3:2 (BP1:BP0) = 11 = **full array write protection** (CAT25010 Table 11, p.6), so every WRITE was refused: wait 0 ms, WEL stuck at 1.
- It worked on another board. We cleared the protection bits once with a WRSR, then removed that code.

---

## Task 5 — Diagnose the broken SPI

**What it proves:** we can find a fault from **evidence** (scope, registers,
datasheets), not by trial and error.

`RUN_TASK 5` builds the **exact same Task 4 program** with **one SPI
configuration setting changed**. It's done in `task5_fault_case.c`, which we
must **not edit**. `RUN_TASK 4` is the same program without the fault, so it
is our "corrected" build.

### The 6 steps the handout requires (README §6)

| Step | What it means |
|---|---|
| 1. Expected | What a correct transaction looks like (from Tasks 3 and 4) |
| 2. Observed | What `RUN_TASK 5` actually does |
| 3. First meaningful difference | The **earliest** place the faulty waveform differs from the good one |
| 4. Register responsible | Which register and field causes that difference |
| 5. Correction | The correct value (= `RUN_TASK 4`) |
| 6. Repeat the measurement | Same capture in `RUN_TASK 4`, and why it now works |

### What we have found so far

| | RUN_TASK 4 (correct) | RUN_TASK 5 (faulty) |
|---|---|---|
| `eeprom_status_before` | 0x00 | **0xFF** |
| `eeprom_read_value` | 0x44 | **0xFF** |
| `eeprom_verify_ok` | 1 | **0** |
| `eeprom_write_wait_ms` (PA0) | 3 ms | **50 ms** = timeout, `eeprom_timeout_count` goes up |
| MOSI vs SCK on the scope | MOSI changes on the **falling** edges, stable at the rising edges | MOSI changes **at the rising edges** |

**What this means, step by step:**
1. **0xFF on everything** is what we read when the EEPROM **isn't driving MISO
   at all.** It's tri-stated and our pull-up gives 1s (Task 2.7). So the
   EEPROM didn't understand our commands (RDSR, READ) and never answered.
2. **The write times out at 50 ms** because the status poll keeps reading 0xFF.
   Bit 0 (RDY) = 1 looks like "busy" forever. Our timeout stopped the board
   hanging, which is a good point to make in the demo.
3. **The scope shows why.** The EEPROM reads MOSI on the **rising** edge
   (CAT25010 p.5), and needs the data **stable** around that edge: setup time
   tSU and hold time tH, Table 7 p.4. In the faulty build MOSI is **changing
   at the rising edge**, so the EEPROM latches the wrong bits. The opcode it
   receives isn't 0x05 or 0x03, so it ignores the transaction.
   **That's the "electrical reason".**

### What is still to do (you must find the field yourselves)
1. **Find the register.** In both modes, read `dbg_spi_cr1` and `dbg_spi_cr2`
   (or the SFRs view → SPI2). Write both values in binary and find the bit(s)
   that differ. Use the CR1/CR2 field tables in the Task 2 section to name the field.
2. **Check it explains the scope.** The field you find must be one that
   controls **which SCK edge the data is sampled or changed on**.
   - Read RM0091 §28.5.6 "Communication formats" (p.763) and its figure.
   - Also compare the **SCK idle level** before the first pulse in both modes.
3. **Captures for the report.** Use the **same** setup in both modes:
   - CH1 = SCK, CH2 = MOSI
   - same timebase and trigger
   - `eeprom_read_loop_enable = 1`

   Save them as `Report/figures/task5_faulty.png` and `task5_corrected.png`.
4. **Optional, stronger:** CH2 on **MISO**. In mode 4 MISO toggles during the
   reply byte; in mode 5 it stays flat high, which proves the EEPROM is silent.
5. **Don't press PA0 in mode 5.** You don't need to write to diagnose, and a
   misread command could change the EEPROM. Use the read loop only.
6. **Afterwards, check the EEPROM is still fine:** `RUN_TASK 4`, power-cycle,
   no PA0. Expect status 0x00 and read 0x44.

Fill this in once confirmed (it's also the report table):

| Expected | Observed | Cause | Correction |
|---|---|---|---|
| Mode 0: MOSI stable at the rising SCK edges; status 0x00, read 0x44, write ≈ 3 ms | MOSI changes at the rising edges; status and read 0xFF; write poll times out (50 ms) | Register `SPI2_CR1`/`CR2`?, field `____`, value `__` | Field = `__` (RUN_TASK 4) → MOSI stable at the rising edges, EEPROM answers |

### What they will ask (handout Task 5 checkpoint)

| They ask | You answer with |
|---|---|
| **The symptom** | "Status and read both 0xFF, the write times out, verify fails. On the scope, MOSI changes at the rising SCK edges." |
| **The register responsible** | The register whose value differs between modes 4 and 5 |
| **The incorrect field value** | Its value in mode 5 |
| **The corrected field value** | Its value in mode 4, and why that matches the EEPROM (CAT25010 p.5: modes (0,0)/(1,1), SI latched on the rising edge) |
| **The electrical reason** | "The EEPROM samples MOSI on the rising edge. With the fault, the STM32 changes MOSI at that same edge, so the data isn't stable when it's latched (tSU/tH, Table 7). The EEPROM receives a wrong opcode, ignores the command and never drives MISO, so we read 0xFF." |
| **Predict a config change** | Use the "predict what changing X does" table in Task 2 |

---

## Task 6 — Non-blocking state machine

**What it proves:** the whole write/read/verify transaction can run *without*
the program ever sitting in a loop waiting for the EEPROM, so the board keeps
responding (to PA3) the whole time.

### The problem with Task 4
`eeprom_write_byte()` sends the WRITE and then **sits in a `while` loop**
reading the status until the write finishes (~3 ms). During those 3 ms nothing
else runs: no button checks, no LEDs, nothing. The handout forbids that in
Task 6 (and bans `HAL_Delay` or any busy-wait for the write cycle).

### The idea: a state machine
Break the transaction into **steps** (states). Every pass of the main loop:

```c
while (1) {
    read_inputs(now);                    // look at PA0 / PA3
    update_eeprom_state_machine(now);    // do ONE small step, then return
    update_outputs();                    // LEDs
}
```

The state machine does **one small step and returns immediately**. A variable
(`ee_state`) remembers where we are, so the next pass carries on from there.
If the EEPROM is still busy, the step is just "is it time to check again? No →
return." So the loop spins thousands of times while the EEPROM writes.

### Our states (`Core/Src/task6_fsm.c`, `ee_state_t`)

| # | State | What it does in one pass | Goes to |
|---|---|---|---|
| 0 | **IDLE** | Nothing. Waits for PA0. | PA0 → 1 |
| 1 | **WRITE_PREP** (write preparation) | Checks the address is valid (0–8191). Every 1 ms reads the status: if RDY = 0 (ready) it sends **WREN**. | ready + WREN → 2; bad address or still busy after 50 ms → 7 |
| 2 | **WRITE** (write transaction) | Reads the status to check **WEL = 1** (WREN worked), then sends **WRITE + 2 address bytes + data**. CS going high starts the write. | WEL = 1 → 3; WEL = 0 → 7 |
| 3 | **WAIT_BUSY** (status checking) | If less than 1 ms since the last check: **return immediately**. Otherwise **one** RDSR: RDY = 0 → done. | RDY = 0 → 4; RDY = 1 → stay (check again in 1 ms); 50 ms passed → 7 |
| 4 | **READ** (read transaction) | READ + 2 address bytes + dummy → `ee_last_read` | → 5 |
| 5 | **VERIFY** | Compares the byte read with the byte written | match → 6; different → 7 |
| 6 | **SUCCESS** | Green LED. Holds until the next PA0. | PA0 → 1 |
| 7 | **FAIL** | Red LED. Holds until the next PA0. | PA0 → 1 |

- **States 1–5 are "busy".** In any of them, **PA3 → IDLE** (abort), and both status LEDs go off.
- A PA0 press while busy is **ignored**, so a second press can't start a write on top of the first.
- The diagram is Figure 5 in the report: it shows the initial state (reset → IDLE), every transition condition, and the success, failure and abort paths.

### Key design points (what the tutor will probe)

| Point | Explanation |
|---|---|
| **One SPI frame per state** | Each state sends at most one or two complete CS-low…CS-high frames (≤ 32 clocks ≈ 128 µs). The waits inside `spi_transfer()` are for single **bits/bytes** on the SPI hardware (µs), not for the EEPROM's 3 ms write cycle. That's what the rule is about. |
| **Waiting without blocking** | WAIT_BUSY stores `ee_last_check = now` (from `HAL_GetTick()`, 1 ms ticks). Next pass: `if (now - ee_last_check < 1 ms) return;`. The handout explicitly allows a tick to decide *when the next status check is due*. |
| **Completion = status register** | We go to READ only when **RDY = 0** (CAT25010 p.5, p.7). The 50 ms timeout is only a hang guard (FAIL), never proof. The status is checked *before* the timeout, so a late check still counts a finished write as success. |
| **Abort leaves the bus usable** | PA3 is checked **first** on every pass. Every SPI frame starts and ends inside one call, so between calls CS is always HIGH and the SPI is idle. An abort can never leave CS low halfway through a command. |
| **Abort during the EEPROM's write** | The EEPROM finishes its write on its own; we can't stop it. That's why WRITE_PREP checks **RDY = 0 before sending WREN**: while busy, the EEPROM ignores everything except RDSR (CAT25010 p.7). |
| **Address/byte latched at PA0** | `ee_addr`/`ee_data` are copied from `eeprom_test_addr`/`eeprom_test_byte` when the transaction starts, so editing them mid-transaction can't corrupt it. |
| **Status LEDs** | Green = verified, red = failed, **both off = in progress or aborted**. At reset, the boot read result shows (TODO 6.4), so the persistence test still shows green. |
| **PB0–7** | `update_outputs()` writes `ee_last_read` every pass with one BSRR write (PB8–15 untouched). |

### Demo helpers we added

| Variable (Live Expressions) | Use |
|---|---|
| `ee_state` | The current state number (table above) |
| `ee_step_ms` (writable) | **Slow-motion mode.** Normally 0. A full transaction takes ~4 ms, which is far too fast to see `ee_state` change or to press PA3 in time. Set it to **1000**: every busy state then waits ≥ 1 s before doing its step, so you can watch 1 → 2 → 3 → 4 → 5 → 6 and have time to abort. It doesn't block: the FSM just keeps returning until the time has passed. |
| `ee_poll_count` | How many status checks WAIT_BUSY made (about 3–5 at full speed) |
| `ee_busy_passes` | **How many times the main loop ran during the transaction.** Thousands = the loop never stopped = non-blocking. |
| `ee_use_fsm` (writable) | 1 = our FSM; 0 = the Task 4 blocking path on PA0, for comparison |
| `ee_last_read` | Last byte read (the LEDs show it) |

### What to show
1. Set `RUN_TASK 6`, build, debug, Resume, and check that `run_task` = 6.
2. **Normal run:** `ee_step_ms` = 0, press PA0.
   - `ee_state` ends at **6**, green LED on, LEDs = 0x44.
   - `ee_poll_count` ≈ 3–5, `ee_busy_passes` in the thousands.
3. **Watch the states:** `ee_step_ms` = 1000, press PA0. `ee_state` steps 1 → 2 → 3 → 4 → 5 → 6, about once a second.
4. **Abort:** `ee_step_ms` = 1000, press PA0, then **PA3** while `ee_state` is 1–5.
   - `ee_state` → **0**, both status LEDs off.
   - Press PA0 again: it runs through to 6 normally, which shows the bus was left usable.

### What they will ask (handout Task 6 checkpoint)

| They want | How / what to say |
|---|---|
| Start with PA0 | Press PA0 (step 2 above) |
| State variable changing | `ee_step_ms` = 1000 and watch `ee_state` (step 3) |
| Successful verification | `ee_state` = 6, green LED, `eeprom_verify_ok` = 1 |
| Byte on the LEDs | 0x44 = `0100 0100` (PB6, PB2 on) |
| Abort with PA3 | Step 4 |
| **Why is it non-blocking?** | "No call ever waits for the EEPROM. Each call does at most one short SPI frame and returns. While the EEPROM writes, WAIT_BUSY only compares `HAL_GetTick()` with the last check time and returns if a check isn't due. So the main loop keeps running `read_inputs()` and PA3 is seen on the very next pass. `ee_busy_passes` proves it: thousands of loop passes during one 4 ms transaction. In the Task 4 version (`ee_use_fsm = 0`), the loop is stuck inside `eeprom_write_byte()` for the whole write." |

### Curveball: "change the address or byte, rebuild, repeat"
- **Without rebuilding:** edit `eeprom_test_addr` / `eeprom_test_byte` in Live Expressions, then press PA0.
- **With a rebuild** (as the handout says): change the starting values at the top of `Core/Src/task4_eeprom.c`:
  ```c
  volatile uint16_t eeprom_test_addr = (uint16_t)EEPROM_ADDR_A;   // e.g. change to 100u
  volatile uint8_t  eeprom_test_byte = (uint8_t)TEST_BYTE_B;      // e.g. change to 0xA5u
  ```
  Then rebuild, debug and press PA0.
  - **Don't** change `STUDENT_N1/N2`: the `_Static_assert`s in `main.c` would (correctly) fail.
  - Change the values back afterwards.
- Valid addresses are 0–8191. An address ≥ 8192 goes to FAIL (red) by design.

### Optional scope evidence (not needed for the report)
- **Setup:** CH1 = CS (EEPROM pin 1), CH2 = SCK, trigger on **CS falling edge**, Single, about 1 ms/div. Press PA0.
- **FSM** (`ee_use_fsm` = 1): RDSR, WREN, RDSR (WEL check), WRITE, then status checks **spaced about 1 ms apart**, with gaps where the loop is doing other work. Then READ.
- **Task 4** (`ee_use_fsm` = 0): the status checks run **back to back** with no gaps, because the CPU is stuck in the polling loop.

---

## Quick reference — numbers to know by heart

| Thing | Value |
|---|---|
| n1 / n2 | 14 / 9 |
| B | 0x44 = 0100 0100 |
| A | 41 = 0x29 |
| PCLK | 8 MHz (HSI) |
| BR | 100 → ÷32 → **250 kHz** |
| Measured SCK | 250.6 kHz (0.24 % error) |
| CR2 / CR1 | 0x1700 / 0x0364 |
| GPIOB MODER / AFRH (PB12–15) | 0xA9000000 / 0x00000000 |
| Mode | CPOL 0, CPHA 0, MSB first |
| SPI2 base / AF | 0x4000 3800 / AF0 |
| Opcodes | WREN 06, RDSR 05, WRITE 02, READ 03 |
| RDY / WEL | bit 0 (1 = busy) / bit 1 |
| Write time | 3 ms (max 5 ms) |
| Clocks | RDSR 16, WREN 8, WRITE 32, READ 32 |
| FSM states | 0 IDLE, 1 WRITE_PREP, 2 WRITE, 3 WAIT_BUSY, 4 READ, 5 VERIFY, 6 SUCCESS, 7 FAIL |
| FSM poll interval / timeout | 1 ms / 50 ms |
