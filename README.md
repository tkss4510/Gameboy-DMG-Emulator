# Game Boy Emulator

A Game Boy (DMG) emulator in C++23 using SDL3. Runs on Windows and Linux.

## Features

- Full CPU instruction set, interrupts, `HALT` and `STOP`
- Timer, PPU (background, window, sprites) and joypad
- ROM-only, MBC1 and MBC2 cartridges, with battery saves

## Build

Requires a [premake5](https://premake.github.io/) binary and a C++23 compiler.

**SDL3** is always built from source, so check it out in `vendor/SDL` (tested with 3.2.31):

```sh
git clone --depth 1 --branch release-3.2.x https://github.com/libsdl-org/SDL vendor/SDL
```

On Windows premake compiles it directly. On Linux premake runs SDL's own CMake build, so [CMake](https://cmake.org/) must be installed, along with SDL's build dependencies (X11/Wayland, audio, ... development packages, see SDL's `docs/README-linux.md`). On Ubuntu/Debian:

```sh
sudo apt install build-essential cmake \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdrm-dev libgbm-dev \
    libgl1-mesa-dev libegl1-mesa-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev libdecor-0-dev
```

Tested on Ubuntu 24.04 with GCC 13.3 and premake5 5.0.0-beta7.

Generate the project, then build it:

```sh
premake5 vs2022              # Windows: Visual Studio solution
premake5 gmake               # Linux: Makefiles, then: make config=release
```

Binaries are written to `bin/Debug/` or `bin/Release/`:

| Binary | Purpose |
|--------|---------|
| `GameBoyEmulator` | The emulator |
| `gb_tests` | Blargg test runner |
| `gb_acid2` | dmg-acid2 runner |

## Run

```sh
bin/Release/GameBoyEmulator path/to/game.gb
```

Controls: `W A S D` = D-pad, `J` = A, `L` = B, `Space` = Select, `Enter` = Start.

## Tests

**Blargg** (e.g. `cpu_instrs`):

```sh
bin/Release/gb_tests path/to/01-special.gb
```

Prints the ROM's serial output. Exit code: `0` passed, `1` failed, `2` stuck or timed out.

**dmg-acid2:**

```sh
bin/Release/gb_acid2 path/to/dmg-acid2.gb [frames_before_dump=120]
```

Compare the window with the reference image from the dmg-acid2 repo. The frame is also saved to `acid2_output.ppm`.

ROMs: [Blargg tests](https://github.com/retrio/gb-test-roms), [dmg-acid2](https://github.com/mattcurrie/dmg-acid2).

Anything that creates a `GameBoy` initialises SDL video, so on a headless machine (CI, SSH) run it with `SDL_VIDEODRIVER=dummy`.

### Results

Checked on Linux with the Release build:

| ROM | Result |
|-----|--------|
| Blargg `cpu_instrs` (all 11 individual ROMs and the combined ROM) | pass |
| Blargg `instr_timing` | pass |
| dmg-acid2 | pass (pixel-identical to the reference image) |
| Blargg `mem_timing` | fail (see [Timing accuracy](#timing-accuracy)) |
| Blargg `halt_bug`, `interrupt_time` | fail |
| Blargg `oam_bug` | 2 of 8 pass (the OAM bug is not emulated) |
| Blargg `dmg_sound` | fail (no sound) |

`halt_bug`, `interrupt_time`, `mem_timing-2`, `oam_bug` and `dmg_sound` don't report over the serial port, so `gb_tests` has nothing to read for them. Run them in the emulator and read the result off the screen.

## Limitations

- DMG only (no Game Boy Color)
- No sound
- No serial transfer; only the Blargg runner reads `SB`/`SC`
- No MBC3/MBC5 or real-time clock. Such ROMs aren't rejected: they load without an error, but bank switching and cartridge RAM don't work
- STAT interrupts only fire for `LYC == LY`, not for the HBlank / VBlank / OAM select bits
- Saves are written to `MBC1_SAVE.sav` / `MBC2_SAVE.sav` in the working directory, so different games overwrite each other
- Echo RAM (`$E000`-`$FDFF`) is not mirrored to work RAM
- The HALT bug is only partly emulated (`halt_bug` fails) and the OAM corruption bug is not emulated (`oam_bug` fails)
- Timing is not fully accurate (see below)

## Timing accuracy

Every instruction takes the right number of cycles (`instr_timing` passes), but timing *inside* an instruction is not modelled (`mem_timing` fails):

- The timer and PPU are only advanced after a whole instruction has run (`GameBoy::Run` in `gameboy.cpp`), so memory accesses inside it see them a few cycles late.
- The PPU uses fixed mode lengths and draws each scanline in one go (`ppu.cpp`), so effects that change registers mid-line don't show.
- OAM DMA is instant instead of taking 160 M-cycles (`memory.cpp`).
