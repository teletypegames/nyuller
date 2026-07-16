// game.c — Whack Hare! state machine.
//
// See game.h for the transition diagram and game_init() / game_step()
// documentation.  Each state has an "enter" action (set the screen,
// reset the frame counter, reset per-state state) and a "step" action
// (check inputs, advance the frame counter, transition).
//
// In Phase 4 the main loop is `while (1) game_step();`.  In Phase 5
// the body of the main loop will be replaced by a raster IRQ handler
// that calls game_step() on line 311.  Either way, game_step() is the
// only thing that needs to run "once per frame".

#include "game.h"
#include "screens.h"
#include "input.h"
#include "score.h"
#include <c64/vic.h>

// Current state.  Set by game_init() and by the per-state step
// functions when a transition is triggered.  Read by the switch in
// game_step().
static byte state;

// Frame counter for the current state.  Reset to 0 by each per-state
// "enter" function.  Compared against per-state constants in the step
// functions (60 for READY, 100 for WAIT and WIN, 300 for GAMEOVER,
// 500 for the DRAW fault timeout).  unsigned short is enough for
// 65535 frames = ~22 minutes at 50 Hz; the longest single state is
// 500 frames (DRAW fault), so 16 bits is more than enough.
static unsigned short frame;

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

// --- per-state enter functions -----------------------------------------
//
// Each "enter" function:
//   - calls show_screen() (or show_white_screen() for DRAW)
//   - calls score_render()
//   - resets `frame` to 0
//   - resets any per-state state (e.g. title_input, draw_was_pressed)
//   - sets the border color (TITLE flashes it; DRAW is all-white;
//     other states are black)

static void game_enter_title(void)
{
    show_screen(SCREEN_TITLE);
    score_render();
    frame = 0;
    title_input = TITLE_IDLE;
    title_first_frame = 0;
    // Border starts white (the "PRESS FIRE" prompt is visible).
    vic.color_border = 1;
}

static void game_enter_ready(void)
{
    show_screen(SCREEN_WAITING1);
    score_render();
    frame = 0;
    vic.color_border = 0;
}

static void game_enter_wait(void)
{
    show_screen(SCREEN_WAITING2);
    score_render();
    frame = 0;
    vic.color_border = 0;
}

static void game_enter_draw(void)
{
    show_white_screen();
    score_render();
    frame = 0;
    vic.color_border = 1;  // white border matches the white screen
    draw_was_pressed[0] = 0;
    draw_was_pressed[1] = 0;
}

static void game_enter_win_p1(void)
{
    show_screen(SCREEN_WIN_HARE);
    score_p1++;
    score_render();
    frame = 0;
    vic.color_border = 0;
}

static void game_enter_win_p2(void)
{
    show_screen(SCREEN_WIN_SCOOT);
    score_p2++;
    score_render();
    frame = 0;
    vic.color_border = 0;
}

static void game_enter_gameover(void)
{
    // Show the title screen, but with the scores reset to 0/0 (the
    // score bar makes this visually obvious: it's the title screen
    // with an empty score bar).
    show_screen(SCREEN_TITLE);
    score_p1 = 0;
    score_p2 = 0;
    score_render();
    frame = 0;
    vic.color_border = 0;
}

// --- per-state step functions ------------------------------------------

static void game_step_title(void)
{
    // Flash the border at 25 Hz (toggle every 2 frames at 50 Hz).
    // The visible effect is a 12.5 Hz blink on the border around the
    // title screen image.  Phase 8 will replace this with actual
    // "PRESS FIRE" text rendered into the bitmap.
    if ((frame & 1) == 0)
        vic.color_border ^= 1;

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
                title_first_frame = frame;
            }
            break;

        case TITLE_FIRST_HELD:
            if (both) {
                if (frame - title_first_frame <= 8) {
                    state = STATE_READY;
                    game_enter_ready();
                } else {
                    title_input = TITLE_WAIT_RELEASE;
                }
            } else if (none) {
                title_input = TITLE_IDLE;
            } else if (frame - title_first_frame > 8) {
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
    if (frame >= 60) {
        state = STATE_WAIT;
        game_enter_wait();
    }
}

static void game_step_wait(void)
{
    // Phase 4: fixed 100 frames.  Phase 5: random 100-250 from
    // PEEK(0xD41B) sampled on READY enter.
    if (frame >= 100) {
        state = STATE_DRAW;
        game_enter_draw();
    }
}

static void game_step_draw(void)
{
    // 500-frame fault timeout (10 sec at 50 Hz).  If neither player
    // fires in 10 sec, abort the round and go back to TITLE.
    if (frame > 500) {
        state = STATE_TITLE;
        game_enter_title();
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
    if (frame >= 100) {
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
    if (frame >= 100) {
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
    if (frame >= 300) {
        state = STATE_TITLE;
        game_enter_title();
    }
}

// --- public API --------------------------------------------------------

void game_init(void)
{
    state = STATE_TITLE;
    game_enter_title();
}

void game_step(void)
{
    frame++;

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
