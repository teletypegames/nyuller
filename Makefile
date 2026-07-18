# nyuller — C64 game (Oscar64 cross-compiler)
# ============================================================
# GNU Make wrapper for building and running via oscar64 / VICE.
#
# Default target: help

# --- paths -------------------------------------------------
ROOT       := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OSCAR64_DIR := $(ROOT)/oscar64
OSCAR64_BIN := $(OSCAR64_DIR)/bin/oscar64
BUILD_DIR  := $(ROOT)/build
MUFFLON_DIR := $(ROOT)/mufflon
MUFFLON_BIN := $(BUILD_DIR)/mufflon
TOOLS_DIR  := $(ROOT)/tools

SRC        := main.c
PRG        := $(BUILD_DIR)/nyuller.prg
D64        := $(BUILD_DIR)/nyuller.d64
LOG        := $(BUILD_DIR)/vice.log
PID_FILE   := $(BUILD_DIR)/vice.pid
SRC_DIR    := $(ROOT)/src

# --- NUFLI source images -----------------------------------
NUFLI_SRC_DIR := $(ROOT)/source_images
NUFLI_OUT_DIR := $(SRC_DIR)/data/nufli

# --- oscar64 flags ----------------------------------------
# Override with:  make OPT=O3    (or O0/O1/O2/Os/g)
OPT          ?= O1
OPT_FLAGS    := $(if $(OPT),-$(OPT))

# --- VICE flags -------------------------------------------
VICE_FLAGS   := +confirmonexit -drive8type 1541

