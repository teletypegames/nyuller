// game.c — Nyuller state machine.
//
// See game.h for the transition diagram and game_init() / game_step()
// documentation.  Each state has an "enter" action (set the screen,
// reset per-state state, sample any randomness) and a "step" action
// (check inputs, compute elapsed frames against enter_frame, decide
// whether to transition).
//
// In Phase 4 the main loop was `while (1) game_step();` and the
// per-state "frame counter" was just a count of how many times
// game_step had been called since entering the current state — so
// state durations were CPU-bound, not wall-clock-bound.
//
// In Phase 5 the busy-wait body of the main loop is gone.  Instead,
// a raster IRQ at line 311 (PAL stable line) runs at exactly 50 Hz
// and calls game_step() once per frame.  All state durations are
// now wall-clock-bound: a state that should last 60 frames lasts
// 60 × 20 ms = 1.2 seconds, regardless of what the CPU is doing
// between IRQs.  The 16-bit global frame_count (incremented by the
// IRQ handler *before* calling game_step) is the new "frame
// counter"; per-state timing uses an enter_frame timestamp captured
// on entry.

#include "game.h"
#include "audio.h"
#include "screens.h"
#include "input.h"
#include "score.h"
#include "banner.h"
#include "draw.h"
#include <c64/vic.h>
#include <c64/sid.h>

// --- globals ------------------------------------------------------------

// Current state.  Set by game_init() and by the per-state step
// functions when a transition is triggered.  Read by the switch in
// game_step().
static byte state;

// Per-state "entered at" timestamp.  Set by each per-state enter
// function to the current frame_count.  The per-state step functions
// compute elapsed = frame_count - enter_frame to decide when to
// transition.  16 bits is enough for 65535 frames = ~22 minutes at
// 50 Hz; the longest single state is 500 frames (DRAW fault), so 16
// bits is more than enough for a single state.
static unsigned short enter_frame;

// WAIT random duration (in frames).  Sampled in game_enter_ready from
// SID $D41B (the oscillator 3 register, effectively random).
// 100 + (sid.random % 150) gives a 2.0-5.0 second suspense window
// at 50 Hz.  The randomness is sampled at READY enter (not WAIT
// enter) so the value is stable for the duration of READY (and the
// subsequent WAIT).
static unsigned short wait_duration_frames;

// --- TITLE-specific input state ----------------------------------------
//
// The TITLE state waits for "both fire buttons pressed simultaneously".
// The de-bounce rule (GAME.md §9): the two buttons must be pressed
// within 8 frames of each other; if not, the game requires both
// buttons to be released before re-trying.  The protocol:
//
//   IDLE          -> on (both pressed at once)         -> READY
//                 -> on (one pressed)                   -> FIRST_HELD
//   FIRST_HELD    -> on (other pressed, within 8 fr)   -> READY
//                 -> on (timeout reached)               -> WAIT_RELEASE
//                 -> on (both released)                 -> IDLE
//   WAIT_RELEASE  -> on (both released)                 -> IDLE
#define TITLE_IDLE          0
#define TITLE_FIRST_HELD    1
#define TITLE_WAIT_RELEASE  2
static byte title_input;
static unsigned short title_first_frame;

// --- DRAW-specific input state -----------------------------------------
//
// The DRAW state needs rising-edge detection: holding fire from before
// DRAW starts (e.g. during WAIT) must not auto-trigger a win.  We
// track the previous frame's state for each port.  Initialized on
// entry to DRAW (in game_enter_draw()).
static char draw_was_pressed[2];

// The DRAW screen's per-frame counter.  Reset to 0 in game_enter_draw
// (with an initial draw_render_counter(0) call to draw "000" on the
// freshly-cleared white screen), then incremented each frame in
// game_step_draw up to a cap of 999.  The 16-bit type is more than
// enough: we never store more than 999, and 999 fits in 10 bits.
static unsigned short draw_counter;

// --- fault stinger -----------------------------------------------------
//
// When the DRAW state's 500-frame fault timeout fires, game_step_draw
// transitions to TITLE and then calls audio_play_fault_stinger() to
// start a 10-frame low square-wave burst on SID voice 1.  The
// cleanup logic lives in audio.c (audio_advance_fault_stinger, called
// from audio_state_step).  See audio.c for details.

