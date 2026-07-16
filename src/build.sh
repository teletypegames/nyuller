#!/bin/sh
# build.sh — compile and optionally run a single C64 program with Oscar64.
#
# Usage:
#   ./build.sh            # compile main.c → build/whack_hare.prg
#   ./build.sh -e         # run in oscar64's built-in emulator (headless, fast)
#   ./build.sh -v         # run in VICE x64 (interactive, needs a display)
#   ./build.sh -V         # run in VICE x64sc (interactive, cycle-exact, slow)
#   ./build.sh -c         # just compile
#
# Output (in ./build/):
#   whack_hare.prg   — loadable C64 program (run with x64, VICE, or real hw)
#   whack_hare.asm   — full 6502 listing
#   whack_hare.map   — region/section/object placement
#   whack_hare.lbl   — VICE monitor label commands
#
# For the development loop, use -e (oscar64's built-in emulator). It runs
# without a display, needs no ROMs, and is faster than VICE. VICE is for
# interactive use in a real terminal session, or for cycle-exact validation
# of raster IRQ and SID timing — which requires a working X display.

set -e

# --- locate the oscar64 compiler -----------------------------------------
# This script is ./src/build.sh, so the repo root is one level up.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OSCAR64_DIR="$ROOT/oscar64"
OSCAR64_BIN="$OSCAR64_DIR/bin/oscar64"
BUILD_DIR="$SCRIPT_DIR/build"

# Output goes in ./build/ (relative to the src dir), kept out of the source tree.
mkdir -p "$BUILD_DIR"

# Build the compiler if the binary is missing. (make -C make is the
# makefile-based build from the upstream oscar64 source tree.)
if [ ! -x "$OSCAR64_BIN" ]; then
	echo "oscar64 compiler not found at $OSCAR64_BIN; building it..."
	( cd "$OSCAR64_DIR" && make -C make compiler )
fi

if [ ! -x "$OSCAR64_BIN" ]; then
	echo "error: $OSCAR64_BIN is still missing after build" >&2
	echo "  try: cd $OSCAR64_DIR && make -C make compiler" >&2
	exit 1
fi

# --- compile + optionally run --------------------------------------------
# Phase 2+ entry point.  helloworld.c is kept around as a minimal sanity
# check; build it explicitly with `./build.sh -c helloworld` (TODO if
# needed).  Default is the real game.
SRC=main.c
EMU_CMD=""

for arg in "$@"; do
	case "$arg" in
		-e) EMU_CMD="oscar64" ;;         # oscar64's built-in emulator (headless)
		-v) EMU_CMD="x64" ;;            # VICE standard
		-V) EMU_CMD="x64sc" ;;          # VICE cycle-exact
		-c) ;;                          # explicit compile-only
		-*) echo "unknown flag: $arg" >&2; exit 1 ;;
	esac
done

echo "compiling $SRC with $OSCAR64_BIN -> $BUILD_DIR/"
# -o puts the .prg in the build dir; the other artifacts (.asm, .map, .lbl)
# follow automatically since they share the base name.
"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$BUILD_DIR/whack_hare.prg" "$SRC"

case "$EMU_CMD" in
	"")
		# compile only
		;;
	"oscar64")
		echo "running whack_hare.prg in oscar64's built-in emulator"
		"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$BUILD_DIR/whack_hare.prg" -e "$SRC"
		;;
	"x64"|"x64sc")
		if ! command -v "$EMU_CMD" >/dev/null 2>&1; then
			echo "error: $EMU_CMD not found in PATH" >&2
			exit 1
		fi
		if [ -z "${DISPLAY:-}" ]; then
			echo "warning: no \$DISPLAY set; VICE may not render correctly" >&2
			echo "  for headless testing use -e (oscar64's built-in emulator)" >&2
		fi
		echo "running whack_hare.prg in VICE ($EMU_CMD)"
		"$EMU_CMD" "$BUILD_DIR/whack_hare.prg"
		;;
esac

echo "done: $BUILD_DIR/whack_hare.prg"
