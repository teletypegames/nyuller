#!/bin/sh
# build.sh — compile and optionally run a single C64 program with Oscar64.
#
# Usage:
#   ./build.sh            # compile main.c → build/whack_hare.prg (-O1)
#   ./build.sh -e         # run in oscar64's built-in emulator (headless, fast)
#   ./build.sh -p         # PLAY: compile, wrap in .d64, launch VICE x64 detached
#   ./build.sh -P         # PLAY: same as -p but with VICE x64sc (cycle-exact)
#   ./build.sh -v         # run in VICE x64 (foreground, blocks terminal)
#   ./build.sh -V         # run in VICE x64sc (foreground, blocks terminal)
#   ./build.sh --kill     # kill any running VICE x64/x64sc process
#   ./build.sh -c         # just compile
#   ./build.sh -O0        # compile with -O0 (no optimization; debug build)
#   ./build.sh -O2        # compile with -O2 (more aggressive inlining)
#   ./build.sh -O3        # compile with -O3 (release build; auto-ZP, outliner)
#   ./build.sh -Os        # compile with -Os (optimize for size)
#   ./build.sh -g         # compile with -g (adds source-level debug info)
#
# Output (in ./build/):
#   whack_hare.prg   — loadable C64 program (run with x64, VICE, or real hw)
#   whack_hare.asm   — full 6502 listing
#   whack_hare.map   — region/section/object placement
#   whack_hare.lbl   — VICE monitor label commands
#   whack.d64        — disk image wrapping whack_hare.prg (for VICE autostart)
#
# For the development loop, use -e (oscar64's built-in emulator). It runs
# without a display, needs no ROMs, and is faster than VICE. Use -p when
# you want to play the game interactively — it launches VICE in the
# background and returns control to your shell so you can do other things.
#
# Default joystick keys in VICE (Settings → Input devices → Joystick
# settings to rebind if needed):
#   Port 2 (left, Scoot):  W/A/S/D for up/left/right/down, Left Ctrl for fire
#   Port 1 (right, Hare):  Arrow keys for up/left/right/down, Right Shift for fire
# (You can rebind under Settings → Input devices → Joystick settings.)
#
# Optimization: the default is oscar64's default (-O1).  Pass -O3 to
# produce a release build (auto-zero-page, outliner, aggressive inlining).
# The release build is functionally identical to the default build on
# cycle-accurate timings — the audio and frame counters are still
# 50 Hz because they use the raster IRQ, not CPU-bound loops.

set -e

# --- locate the oscar64 compiler -----------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OSCAR64_DIR="$ROOT/oscar64"
OSCAR64_BIN="$OSCAR64_DIR/bin/oscar64"
BUILD_DIR="$SCRIPT_DIR/build"
PRG="$BUILD_DIR/whack_hare.prg"
D64="$BUILD_DIR/whack.d64"

mkdir -p "$BUILD_DIR"

# --- kill any running VICE ------------------------------------------------
if [ "${1:-}" = "--kill" ]; then
	pids=$(pgrep -f "x64sc? +confirmonexit.*whack\.d64" 2>/dev/null || true)
	if [ -n "$pids" ]; then
		echo "killing VICE process(es): $pids"
		kill $pids 2>/dev/null || true
		sleep 1
		kill -9 $pids 2>/dev/null || true
	else
		echo "no VICE process running (autostart of whack.d64)"
	fi
	exit 0
fi

# Build the compiler if it's missing.
if [ ! -x "$OSCAR64_BIN" ]; then
	echo "oscar64 compiler not found at $OSCAR64_BIN; building it..."
	( cd "$OSCAR64_DIR" && make -C make compiler )
fi

if [ ! -x "$OSCAR64_BIN" ]; then
	echo "error: $OSCAR64_BIN is still missing after build" >&2
	echo "  try: cd $OSCAR64_DIR && make -C make compiler" >&2
	exit 1
fi