// --- transition stinger voice mapping ----------------------------------
//
// The state-transition stinger is a 5-frame low square wave played
// on every game_enter_X() call (after audio_state_enter() so the
// new state's audio isn't immediately silenced).  Each state picks
// a voice that is FREE in the new state so the stinger doesn't
// collide with the new state's main cue:
//
//   STATE_TITLE     -> voice 0  (TITLE is silent; voice 1+2 free)
//   STATE_READY     -> voice 1  (READY uses voice 0 only; voice 1+2 free)
//   STATE_WAIT      -> voice 0  (WAIT uses voice 0+1; voice 2 is the RNG source)
//   STATE_DRAW      -> voice 0  (DRAW uses voice 2 for noise; voice 0+1 free)
//   STATE_WIN_P1/P2 -> voice 2  (WIN uses voice 0+1; voice 2 free)
//   STATE_GAMEOVER  -> voice 2  (GAMEOVER uses voice 0+1; voice 2 free)
//
// (Fault stinger always uses voice 1; it fires only in TITLE so
// voice 1 is always free there.)
#define STINGER_DURATION	5

static byte stinger_voice_for_state(byte s)
{
	switch (s) {
		case STATE_TITLE:    return 0;
		case STATE_READY:    return 1;
		case STATE_WAIT:     return 0;
		case STATE_DRAW:     return 0;
		case STATE_WIN_P1:
		case STATE_WIN_P2:   return 2;
		case STATE_GAMEOVER: return 2;
		default:             return 0;
	}
}

// --- last winner --------------------------------------------------------
//
// Set by game_enter_win_p1/p2 to the winning player (1 or 2).  Used
// by game_enter_gameover to pick the "HARE WINS!" or "SCOOT WINS!"
// banner text.  Static because only game.c cares about it.
static byte last_winner;

// --- per-state enter functions -----------------------------------------
//
// Each "enter" function:
//   - calls show_screen() (or show_white_screen() for DRAW)
//   - calls score_render()
//   - sets enter_frame = frame_count (the per-state timestamp)
//   - resets any per-state state (e.g. title_input, draw_was_pressed)
//   - sets the border color (TITLE flashes it; DRAW is all-white;
//     other states are black)
//   - samples the SID random for WAIT duration (in READY enter, not
//     WAIT enter, per the Phase 5 spec)
//   - resets and renders the DRAW counter (DRAW only: draw_counter
//     = 0, draw_render_counter(0))
//   - calls audio_state_enter(state) to set up the SID for this
//     state's cue.  This is the last step so the previous state's
//     audio (if any) is still playing while we set up the screen,
//     and the new state's audio takes over atomically.
//
// The fault stinger (a 10-frame low square-wave burst on voice 1) is
// triggered from game_step_draw() AFTER game_enter_title() returns,
// not from game_enter_title() itself — otherwise audio_state_enter()
// would silence it immediately.

static void game_enter_title(void)
{
	show_screen(SCREEN_TITLE);
	score_p1 = 0;
	score_p2 = 0;
	score_render();
	// "PRESS FIRE" prompt: drawn in row 1 (below the score bar) of
	// the title screen.  Re-rendered on every TITLE entry so it
	// stays visible after any state that may have overwritten the
	// row (only GAMEOVER writes row 1, but show_screen() re-loads
	// the full title bitmap which overwrites it with the title
	// art — so we always re-render the prompt here).
	banner_render("PRESS FIRE", 1);
	enter_frame = frame_count;
	title_input = TITLE_IDLE;
	title_first_frame = 0;
	// Border starts white (the "PRESS FIRE" prompt is visible).
	// The per-frame flash in game_step_title() toggles it at 1 Hz
	// (50 frames on, 50 frames off).
	vic.color_border = 1;
	audio_state_enter(STATE_TITLE);
	// 5-frame transition stinger on voice 0 (free in TITLE).
	audio_play_stinger(stinger_voice_for_state(STATE_TITLE), STINGER_DURATION);
}

static void game_enter_ready(void)
{
	show_screen(SCREEN_WAITING1);
	score_render();
	// Row 1 of the waiting1 screen — clear any leftover banner.
	banner_clear(1);
	enter_frame = frame_count;
	// Sample SID oscillator 3 ($D41B) for the upcoming WAIT duration.
	// 100 + (sid.random % 151) frames = 2.0..5.0 sec at 50 Hz.  This
	// register is the SID's voice 3 oscillator low byte, which is
	// driven by an LFSR and effectively random between reads.
	wait_duration_frames = 100 + (sid.random % 151);
	vic.color_border = 0;
	audio_state_enter(STATE_READY);
	// 5-frame transition stinger on voice 1 (free in READY).
	audio_play_stinger(stinger_voice_for_state(STATE_READY), STINGER_DURATION);
}

static void game_enter_wait(void)
{
	show_screen(SCREEN_WAITING2);
	score_render();
	banner_clear(1);
	enter_frame = frame_count;
	vic.color_border = 0;
	audio_state_enter(STATE_WAIT);
	// 5-frame transition stinger on voice 2 (free in WAIT).
	audio_play_stinger(stinger_voice_for_state(STATE_WAIT), STINGER_DURATION);
}

