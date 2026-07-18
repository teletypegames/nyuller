# nyuller

A C64 programming project using [Oscar64](https://github.com/drmortalwombat/oscar64)
as the cross-compiler. Oscar64 is checked in as a git submodule under
`./oscar64/`.

## Layout

```
.
├── oscar64/        # Oscar64 cross-compiler (git submodule)
├── build/          # Build output (gitignored)
├── docs/c64/       # Low-level C64 reference (memory map, VIC, CIA, SID, …)
├── src/            # Your C code
│   ├── helloworld.c
│   └── build.sh    # Compile + run helper
├── OSCAR64.md      # Notes on the Oscar64 compiler internals
└── PROG_C64.md     # Notes on programming the C64 hardware
```

## First-time setup

```sh
# 1. Clone with submodules:
git clone --recurse-submodules <this-repo-url>

# Or, if you already cloned without --recurse-submodules:
git submodule update --init --recursive
```

## Building

```sh
cd src
./build.sh            # compile main.c → build/whack_hare.prg (-O1)
./build.sh -e         # run in oscar64's built-in emulator (headless, fast)
./build.sh -p         # PLAY: launch VICE in background (see build.sh --help)
./build.sh -v         # run in VICE x64 (foreground, blocks terminal)
./build.sh -V         # run in VICE x64sc (cycle-exact, foreground)
./build.sh --kill     # kill any detached VICE process
./build.sh -c         # just compile
./build.sh -O3        # release build (auto-ZP, outliner, aggressive inlining)
```

`build.sh` will build the oscar64 compiler automatically the first time
(it runs `make -C make compiler` inside `./oscar64/` if
`./oscar64/bin/oscar64` doesn't exist yet).

**Default test tool is the oscar64 built-in emulator** (`-e`): it
runs headless, needs no ROMs, no display, and is fast. Every `Verify`
step in `tasks.md` uses this.

**VICE 3.9 is installed** at `/usr/bin/` (`x64`, `x64sc`, `x128`,
`xvic`, `xpet`) and is available via `-v` / `-V`. It is a GUI
emulator and **needs a real X11 / Wayland display to render** — it
won't produce useful screenshots in this headless environment.
Use it from a real terminal session for interactive play-testing
and cycle-exact validation of raster IRQ and SID timing; don't
expect to script it.

## Documentation

- `PROG_C64.md` — how the C64 hardware actually works, from a low-level
  programming perspective. Start here if you want to understand what's
  going on under the hood.
- `OSCAR64.md` — how the Oscar64 compiler works internally, plus a C64-
  specific section on how to use it well (memory model, bank switching,
  raster IRQs, common mistakes).
- `docs/c64/` — downloaded reference material: the full Commodore 64
  Programmer's Reference Guide text, Christian Bauer's canonical VIC-II
  paper, and per-chip reference notes.
