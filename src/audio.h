#ifndef NYULLER_AUDIO_H
#define NYULLER_AUDIO_H

// audio.h — SID audio service for the Nyuller state machine.
//
// The audio layer is a thin state-driven service: the game's per-state
// "enter" actions call audio_state_enter() to set up the SID for that
// state's cue, and the raster IRQ handler's per-frame tick calls
// audio_state_step() to advance the cue's schedule.
//
// audio_init() must be called once at startup, after memmap_setup()
// and before rasterirq_setup().  audio_play_fault_stinger() is called
// from the DRAW state's fault handler (in game.c) to start a short
// low square-wave stinger on voice 1; the stinger cleans itself up
// over the next 10 frames via audio_state_step().
//
// **No leakage between states.**  audio_state_enter() calls
// audio_stop() internally before setting up the new state, so the
// previous state's SID writes (e.g. the WAIT voice 0/1 loop) cannot
// bleed into the next state (e.g. DRAW's voice 2 noise stab).
//
// **The audio_state argument.**  The STATE_* constants are defined in
// game.h.  We include it here so audio_state_enter() and
// audio_state_step() can switch on the same values the game uses.

#include "game.h"

void audio_init(void);

// audio_stop() — silence all 3 SID voices and reset their registers.
// Called internally by audio_state_enter() and also available to the
// game for explicit silencing (currently unused at the game layer).
void audio_stop(void);

// audio_state_enter(state) — silence the SID, then set up the cue
// for the given state.  Resets the per-state schedule.  See audio.c
// for the per-state cue definitions.
void audio_state_enter(int state);

// audio_state_step(state) — advance the per-state schedule by one
// frame.  Called from game_step() once per frame.  Also ticks down
// the fault-stinger counter and gates voice 1 off when it expires.
void audio_state_step(int state);

// audio_play_fault_stinger() — start a 10-frame low square-wave
// stinger on voice 1.  Called from game_step_draw() when the DRAW
// fault timeout fires (after audio_state_enter(TITLE) so the stinger
// isn't immediately silenced by the state transition).
void audio_play_fault_stinger(void);

// audio_play_stinger(voice, duration) — start a low square-wave
// stinger on the given voice for the given number of frames.  Called
// on every state transition by game_enter_X() (5 frames = 0.1 sec).
// The voice must be chosen to avoid colliding with the new state's
// audio — see game.c for the per-state voice mapping.
void audio_play_stinger(int voice, int duration);

#pragma compile("audio.c")

#endif