static void game_enter_draw(void)
{
	show_white_screen();
	score_render();
	banner_clear(1);
	enter_frame = frame_count;
	vic.color_border = 1;  // white border matches the white screen
	draw_was_pressed[0] = input_fire(1);  // Hare — capture current state
	draw_was_pressed[1] = input_fire(0);  // Scoot — so holding from WAIT
	                                    //           doesn't auto-win
	// Reset the per-frame counter to 0 and render "000" once.
	// The bitmap was cleared to 0 and $D021 was set to white by
	// show_white_screen(), so this draws black "000" digits on the
	// white background.  game_step_draw() will then increment the
	// counter and re-render each frame.  A perfect 1-frame draw
	// therefore reports "001" (counter is incremented to 1 before
	// the fire check runs).
	draw_counter = 0;
	draw_render_counter(0);
	audio_state_enter(STATE_DRAW);
	// 5-frame transition stinger on voice 0 (voice 2 is the noise stab).
	audio_play_stinger(stinger_voice_for_state(STATE_DRAW), STINGER_DURATION);
}

static void game_enter_win_p1(void)
{
	show_screen(SCREEN_WIN_HARE);
	score_p1++;
	score_render();
	banner_clear(1);
	enter_frame = frame_count;
	vic.color_border = 0;
	last_winner = 1;
	audio_state_enter(STATE_WIN_P1);
	// 5-frame transition stinger on voice 2 (free in WIN).
	audio_play_stinger(stinger_voice_for_state(STATE_WIN_P1), STINGER_DURATION);
}

static void game_enter_win_p2(void)
{
	show_screen(SCREEN_WIN_SCOOT);
	score_p2++;
	score_render();
	banner_clear(1);
	enter_frame = frame_count;
	vic.color_border = 0;
	last_winner = 2;
	audio_state_enter(STATE_WIN_P2);
	// 5-frame transition stinger on voice 2 (free in WIN).
	audio_play_stinger(stinger_voice_for_state(STATE_WIN_P2), STINGER_DURATION);
}

static void game_enter_gameover(void)
{
	// Show the title screen with the final scores still displayed
	// (5 : x or x : 5) and the winner banner ("HARE WINS!" or
	// "SCOOT WINS!") in row 1 below the score bar.  Scores are not
	// reset here — they reset on the next TITLE entry.
	show_screen(SCREEN_TITLE);
	score_render();
	// "HARE WINS!" or "SCOOT WINS!" — driven by last_winner set in
	// game_enter_win_p1/p2.  show_screen(SCREEN_TITLE) just
	// reloaded the title bitmap, so row 1 currently has the title
	// art; banner_render() will clear and overwrite it.
	if (last_winner == 1)
		banner_render("HARE WINS!", 1);
	else
		banner_render("SCOOT WINS!", 1);
	enter_frame = frame_count;
	vic.color_border = 0;
	audio_state_enter(STATE_GAMEOVER);
	// 5-frame transition stinger on voice 2 (free in GAMEOVER).
	audio_play_stinger(stinger_voice_for_state(STATE_GAMEOVER), STINGER_DURATION);
}

// --- per-state step functions ------------------------------------------

static void game_step_title(void)
{
	// Flash the border at 1 Hz: 50 frames on, 50 frames off.
	// The full cycle is 100 frames = 2.0 sec, so the visible flash
	// rate is 0.5 Hz (one on-off per 2 sec).  Phase 8 task spec
	// calls this "1 Hz" but with explicit "50 on, 50 off" — the
	// parenthetical is the actual rate, and we follow it.
	unsigned short elapsed = frame_count - enter_frame;
	if ((elapsed % 100) < 50)
		vic.color_border = 1;  // white
	else
		vic.color_border = 0;  // black

	char p1 = input_fire(1);  // Hare (port 1)
	char p2 = input_fire(0);  // Scoot (port 0)
	char both = p1 && p2;
	char none = !p1 && !p2;

	switch (title_input) {
		case TITLE_IDLE:
			if (both) {
				state = STATE_READY;
				game_enter_ready();
			} else if (p1 || p2) {
				title_input = TITLE_FIRST_HELD;
				title_first_frame = frame_count;
			}
			break;

		case TITLE_FIRST_HELD:
			if (both) {
				if (frame_count - title_first_frame <= 8) {
					state = STATE_READY;
					game_enter_ready();
				} else {
					title_input = TITLE_WAIT_RELEASE;
				}
			} else if (none) {
				title_input = TITLE_IDLE;
			} else if (frame_count - title_first_frame > 8) {
				// Held too long without the other button following.
				title_input = TITLE_WAIT_RELEASE;
			}
			// else: one still held, other not yet, within 8 frames.
			break;

		case TITLE_WAIT_RELEASE:
			if (none) {
				title_input = TITLE_IDLE;
			}
			break;
	}
}

