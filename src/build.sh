#!/bin/sh
# build.sh — compile and optionally run a single C64 program with Oscar64.
#
# Usage:
#   ./build.sh            # compile helloworld.c → build/helloworld.prg
#   ./build.sh -e         # compile, then run in the oscar64 built-in emulator
#   ./build.sh -c         # just compile (default; -c is a no-op for clarity)
#
# Output (in ./build/):
#   helloworld.prg   — loadable C64 program (run with x64, VICE, or real hw)
#   helloworld.asm   — full 6502 listing
#   helloworld.map   — region/section/object placement
#   helloworld.lbl   — VICE monitor label commands

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
SRC=helloworld.c
EMU_FLAGS=""

for arg in "$@"; do
	case "$arg" in
		-e) EMU_FLAGS="-e" ;;
		-c) ;;  # explicit compile-only, no extra flags
		-*) echo "unknown flag: $arg" >&2; exit 1 ;;
	esac
done

echo "compiling $SRC with $OSCAR64_BIN -> $BUILD_DIR/"
# -o puts the .prg in the build dir; the other artifacts (.asm, .map, .lbl)
# follow automatically since they share the base name.
"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$BUILD_DIR/helloworld.prg" "$SRC"

if [ -n "$EMU_FLAGS" ]; then
	echo "running helloworld.prg in oscar64's built-in emulator"
	# The emulator reads the .prg; re-run with -e pointing at the same output.
	"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$BUILD_DIR/helloworld.prg" -e "$SRC"
fi

echo "done: $BUILD_DIR/helloworld.prg"
