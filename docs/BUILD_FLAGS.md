# Build Flags: ATS_EX (ATS-20/ATS-20+, ATmega328P)

Compiler and linker flags used for firmware compilation

---

## Target

- MCU: **ATmega328P** (Arduino Nano / ATS-20 / ATS-20+)
- Toolchain: **avr-gcc / avr-g++** (Arduino AVR core)
- Language standard: gnu++11 (Arduino default)

---

## Compile flags (avr-g++ -c)

```
-Os
-ffunction-sections
-fdata-sections
-flto
-fuse-linker-plugin
-fno-fat-lto-objects
-fno-jump-tables
-mcall-prologues
-fno-unwind-tables
-fno-asynchronous-unwind-tables
-fno-rtti
-fno-threadsafe-statics
-fno-exceptions
-g3
-ggdb3
-fira-algorithm=priority
-fno-tree-scev-cprop
```

Notes:
- `-Os` : optimize for size (flash is 100% full in release builds)
- `-ffunction-sections -fdata-sections` + linker `--gc-sections` : dead-code elimination
- `-flto -fuse-linker-plugin -fno-fat-lto-objects` : link-time optimization across TUs
- `-fno-jump-tables` : avoids jump tables in flash (saves space, faster on AVR)
- `-mcall-prologues` : smaller function prologue/epilogue
- `-fno-unwind-tables -fno-asynchronous-unwind-tables` : drop unwind metadata
- `-fno-rtti -fno-threadsafe-statics -fno-exceptions` : C++ runtime trimming
- `-g3 -ggdb3` : full debug info (for objdump/avr-nm analysis)
- `-fira-algorithm=priority` : alternative register allocator (priority instead of Chaitin-Briggs), fewer spill/reload sequences
- `-fno-tree-scev-cprop` : disable constant propagation out of loops

> `-fira-algorithm=priority` and `-fno-tree-scev-cprop` are heuristic-only flags: they do not change program semantics, only code generation.

---

## Link flags (avr-g++ -o firmware.elf)

```
-Wl,--gc-sections
-Wl,--relax
-Wl,-O1
-Wl,--sort-common
-Wl,--sort-section=name
-Wl,-Map,firmware.map
-Wl,--cref
-Wl,--demangle
-Wl,--print-gc-sections
```

Notes:
- `--gc-sections` : remove unused sections (works with -ffunction-sections/-fdata-sections)
- `--relax` : AVR relaxation (e.g. smaller call/jmp encodings)
- `-O1` : linker-time optimization level
- `--sort-common --sort-section=name` : deterministic placement
- `-Map,firmware.map` : map file for flash/RAM usage analysis
- `--cref` : cross-reference table in the map
- `--demangle` : C++ names demangled in map/logs
- `--print-gc-sections` : list removed sections (useful for size debugging)

---

## Reference command lines

Compile a translation unit:

```
avr-g++ -c -Os -ffunction-sections -fdata-sections -flto -fuse-linker-plugin \
  -fno-fat-lto-objects -fno-jump-tables -mcall-prologues \
  -fno-unwind-tables -fno-asynchronous-unwind-tables \
  -fno-rtti -fno-threadsafe-statics -fno-exceptions -g3 -ggdb3 \
  -fira-algorithm=priority -fno-tree-scev-cprop \
  -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO_AVR_NANO \
  -I. -o file.o file.cpp
```

Link the firmware:

```
avr-g++ -Os -flto -fuse-linker-plugin -fno-fat-lto-objects \
  -mmcu=atmega328p \
  -Wl,--gc-sections -Wl,--relax -Wl,-O1 \
  -Wl,--sort-common -Wl,--sort-section=name \
  -Wl,-Map,firmware.map -Wl,--cref -Wl,--demangle -Wl,--print-gc-sections \
  -o firmware.elf *.o -lm
```

Produce the flashable hex:

```
avr-objcopy -O ihex -R .eeprom firmware.elf firmware.hex
avr-size --format=avr --mcu=atmega328p firmware.elf
```

---

## Relevant build-time defines (Defines.h)

These macros select features and affect flash usage, set them in `ATS_EX/Defines.h`:

- `PATCH_EX_SSB` : compressed SSB patch loader (recommended)
- `PATCH_EX_SSB_NEW` : new SSB patch with audio improvements
- `PATCH_EX_AM` : improved AM patch (audio-dips fix)
- `ENABLE_RDS_MINI` : lightweight RDS RadioText (off by default in v7.2.1)
- `MOD_NO_RDS` : flash-saving build without full RDS

> The release build uses the full ATmega328P flash (30720/30720 bytes)
> Any added feature requires disabling another (see README)

---

## Analysis helpers

```
avr-nm --size-sort -C firmware.elf   # symbol sizes (demangled)
avr-objdump -d -C firmware.elf       # disassembly
avr-size -A firmware.elf             # section sizes
```