static void game_step_ready(void)
{
	if (frame_count - enter_frame >= 60) {
		state = STATE_WAIT;
		game_enter_wait();
	}
}

static void game_step_wait(void)
{
	// Random 100..250 frames, sampled in game_enter_ready.  We
	// intentionally do NOT read input here — pressing fire during
	// WAIT is "cheating" and the game ignores it (GAME.md §9).  The
	// DRAW state still does rising-edge detection, so any fire
	// pressed here is not remembered.
	if (frame_count - enter_frame >= wait_duration_frames) {
		state = STATE_DRAW;
		game_enter_draw();
	}
}

static void game_step_draw(void)
{
	unsigned short elapsed = frame_count - enter_frame;

	// Flash effect: the border $D020 strobes between black and white
	// for the first 4 frames of DRAW, then settles to white.  The
	// strobe creates a brief visible "flash" at the start of the
	// round (4 frames at 50 Hz = ~80 ms).  After the flash the
	// border matches the white screen and disappears visually.
	if (elapsed < 4)
		vic.color_border = (elapsed & 1) ? 0 : 1;
	else
		vic.color_border = 1;

	// Per-frame counter: increment (capped at 999) and re-render.
	// Drawn after the flash so the border strobe and the digit
	// update happen in the same IRQ; the next visible frame shows
	// both updates together.
	if (draw_counter < 999)
		draw_counter++;
	draw_render_counter(draw_counter);

	// 500-frame fault timeout (10 sec at 50 Hz).  If neither player
	// fires in 10 sec, abort the round, trigger a short stinger on
	// SID voice 1, and go back to TITLE.  No point awarded.
	if (elapsed >= 500) {
		state = STATE_TITLE;
		game_enter_title();
		// Trigger the stinger AFTER audio_state_enter(TITLE)
		// so it isn't silenced by the state transition.
		audio_play_fault_stinger();
		return;
	}

	// First to fire wins.  Rising-edge detection so holding fire from
	// before DRAW doesn't auto-trigger a win (e.g. if the player
	// presses during WAIT and keeps it held).  P1 wins ties (matches
	// the "fire1 first" branch in GAME.md §9).
	char now_p1 = input_fire(1);
	char now_p2 = input_fire(0);
	char edge_p1 = now_p1 && !draw_was_pressed[0];
	char edge_p2 = now_p2 && !draw_was_pressed[1];
	draw_was_pressed[0] = now_p1;
	draw_was_pressed[1] = now_p2;

	if (edge_p1) {
		state = STATE_WIN_P1;
		game_enter_win_p1();
	} else if (edge_p2) {
		state = STATE_WIN_P2;
		game_enter_win_p2();
	}
}

static void game_step_win_p1(void)
{
	if (frame_count - enter_frame >= 100) {
		if (score_p1 >= 5) {
			state = STATE_GAMEOVER;
			game_enter_gameover();
		} else {
			state = STATE_READY;
			game_enter_ready();
		}
	}
}

static void game_step_win_p2(void)
{
	if (frame_count - enter_frame >= 100) {
		if (score_p2 >= 5) {
			state = STATE_GAMEOVER;
			game_enter_gameover();
		} else {
			state = STATE_READY;
			game_enter_ready();
		}
	}
}

static void game_step_gameover(void)
{
	if (frame_count - enter_frame >= 300) {
		state = STATE_TITLE;
		game_enter_title();
	}
}

// --- public API --------------------------------------------------------

// Definition of the global frame_count declared in game.h.  Lives in
// BSS so it's zero at startup; the raster IRQ handler increments it.
volatile unsigned short frame_count;

void game_init(void)
{
	state = STATE_TITLE;
	// enter_frame is set by game_enter_title below; the very first
	// game_step() call happens in the same raster IRQ that
	// increments frame_count from 0 to 1, so enter_frame=0 there
	// is fine (elapsed = 1 on the first call).
	game_enter_title();
}

void game_step(void)
{
	// Advance the audio state machine.  audio_state_step() handles
	// the per-frame fault-stinger cleanup AND the per-state
	// schedule (READY ping-pong, WAIT loop, DRAW stab cleanup,
	// WIN arpeggio, GAMEOVER fanfare).  It must be called every
	// frame regardless of state, and after any state transition
	// that happened in this frame's per-state step (so the new
	// state's audio advances by one frame).
	audio_state_step(state);

	switch (state) {
		case STATE_TITLE:    game_step_title();    break;
		case STATE_READY:    game_step_ready();    break;
		case STATE_WAIT:     game_step_wait();     break;
		case STATE_DRAW:     game_step_draw();     break;
		case STATE_WIN_P1:   game_step_win_p1();   break;
		case STATE_WIN_P2:   game_step_win_p2();   break;
		case STATE_GAMEOVER: game_step_gameover(); break;
	}
}