# --- parse args -----------------------------------------------------------
SRC=main.c
EMU_CMD=""
PLAY_VICE=""
OPT_FLAGS=""

for arg in "$@"; do
	case "$arg" in
		-e) EMU_CMD="oscar64" ;;
		-v) EMU_CMD="x64" ;;
		-V) EMU_CMD="x64sc" ;;
		-p) PLAY_VICE="x64" ;;
		-P) PLAY_VICE="x64sc" ;;
		-c) ;;
		-O0) OPT_FLAGS="$OPT_FLAGS -O0" ;;
		-O1) OPT_FLAGS="$OPT_FLAGS -O1" ;;
		-O2) OPT_FLAGS="$OPT_FLAGS -O2" ;;
		-O3) OPT_FLAGS="$OPT_FLAGS -O3" ;;
		-Os) OPT_FLAGS="$OPT_FLAGS -Os" ;;
		-g)  OPT_FLAGS="$OPT_FLAGS -g"  ;;
		-*) echo "unknown flag: $arg" >&2; exit 1 ;;
	esac
done

# --- compile --------------------------------------------------------------
echo "compiling $SRC with $OSCAR64_BIN -> $BUILD_DIR/"
"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$PRG" $OPT_FLAGS "$SRC"

# --- optional actions -----------------------------------------------------
case "$EMU_CMD" in
	"oscar64")
		echo "running whack_hare.prg in oscar64's built-in emulator"
		"$OSCAR64_BIN" -i="$OSCAR64_DIR/include" -o="$PRG" $OPT_FLAGS -e "$SRC"
		;;
	"x64"|"x64sc")
		echo "wrapping $PRG in $D64 (VICE autostart needs a disk image)..."
		if ! command -v c1541 >/dev/null 2>&1; then
			echo "error: c1541 not found (needed to build the .d64 wrapper)" >&2
			exit 1
		fi
		c1541 -format wh,of d64 "$D64" >/dev/null
		c1541 "$D64" >/dev/null <<EOF
write $PRG
EOF
		if [ -z "${DISPLAY:-}" ]; then
			echo "warning: no \$DISPLAY set; VICE won't show a window" >&2
		fi
		echo "running whack_hare in VICE ($EMU_CMD, blocking)"
		"$EMU_CMD" +confirmonexit -autostart "$D64"
		;;
esac

if [ -n "$PLAY_VICE" ]; then
	if [ -z "${DISPLAY:-}" ]; then
		echo "error: no \$DISPLAY set; cannot launch VICE in a window" >&2
		exit 1
	fi
	if ! command -v "$PLAY_VICE" >/dev/null 2>&1; then
		echo "error: $PLAY_VICE not found in PATH" >&2
		exit 1
	fi
	echo "wrapping $PRG in $D64 (VICE autostart needs a disk image)..."
	c1541 -format wh,of d64 "$D64" >/dev/null
	c1541 "$D64" >/dev/null <<EOF
write $PRG
EOF
	echo "launching VICE $PLAY_VICE in the background with $D64..."
	log="$BUILD_DIR/vice.log"
	( setsid nohup "$PLAY_VICE" +confirmonexit -autostart "$D64" \
		> "$log" 2>&1 < /dev/null & echo $! > "$BUILD_DIR/vice.pid" )
	# setsid + nohup detach VICE from this shell so it survives our exit.
	pid=$(cat "$BUILD_DIR/vice.pid")
	sleep 1
	if kill -0 "$pid" 2>/dev/null; then
		echo ""
		echo "VICE launched (PID $pid). Look for the x64 window on your desktop."
		echo "Default keys: Port 1 (Hare)  = Arrows + Right Shift (fire)"
		echo "             Port 2 (Scoot) = W A S D + Left Ctrl (fire)"
		echo "Rebind under Settings → Input devices → Joystick settings."
		echo "To stop VICE later:  ./build.sh --kill"
		echo "Log file: $log"
	else
		echo "error: VICE failed to start; see $log for details" >&2
		exit 1
	fi
fi

echo "done: $PRG"
