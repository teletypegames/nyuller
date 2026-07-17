# Known Issues

Issues found during the 7-agent code review that are deferred (not
fixed in the current build) because they require a more substantial
refactor or only matter on real hardware.

## 1. LZO decompression runs inside the raster IRQ (Review 4)

**Symptom:** On every state transition that loads an LZO-compressed
screen (TITLE, READY→WAIT, WAIT→DRAW is white, DRAW→WIN, WIN→READY,
GAMEOVER), the user sees a ~170 ms screen freeze with partial-update
tearing. About 8-9 frames worth of raster time.

**Cause:** `show_screen()` (in `screens.c`) calls
`oscar_expand_lzo((char *)0xE000, …)` for 8 000 output bytes, which
takes ~170 000 cycles at 1 MHz. The raster IRQ handler at line 311
(`src/tick.c:78-82`) calls `game_step()` which dispatches to
`game_enter_X()` which calls `show_screen()`. The VIC is mid-scanout
and the bitmap is being overwritten live.

**Workarounds considered:**
- Pre-decompress during the long READY/WAIT dwells (would require
  predicting the next state)
- Atomic memcpy in VBlank (8000 bytes at 1 byte/9 cycles = 72 000
  cycles; doesn't fit in 1575-cycle VBlank)
- Pending-flag pattern with main loop (would introduce a 1-frame
  visual lag between state change and screen update)
- Move the entire state machine to main loop, keep only frame_count
  in IRQ (loses audio cue precision)

**Decision:** Accepted for now. The visible tearing is annoying but
not a playability blocker; the game still works correctly. The
deferred-swap refactor is a self-contained follow-up commit.

## 2. IRQ half of `mmap_trampoline()` is silently disabled (Review 1)

**Symptom:** The mmap_trampoline writes `IRQTrampoline` to
`$FFFE/$FFFF` and `NMITrampoline` to `$FFFA/$FFFB`. But
`rirq_init(false)` in `src/tick.c:100` overwrites `$FFFE/$FFFF`
with `rirq_isr_ram_io` (per `oscar64/include/c64/rasterirq.c:572-578`).
So the IRQ half of the trampoline is dead code; only the NMI half
remains.

**Why it works anyway:** `src/tick.c:92-95` masks both CIAs
(`cia1.icr = 0x7f; cia2.icr = 0x7f;`), so the only IRQ source is
the raster. `rirq_isr_ram_io` correctly acks the raster and runs
`frame_tick_handler`. The trampoline is never needed.

**Latent risk:** If anyone ever un-masks a CIA source (e.g. to use
CIA 1 Timer A for the jiffy clock), a stray CIA IRQ will land in
`rirq_isr_ram_io` which only acks `$D019` and returns — it will
spin re-entering on the next CIA edge. The fix would be to
re-install the trampoline after `rirq_init()`.

**Decision:** Documented. No code change.

## 3. Audio schedule is 1 frame short per note (Reviews 2, audio review)

**Symptom:** WIN cue plays 4 notes × 9 frames = 36 frames = 0.72 s
(spec says 4 × 10 = 0.80 s). GAMEOVER plays 5 notes × 29 frames =
2.90 s (spec says 5 × 30 = 3.00 s). About 10% shorter per note.

**Cause:** `audio_state_step()` does `audio_step++` at the *start* of
the function, so the first call to step (in the same IRQ as the state
enter) has `audio_step == 1`. The schedule uses `if (audio_step ==
10) …` to trigger the next note, which fires on the 10th call (frame
11 of the state), not the 10th frame.

**Verified unaffected:** READY (8 frames per note — the schedule
checks `== 8` and `== 16` which line up correctly with 8+8=16
total), DRAW noise (4 frames — checks `>= 4` which also lines up
correctly), WAIT retrigger cadence (the `mod 16` checks produce the
right 16-frame cycles).

**Decision:** Cosmetic. Not worth the schedule risk of changing
`audio_step++` to the end of the function (would need to verify
all 5 state schedules still work).

## 4. `docs/c64/vic/graphics_modes.md` has wrong color mapping (Review 7)

**Symptom:** The local doc (line 52) says multicolor bitmap mode
"01" pixels use `$D022` and "10" pixels use `$D023`. The C64
hardware (per cebix-vic-article §3.7.3.4) actually uses the high /
low nibble of the cell's screen memory byte for "01" / "10"
respectively. `$D022` and `$D023` are not used at all in multicolor
bitmap mode.

**Impact:** None on runtime — the code in `src/screens.c` and the
Python in `tools/convert_screens.py` both follow the correct
hardware mapping. Only future developers reading the local doc
would be misled. The same doc also has the wrong mapping for
multicolor CHARACTER mode on line 40 (not used by this project).

**Decision:** Doc-only fix. Deferred to a doc-cleanup commit.

## 5. First-round WAIT was always 100 frames (Reviews 2, fixed)

Fixed in the same review-driven commit: `audio_init()` now sets
`sid.voices[2].freq = 0xffff` after `audio_stop()` so the SID
oscillator-3 (used as a random number source via `$D41B`) is
running from the very first sample.

## 6. DRAW state "instant win" on held fire (Reviews 2, 5, fixed)

Fixed in the same review-driven commit: `game_enter_draw()` now
initializes `draw_was_pressed[]` from the current input state
(via `input_fire()`), not from zero, so a player holding fire
from WAIT into DRAW does NOT auto-win on the first frame.

## 7. `.attr` tables were in BASIC ROM space (Review 1, fixed)

Fixed in the same review-driven commit: the `screens` region moved
from `$A000-$C000` (overlapping the banked-in BASIC ROM) to
`$BC00-$D000` (always-RAM under the banked-out KERNAL and below
the I/O area).

## 8. Build script bugs (Review 6, fixed)

Fixed in the same review-driven commit:
- `--kill` pgrep regex was broken (unescaped `+`, missing group).
- `-v -p` (or `-V -P`) would launch two VICE processes — now
  rejected with a clear error.
- `c1541` exits 0 even on failure, so `set -e` couldn't catch
  a failed `.d64` build — now verified with a `c1541 -list` check.
- `--kill` now also removes the stale `vice.pid` and `vice.log`.
- `-p` / `-P` now kill any prior VICE process before launching
  a new one.
