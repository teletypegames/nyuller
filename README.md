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
│   └── *.c, *.h
├── Makefile        # Build + run targets (gnu make)
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
make                    # show help with all targets
make compile            # compile → build/nyuller.prg (-O1)
make run                # compile + run in oscar64 emulator (headless, fast)
make play               # compile + launch VICE x64 in background
make kill               # kill any detached VICE
make OPT=O3             # release build (auto-ZP, outliner, aggressive inlining)
make clean              # remove build/ artifacts
```

`make` will build the oscar64 compiler automatically the first time
(it runs `make -C make compiler` inside `./oscar64/` if
`./oscar64/bin/oscar64` doesn't exist yet).

Run `make help` for the full list of targets with descriptions.

**Default test tool is the oscar64 built-in emulator** (`make run`): it
runs headless, needs no ROMs, no display, and is fast. Every `Verify`
step in `tasks.md` uses this.

**VICE 3.9 is installed** at `/usr/bin/` (`x64`, `x64sc`, `x128`,
`xvic`, `xpet`) and is available via `make run-vice` / `make run-vice-cycle`.
It is a GUI emulator and **needs a real X11 / Wayland display to render** — it
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