# --- source files (for dependency tracking) ----------------
SRC_FILES    := $(wildcard $(SRC_DIR)/*.c)
HDR_FILES    := $(wildcard $(SRC_DIR)/*.h)

# --- phony targets ----------------------------------------
.PHONY: help compile run run-vice run-vice-cycle \
        play play-cycle kill clean ensure-build-dir ensure-oscar64 \
        ensure-mufflon nufli nufli-clean

# ============================================================
# help (default)
# ============================================================
help:
	@echo ""
	@echo "  nyuller — C64 game (Oscar64 cross-compiler)"
	@echo "  ──────────────────────────────────────────────"
	@echo ""
	@echo "  Compile"
	@echo "    make                    compile main.c → build/nyuller.prg (-O1)"
	@echo "    make compile            same as above"
	@echo "    make OPT=O3             compile with -O3 (release build)"
	@echo "    make OPT=O0             compile with -O0 (debug build)"
	@echo "    make OPT=Os             compile with -Os (optimize for size)"
	@echo ""
	@echo "  Run"
	@echo "    make run                compile + run in oscar64 emulator (headless, fast)"
	@echo "    make run-vice           compile + run in VICE x64 (foreground, blocks)"
	@echo "    make run-vice-cycle     compile + run in VICE x64sc (cycle-exact, slow)"
	@echo ""
	@echo "  Play (detached — returns to shell)"
	@echo "    make play               compile + launch VICE x64 in background"
	@echo "    make play-cycle         compile + launch VICE x64sc in background"
	@echo "    make kill               kill any running detached VICE"
	@echo ""
	@echo "  Clean"
	@echo "    make clean              remove build/ artifacts and VICE log/pid files"
	@echo ""
	@echo "  NUFLI images"
	@echo "    make nufli              convert source PNGs → NUFLI assembly data"
	@echo "    make nufli-clean        remove NUFLI generated files"
	@echo ""
	@echo "  Output (in build/)"
	@echo "    nyuller.prg             loadable C64 program"
	@echo "    nyuller.d64             disk image wrapping nyuller.prg (for VICE autostart)"
	@echo "    nyuller.asm             full 6502 listing"
	@echo "    nyuller.map             region/section/object placement"
	@echo "    nyuller.lbl             VICE monitor label commands"
	@echo "    nyuller.int             intermediate compiler output"
	@echo "    nyuller.dbj             intermediate compiler output"
	@echo "    nyuller.csz             intermediate compiler output"
	@echo ""
	@echo "  VICE note: -drive8type 1541 is required for autostart to work."
	@echo "  Without this flag the autostart LOAD\"*\",8,1 fails with ?DEVICE"
	@echo "  NOT PRESENT.  We have the original 1541 ROM installed at:"
	@echo "    ~/.local/share/vice/DRIVES/dos1541-325302-01+901229-05.bin"
	@echo ""
	@echo "  Default joystick keys in VICE (rebindable under Settings):"
	@echo "    Port 2 (left, Scoot)  W/A/S/D + Left Ctrl (fire)"
	@echo "    Port 1 (right, Hare)  Arrows  + Right Shift (fire)"
	@echo ""

# ============================================================
# ensure-build-dir
# ============================================================
ensure-build-dir:
	@mkdir -p "$(BUILD_DIR)"

# ============================================================
# ensure-oscar64 — build the compiler if missing
# ============================================================
ensure-oscar64:
	@if [ ! -x "$(OSCAR64_BIN)" ]; then \
		echo "oscar64 compiler not found at $(OSCAR64_BIN); building it..."; \
		cd "$(OSCAR64_DIR)" && make -C make compiler; \
	fi
	@if [ ! -x "$(OSCAR64_BIN)" ]; then \
		echo "error: $(OSCAR64_BIN) is still missing after build" >&2; \
		echo "  try: cd $(OSCAR64_DIR) && make -C make compiler" >&2; \
		exit 1; \
	fi

# ============================================================
# ensure-mufflon — download source + build if missing
# ============================================================
ensure-mufflon: ensure-build-dir
	@if [ ! -f "$(MUFFLON_DIR)/mufflon.c" ]; then \
		echo "mufflon source not found; downloading from CSDb..."; \
		mkdir -p "$(MUFFLON_DIR)"; \
		curl -sL "https://csdb.dk/getinternalfile.php/180585/Mufflon1.0-source+GUI+Bonus.zip" -o "$(BUILD_DIR)/mufflon.zip"; \
		cd "$(BUILD_DIR)" && unzip -qo mufflon.zip "Mufflon1.0-source+Bonus/mufflon-source+GUI/mufflon.c" "Mufflon1.0-source+Bonus/mufflon-source+GUI/mufflon.h"; \
		cp "$(BUILD_DIR)/Mufflon1.0-source+Bonus/mufflon-source+GUI/mufflon.c" "$(MUFFLON_DIR)/"; \
		cp "$(BUILD_DIR)/Mufflon1.0-source+Bonus/mufflon-source+GUI/mufflon.h" "$(MUFFLON_DIR)/"; \
		rm -rf "$(BUILD_DIR)/Mufflon1.0-source+Bonus" "$(BUILD_DIR)/mufflon.zip"; \
	fi
	@if [ ! -f "$(MUFFLON_DIR)/mufflon.c" ]; then \
		echo "error: mufflon source download failed" >&2; \
		exit 1; \
	fi
	@if [ ! -x "$(MUFFLON_BIN)" ]; then \
		echo "building mufflon -> $(MUFFLON_BIN)"; \
		cd "$(MUFFLON_DIR)" && gcc -o "$(MUFFLON_BIN)" -lm -O5 -ffast-math mufflon.c; \
	fi
	@if [ ! -x "$(MUFFLON_BIN)" ]; then \
		echo "error: $(MUFFLON_BIN) is still missing after build" >&2; \
		exit 1; \
	fi

# ============================================================
# NUFLI image pipeline
# ============================================================

# Source screen images (PNG)
NUFLI_SCREENS := screen_title screen_waiting1 screen_waiting_2 screen_win_hare screen_win_scoot

# Generated files
NUFLI_NUF_FILES := $(addprefix $(NUFLI_OUT_DIR)/,$(addsuffix .nuf,$(NUFLI_SCREENS)))
NUFLI_ASM_FILES := $(addprefix $(NUFLI_OUT_DIR)/,$(addsuffix .asm,$(NUFLI_SCREENS)))
NUFLI_HDR_FILES := $(addprefix $(NUFLI_OUT_DIR)/,$(addsuffix .h,$(NUFLI_SCREENS)))

# Build all NUFLI images
nufli: $(NUFLI_ASM_FILES) $(NUFLI_HDR_FILES)
	@echo "NUFLI images built in $(NUFLI_OUT_DIR)/"

# Clean NUFLI artifacts
nufli-clean:
	@rm -f $(NUFLI_OUT_DIR)/*.nuf $(NUFLI_OUT_DIR)/*.asm $(NUFLI_OUT_DIR)/*.h
	@rm -f $(NUFLI_OUT_DIR)/*.bmp
	@echo "cleaned NUFLI images"

# PNG → BMP (Mufflon needs BMP input)
$(NUFLI_OUT_DIR)/%.bmp: $(NUFLI_SRC_DIR)/%.png | ensure-build-dir
	@mkdir -p $(NUFLI_OUT_DIR)
	@echo "converting $< -> $@"
	@python3 -c "from PIL import Image; Image.open('$<').resize((320,200), Image.LANCZOS).save('$@')"

# BMP → NUFLI (.nuf)
$(NUFLI_OUT_DIR)/%.nuf: $(NUFLI_OUT_DIR)/%.bmp | ensure-mufflon
	@echo "converting $< -> $@"
	@$(MUFFLON_BIN) $< -o $@ --shutup

# NUFLI → Assembly (.asm + .h)
$(NUFLI_OUT_DIR)/%.asm $(NUFLI_OUT_DIR)/%.h: $(NUFLI_OUT_DIR)/%.nuf
	@echo "converting $< -> $@ + $(basename $@).h"
	@python3 $(TOOLS_DIR)/nuf_to_asm.py $< $(NUFLI_OUT_DIR)/$*

# ============================================================
# $(PRG) — compile main.c → nyuller.prg
# ============================================================
$(PRG): $(SRC_FILES) $(HDR_FILES) $(NUFLI_ASM_FILES) | ensure-build-dir ensure-oscar64
	@echo "compiling $(SRC) with $(OSCAR64_BIN) -> $(BUILD_DIR)/"
	cd "$(SRC_DIR)" && "$(OSCAR64_BIN)" -i="$(OSCAR64_DIR)/include" -o="$(PRG)" $(OPT_FLAGS) "$(SRC)"

# ============================================================
# compile — convenience alias for $(PRG)
# ============================================================
compile: $(PRG)

# ============================================================
# run — oscar64 built-in emulator (headless, fast)
# ============================================================
run: ensure-build-dir ensure-oscar64
	@echo "running $(SRC) in oscar64's built-in emulator"
	cd "$(SRC_DIR)" && "$(OSCAR64_BIN)" -i="$(OSCAR64_DIR)/include" -o="$(PRG)" $(OPT_FLAGS) -e "$(SRC)"

# ============================================================
# run-vice — VICE x64 (foreground, blocks terminal)
# ============================================================
run-vice: $(D64)
	@command -v x64 >/dev/null 2>&1 || { echo "error: x64 not found in PATH" >&2; exit 1; }
	@[ -n "$$DISPLAY" ] || { echo "error: no \$DISPLAY" >&2; exit 1; }
	@echo "running nyuller in VICE x64 (blocking)"
	x64 $(VICE_FLAGS) -autostart "$(D64)"

# ============================================================
# run-vice-cycle — VICE x64sc (foreground, cycle-exact, slow)
# ============================================================
run-vice-cycle: $(D64)
	@command -v x64sc >/dev/null 2>&1 || { echo "error: x64sc not found in PATH" >&2; exit 1; }
	@[ -n "$$DISPLAY" ] || { echo "error: no \$DISPLAY" >&2; exit 1; }
	@echo "running nyuller in VICE x64sc (cycle-exact, blocking)"
	x64sc $(VICE_FLAGS) -autostart "$(D64)"

# ============================================================
# play — VICE x64 detached (returns to shell)
# ============================================================
play: $(D64)
	@command -v x64 >/dev/null 2>&1 || { echo "error: x64 not found in PATH" >&2; exit 1; }
	@[ -n "$$DISPLAY" ] || { echo "error: no \$DISPLAY" >&2; exit 1; }
	@# Kill any previous VICE first.
	@existing=$$(pgrep -f 'x64(sc)? \+confirmonexit.*nyuller\.d64' 2>/dev/null || true); \
	if [ -n "$$existing" ]; then \
		echo "killing previous VICE process(es): $$existing"; \
		kill $$existing 2>/dev/null || true; \
		sleep 1; \
		kill -9 $$existing 2>/dev/null || true; \
	fi
	@echo "launching VICE x64 in the background with $(D64)..."
	@setsid nohup x64 $(VICE_FLAGS) -autostart "$(D64)" \
		> "$(LOG)" 2>&1 < /dev/null &
	@sleep 1
	@pid=$$(pgrep -f 'x64 \+confirmonexit.*nyuller\.d64' 2>/dev/null | head -1); \
	if [ -n "$$pid" ]; then \
		echo "$$pid" > "$(PID_FILE)"; \
		echo ""; \
		echo "VICE launched (PID $$pid). Look for the x64 window on your desktop."; \
		echo "Default keys: Port 1 (Hare)  = Arrows + Right Shift (fire)"; \
		echo "             Port 2 (Scoot) = W A S D + Left Ctrl (fire)"; \
		echo "Rebind under Settings → Input devices → Joystick settings."; \
		echo "To stop VICE later:  make kill"; \
		echo "Log file: $(LOG)"; \
	else \
		echo "error: VICE failed to start; see $(LOG) for details" >&2; \
		exit 1; \
	fi

# ============================================================
# play-cycle — VICE x64sc detached (cycle-exact, returns to shell)
# ============================================================
play-cycle: $(D64)
	@command -v x64sc >/dev/null 2>&1 || { echo "error: x64sc not found in PATH" >&2; exit 1; }
	@[ -n "$$DISPLAY" ] || { echo "error: no \$DISPLAY" >&2; exit 1; }
	@existing=$$(pgrep -f 'x64(sc)? \+confirmonexit.*nyuller\.d64' 2>/dev/null || true); \
	if [ -n "$$existing" ]; then \
		echo "killing previous VICE process(es): $$existing"; \
		kill $$existing 2>/dev/null || true; \
		sleep 1; \
		kill -9 $$existing 2>/dev/null || true; \
	fi
	@echo "launching VICE x64sc in the background with $(D64)..."
	@setsid nohup x64sc $(VICE_FLAGS) -autostart "$(D64)" \
		> "$(LOG)" 2>&1 < /dev/null &
	@sleep 1
	@pid=$$(pgrep -f 'x64sc \+confirmonexit.*nyuller\.d64' 2>/dev/null | head -1); \
	if [ -n "$$pid" ]; then \
		echo "$$pid" > "$(PID_FILE)"; \
		echo ""; \
		echo "VICE launched (PID $$pid). Look for the x64sc window on your desktop."; \
		echo "Default keys: Port 1 (Hare)  = Arrows + Right Shift (fire)"; \
		echo "             Port 2 (Scoot) = W A S D + Left Ctrl (fire)"; \
		echo "Rebind under Settings → Input devices → Joystick settings."; \
		echo "To stop VICE later:  make kill"; \
		echo "Log file: $(LOG)"; \
	else \
		echo "error: VICE failed to start; see $(LOG) for details" >&2; \
		exit 1; \
	fi

# ============================================================
# kill — stop any detached VICE
# ============================================================
kill:
	@# pgrep -f uses ERE: + is a quantifier, so the regex must escape it.
	@# x64(sc)? matches both "x64" and "x64sc".  The process is launched
	@# with the literal argument string `+confirmonexit`.
	@pids=$$(pgrep -f 'x64(sc)? \+confirmonexit.*nyuller\.d64' 2>/dev/null || true); \
	if [ -n "$$pids" ]; then \
		echo "killing VICE process(es): $$pids"; \
		kill $$pids 2>/dev/null || true; \
		sleep 1; \
		kill -9 $$pids 2>/dev/null || true; \
	else \
		echo "no VICE process running (autostart of nyuller.d64)"; \
	fi
	@rm -f "$(PID_FILE)" "$(LOG)"

# ============================================================
# $(D64) — build the .d64 disk image from the .prg
# ============================================================
# c1541's exit code is unreliable (the interactive form always
# exits 0 even on failure), so the only reliable success signal
# is listing the disk afterwards and checking the PRG is there.
# The flag-form `c1541 -list image` doesn't work (c1541 treats
# it as a unit number), so we use the interactive heredoc form.
# All c1541 calls are wrapped in `|| true` so the final
# list-and-grep is the authoritative check.
$(D64): $(PRG) | ensure-build-dir
	@command -v c1541 >/dev/null 2>&1 || { echo "error: c1541 not found" >&2; exit 1; }
	@echo "wrapping $(PRG) in $(D64) (VICE autostart needs a disk image)..."
	@c1541 -format ny,of d64 "$(D64)" >/dev/null 2>&1 || true
	@echo "write $(PRG)" | c1541 "$(D64)" >/dev/null 2>&1 || true
	@if printf "list\nquit\n" | c1541 "$(D64)" 2>/dev/null | grep -q "nyuller.prg"; then \
		:; \
	else \
		echo "error: failed to write $(PRG) to $(D64) (c1541 silently exited 0)" >&2; \
		rm -f "$(D64)"; \
		exit 1; \
	fi

# ============================================================
# clean
# ============================================================
clean:
	@rm -f "$(PRG)" "$(D64)" "$(BUILD_DIR)/nyuller.asm" "$(BUILD_DIR)/nyuller.map" \
		"$(BUILD_DIR)/nyuller.lbl" "$(BUILD_DIR)/nyuller.int" "$(BUILD_DIR)/nyuller.dbj" \
		"$(BUILD_DIR)/nyuller.csz" "$(PID_FILE)" "$(LOG)"
	@echo "cleaned build/"
