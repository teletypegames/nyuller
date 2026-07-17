#ifndef WHACK_HARE_GAME_H
#define WHACK_HARE_GAME_H

// game.h — Whack Hare! state machine.
//
// States (see game.c for the per-state behavior and the transition
// diagram from GAME.md §9):
//
//   STATE_TITLE     — title screen, flashing border, wait for both fire
//   STATE_READY     — "ready" screen, 60 frames (1.2 s at 50 Hz)
//   STATE_WAIT      — "wait" screen, 100..250 frames (random, from
//                     SID $D41B sampled on READY enter)
//   STATE_DRAW      — white screen, wait for first fire (or 500-frame
//                     fault timeout)
//   STATE_WIN_P1    — "Hare won" screen, 100 frames, +1 to player 1
//   STATE_WIN_P2    — "Scoot won" screen, 100 frames, +1 to player 2
//   STATE_GAMEOVER  — title screen with scores reset, 300 frames
//
// Transitions:
//
//   TITLE    --both fire (within 8 frames)-->  READY
//   READY    --60 frames-------------------->  WAIT
//   WAIT     --random 100..250 frames------>  DRAW
//   DRAW     --port 1 fire (rising edge)--->  WIN_P1
//   DRAW     --port 0 fire (rising edge)--->  WIN_P2
//   DRAW     --500 frames (fault)---------->  TITLE
//   WIN_P1   --100 frames + score < 5----->  READY
//   WIN_P1   --100 frames + score == 5---->  GAMEOVER
//   WIN_P2   --100 frames + score < 5----->  READY
//   WIN_P2   --100 frames + score == 5---->  GAMEOVER
//   GAMEOVER --300 frames (scores reset)-->  TITLE
//
// Timing: the per-state frame counter is replaced (Phase 5) by a
// 16-bit global frame_count incremented by the raster IRQ at 50 Hz
// (line 311).  Per-state step actions compute elapsed =
// frame_count - enter_frame to decide when to transition.

#include <c64/types.h>

#define STATE_TITLE     0
#define STATE_READY     1
#define STATE_WAIT      2
#define STATE_DRAW      3
#define STATE_WIN_P1    4
#define STATE_WIN_P2    5
#define STATE_GAMEOVER  6

// frame_count — global 50 Hz frame counter.  Incremented by the
// raster IRQ handler in rasterirq.c (see rasterirq.h) once per
// frame, *before* calling game_step().  So at the time game_step()
// runs, frame_count is the current frame number (1 on the first
// call after power-on, 2 on the second, etc.).
//
// Marked volatile because the IRQ handler is the writer and the
// game-step functions are the readers.  16 bits = 65535 frames =
// ~22 minutes at 50 Hz; the longest single state is 500 frames
// (DRAW fault), so 16 bits is plenty for one state.  We never read
// frame_count from the main loop (the main loop is `while (1) {}`
// in Phase 5) so torn-read races are not a concern.
extern volatile unsigned short frame_count;

// game_init() — set up the state machine and enter the TITLE state.
// Call once at startup, after memmap_setup() and score_init() and
// rasterirq_setup().  Calls show_screen(SCREEN_TITLE) and
// score_render() as part of the TITLE entry action.
void game_init(void);

// game_step() — advance the state machine by one frame.  Called from
// the raster IRQ handler at line 311.  Per-state step actions read
// both fire buttons, compute elapsed frames against the per-state
// enter_frame timestamp, and trigger state transitions (each
// transition calls the new state's "enter" action immediately, so
// the next step operates on the new state).
//
// game_step() also drives per-state visual updates that need to run
// every frame (the TITLE border flash at 1 Hz, the DRAW border
// flash + per-frame counter, and the fault-stinger audio gate-off
// counter).
//
// Marked __noinline so -O3 doesn't inline the whole state machine
// into the IRQ handler (which would make the handler thousands of
// bytes and overrun the raster line budget on real hardware).
__noinline void game_step(void);

#pragma compile("game.c")

#endif
