#ifndef WHACK_HARE_GAME_H
#define WHACK_HARE_GAME_H

// game.h — Whack Hare! state machine.
//
// States (see game.c for the per-state behavior and the transition
// diagram from GAME.md §9):
//
//   STATE_TITLE     — title screen, flashing border, wait for both fire
//   STATE_READY     — "ready" screen, 60 frames (1.2 s at 50 Hz)
//   STATE_WAIT      — "wait" screen, 100 frames (2 s; Phase 5: random)
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
//   WAIT     --100 frames------------------->  DRAW
//   DRAW     --port 1 fire (rising edge)--->  WIN_P1
//   DRAW     --port 0 fire (rising edge)--->  WIN_P2
//   DRAW     --500 frames (fault)---------->  TITLE
//   WIN_P1   --100 frames + score < 5----->  READY
//   WIN_P1   --100 frames + score == 5---->  GAMEOVER
//   WIN_P2   --100 frames + score < 5----->  READY
//   WIN_P2   --100 frames + score == 5---->  GAMEOVER
//   GAMEOVER --300 frames (scores reset)-->  TITLE

#define STATE_TITLE     0
#define STATE_READY     1
#define STATE_WAIT      2
#define STATE_DRAW      3
#define STATE_WIN_P1    4
#define STATE_WIN_P2    5
#define STATE_GAMEOVER  6

// game_init() — set up the state machine and enter the TITLE state.
// Call once at startup, after memmap_setup() and score_init().
// Calls show_screen(SCREEN_TITLE) and score_render() as part of the
// TITLE entry action.
void game_init(void);

// game_step() — advance the state machine by one frame.
//
// In Phase 4 the main loop calls this in a tight busy-wait.  In
// Phase 5 a raster IRQ handler at line 311 will call it at 50 Hz.
// Per-state step actions read both fire buttons, advance the per-state
// frame counter, and trigger state transitions (each transition calls
// the new state's "enter" action immediately, so the next step
// operates on the new state).
//
// game_step() also drives per-state visual updates that need to run
// every frame (currently just the TITLE border flash at 25 Hz).
void game_step(void);

#pragma compile("game.c")

#endif
