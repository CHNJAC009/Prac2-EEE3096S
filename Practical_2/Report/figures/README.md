# Figures

Save captures here with these exact names (PNG, JPG or PDF - if you use a
different extension, change the file name in the matching `\scopefig` /
`\subscope` call). Until a file exists, the report shows a red placeholder box.

| File | Section | What to capture |
|---|---|---|
| `task1_pc13.png` | 1 | PC13 square wave (header P1) |
| `task3_sck_mosi.png` | 3 | One byte B: CH1 = SCK (PB13), CH2 = MOSI (PB15), cursors on one SCK period |
| `task4_cs_sck.png` | 4 | Real EEPROM transaction: CH1 = CS, CH2 = SCK, all clocks visible while CS low |
| `task5_faulty.png` | 5 | Chosen measurement with `RUN_TASK 5` |
| `task5_corrected.png` | 5 | The **same** measurement with `RUN_TASK 4` |
| `task6_fsm.png` | 6 | State diagram (or draw it in TikZ; see `sections/06_fsm.tex`) |

Scope tips: probes and channel menu both on 10x, ground clip on board ground,
and use `eeprom_read_loop_enable = 1` (Live Expressions) for a steady,
read-only trace in Tasks 4 and 5.
