// audio.c — SID audio service for the Nyuller state machine.
//
// The audio layer is a thin state-driven service.  The game's
// per-state "enter" actions call audio_state_enter() to set up the
// SID for that state's cue, and the per-frame game_step() calls
// audio_state_step() to advance the cue's schedule.  A short fault
// stinger on voice 1 is also exposed via audio_play_fault_stinger()
// and cleans itself up over 10 frames via the per-frame tick.
//
// **SID cue table (from GAME.md §6).**
//
//   STATE_TITLE     silent (Phase 6 leaves TITLE silent; the optional
//                   low idle pulse from the spec is not implemented).
//   STATE_READY     voice 0 triangle A4 (8 frames) -> A5 (8 frames),
//                   attack=0, decay=9, no sustain, release=0.
//   STATE_WAIT      voice 0 square C2 + voice 1 triangle C3 (1 octave
//                   up), both with attack=0, decay=15, sustain=8,
//                   release=15.  Voice 0 retriggered every 16 frames,
//                   voice 1 every 16 frames offset by 8 (so voice 1
//                   sounds on frames 8-15, 24-31, ...).
//   STATE_DRAW      voice 2 noise, attack=0, decay=1, sustain=0,
//                   release=2; gated on for 4 frames.
//   STATE_WIN_*     voice 0 triangle C5-E5-G5-C6 arpeggio, 11 frames
//                   per note (~0.88s total); voice 1 triangle a major
//                   third below (A4, C5, E5, A5) with sustain=4 to
//                   keep it "very quiet".
//   STATE_GAMEOVER  voice 0 triangle G4-C5-E5-G5-C6 fanfare, 31
//                   frames per note (~3.1s total); voice 1 sustained C4
//                   (a perfect fifth below G4) throughout.
//
// **No leakage between states.**  audio_state_enter() calls
// audio_stop() first, then sets up the new state.  This is critical
// for the WAIT -> DRAW transition: WAIT's looping voice 0/1 must
// stop before DRAW's voice 2 noise stab starts.
//
// **Fault stinger.**  A 10-frame low square-wave burst on voice 1
// (SID_FREQ_PAL(100), sustain=15).  Triggered from game_step_draw()
// AFTER game_enter_title() (so the state transition doesn't silence
// it).  Cleans itself up via audio_state_step().
//
// **Transition stinger.**  A 5-frame low square-wave burst played on
// every state transition.  Uses audio_play_stinger(voice, duration)
// with a per-state voice chosen to avoid colliding with the new
// state's audio (see the comments in game.c for the per-state voice
// mapping).  Also cleans itself up via audio_state_step().

#include "audio.h"
#include "notes.h"
#include <c64/sid.h>
#include <c64/types.h>

// --- internal state -----------------------------------------------------

// Current audio state.  Set by audio_state_enter() and matched
// against the state argument of audio_state_step().  0xff = "none".
#define AUDIO_STATE_NONE   0xff
static byte audio_state;

// Per-state frame counter.  Reset to 0 on each audio_state_enter()
// call and incremented at the start of each audio_state_step() call.
// 16 bits is enough for the longest cue (GAMEOVER is 155 frames).
static unsigned short audio_step;

// Fault stinger counter.  > 0 means the stinger is currently playing
// on voice 1; audio_state_step() decrements it once per frame and
// gates voice 1 off when it hits 0.  Also cancelled by
// audio_state_enter() so the stinger doesn't bleed into a new state
// that uses voice 1 (WAIT, WIN, GAMEOVER).
static byte fault_stinger_ticks;

// Generic stinger counter.  Set by audio_play_stinger(voice, duration);
// cleared by audio_state_enter() (so a transition stinger doesn't
// leak into the next state).  audio_state_step() decrements it once
// per frame and gates the stinger's voice off when it hits 0.
static byte stinger_ticks;
static byte stinger_voice;

// --- public API ---------------------------------------------------------

void audio_init(void)
{
	// Master volume 15, no filter modes, no voice filtering.  Filter
	// cutoff + resonance are also zeroed so no surprises from reset
	// state.  See SID register map at $D415-$D418.
	sid.ffreq = 0;
	sid.resfilt = 0;
	sid.fmodevol = 0x0f;

	audio_state = AUDIO_STATE_NONE;
	audio_step = 0;
	fault_stinger_ticks = 0;
	stinger_ticks = 0;
	stinger_voice = 0;

	// Silence all 3 voices to clear any leftover SID state.
	audio_stop();

	// The game uses the SID's voice-3 oscillator as its random
	// number source (read via $D41B in game.c).  audio_stop() above
	// wrote freq=0 to all 3 voices, which leaves the oscillator
	// stopped and $D41B stuck at 0 — so the first round of every
	// session would always get the same (deterministic) WAIT
	// duration.  Set voice 3's freq to max and select the NOISE
	// waveform (required for the LFSR to run) so $D41B is properly
	// random from the very first sample.  No GATE bit → silent.
	sid.voices[2].freq = 0xffff;
	sid.voices[2].ctrl = SID_CTRL_NOISE;
}

void audio_stop(void)
{
	// Gate off voices 0-1 and clear their registers.  Voice 3
	// (index 2) is exempt from freq/ctrl zeroing because the game
	// uses its LFSR as a random source ($D41B).  Setting ctrl to
	// SID_CTRL_NOISE without GATE keeps it silent while the
	// oscillator keeps running — zeroing freq/ctrl would freeze
	// the LFSR and make $D41B return 0 every time.
	sid.voices[0].ctrl = 0;
	sid.voices[1].ctrl = 0;
	sid.voices[2].ctrl = SID_CTRL_NOISE;
	sid.voices[0].attdec = 0;
	sid.voices[1].attdec = 0;
	sid.voices[2].attdec = 0;
	sid.voices[0].susrel = 0;
	sid.voices[1].susrel = 0;
	sid.voices[2].susrel = 0;
	sid.voices[0].freq = 0;
	sid.voices[1].freq = 0;
	sid.voices[2].freq = 0xffff;
	sid.voices[0].pwm = 0;
	sid.voices[1].pwm = 0;
	sid.voices[2].pwm = 0;
}

// --- internal helpers ---------------------------------------------------

// Configure a voice's freq/ADSR/PWM/waveform but do NOT gate on.  The
// caller is responsible for setting SID_CTRL_GATE when the note
// should sound.  This split lets us write a voice's parameters
// without producing an audible click (the gate stays off until
// everything else is set up).
static void audio_setup_voice(int v, unsigned freq, byte waveform,
                              byte attdec, byte susrel, unsigned pwm)
{
	sid.voices[v].freq = freq;
	sid.voices[v].pwm = pwm;
	sid.voices[v].attdec = attdec;
	sid.voices[v].susrel = susrel;
	sid.voices[v].ctrl = waveform;
}

// Gate a voice on (re-trigger the ADSR envelope).  The voice's
// waveform must already be selected (audio_setup_voice does this).
static void audio_gate_on(int v, byte waveform)
{
	sid.voices[v].ctrl = waveform;
	sid.voices[v].ctrl = waveform | SID_CTRL_GATE;
}

// Gate a voice off (start the release phase of the envelope).
static void audio_gate_off(int v, byte waveform)
{
	sid.voices[v].ctrl = waveform;
}

// Per-frame fault stinger tick.  Decrements fault_stinger_ticks; when
// it hits 0, gate voice 1 off.  Cheap to call when no stinger is
// active (single byte compare, branch not taken).
static void audio_advance_fault_stinger(void)
{
	if (fault_stinger_ticks > 0) {
		fault_stinger_ticks--;
		if (fault_stinger_ticks == 0)
			sid.voices[1].ctrl = SID_CTRL_RECT;
	}
}

// Per-frame transition-stinger tick.  Same shape as the fault
// stinger tick, but operates on the voice picked by
// audio_play_stinger().  Runs independently of the fault stinger
// (the two can overlap, e.g. a state transition that happens to
// coincide with a fault).
static void audio_advance_stinger(void)
{
	if (stinger_ticks > 0) {
		stinger_ticks--;
		if (stinger_ticks == 0)
			sid.voices[stinger_voice].ctrl = SID_CTRL_RECT;
	}
}

void audio_play_fault_stinger(void)
{
	// Low square wave on voice 1.  ~100 Hz is in the "low buzz"
	// range, not a musical note.  Attack=0, decay=0, sustain=15
	// (max), release=0 means: the note is at full volume the
	// instant the gate goes on and stays at full volume until gated
	// off.  The 10-frame duration is set here; audio_state_step()
	// will gate it off.
	sid.voices[1].freq = SID_FREQ_PAL(100);
	sid.voices[1].pwm = 0x0800;
	sid.voices[1].attdec = SID_ATK_2;
	sid.voices[1].susrel = 0xf0;
	sid.voices[1].ctrl = SID_CTRL_RECT | SID_CTRL_GATE;
	fault_stinger_ticks = 10;
}

void audio_play_stinger(int voice, int duration)
{
	// Brief low square-wave stinger on the given voice.  Used on
	// every state transition (5 frames = 0.1 sec at 50 Hz).  The
	// voice must be chosen to avoid colliding with the new state's
	// audio (see game.c for the per-state voice mapping).  Cleans
	// itself up via audio_advance_stinger() (called from
	// audio_state_step()).
	//
	// Will be silenced by the NEXT audio_state_enter() call (i.e.
	// the next state transition) via the audio_stop() it does
	// internally, so a long stinger can't bleed across states.
	sid.voices[voice].freq = SID_FREQ_PAL(100);
	sid.voices[voice].pwm = 0x0800;
	sid.voices[voice].attdec = SID_ATK_2;
	sid.voices[voice].susrel = 0xf0;
	sid.voices[voice].ctrl = SID_CTRL_RECT | SID_CTRL_GATE;
	stinger_voice = (byte)voice;
	stinger_ticks = (byte)duration;
}

void audio_state_enter(int state)
{
	// First silence the SID — this is what prevents the WAIT loop
	// from leaking into DRAW, and what cancels a mid-play fault
	// stinger when transitioning to a state that uses voice 1.
	audio_stop();
	fault_stinger_ticks = 0;
	stinger_ticks = 0;
	audio_step = 0;
	audio_state = (byte)state;

	switch (state) {
		case STATE_TITLE:
			// Silent per the Phase 6 spec.  The fault stinger
			// (triggered separately by game.c after this call)
			// is the only audio that may play in TITLE.
			break;

		case STATE_READY:
			// Ping-ping: triangle A4 then A5, 8 frames each.
			// ADSR: attack=0 (2ms), decay=4 (~114ms), no
			// sustain, release=0 (6ms).  With sustain=0 the
			// envelope drops to 0 once decay finishes, but
			// audio_state_step() retriggers the gate every 8
			// frames so the second note plays.
			audio_setup_voice(0, NOTE_A4, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_114,
			                  0x00,
			                  0x0800);
			audio_gate_on(0, SID_CTRL_TRI);
			break;

		case STATE_WAIT:
			// Square C2 on voice 0, triangle C3 on voice 1.
			// ADSR: attack=0, decay=15 (24s), sustain=8
			// (half-volume hold), release=15.  Decay 15 is
			// intentionally very slow — between retriggers
			// (every 16 frames) the envelope barely drops, so
			// the cue sounds more like a sustained pair of
			// voices with a periodic retrigger than a true
			// "pulse-pulse" pattern.  The retrigger cadence
			// is what gives the suspense its rhythm.
			audio_setup_voice(0, NOTE_C2, SID_CTRL_RECT,
			                  SID_ATK_2 | SID_DKY_24000,
			                  (8 << 4) | 0x0f,
			                  0x0800);
			audio_gate_on(0, SID_CTRL_RECT);

			audio_setup_voice(1, NOTE_C3, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_24000,
			                  (8 << 4) | 0x0f,
			                  0x0800);
			// Voice 1 not gated yet — audio_state_step()
			// gates it on at audio_step == 8 (so it's offset
			// by half a cycle from voice 0).
			break;

		case STATE_DRAW:
			// Noise on voice 2.  The noise waveform's actual
			// pitch is taken from voice 3's oscillator
			// frequency (a SID hardware quirk); we set the
			// freq to max for a "hissy" stab.  ADSR:
			// attack=0, decay=0 (6ms), sustain=0, release=2.
			// Sustain=0 means the envelope drops to 0 almost
			// immediately, but the gate is held on for 4
			// frames so the noise is at full volume the whole
			// time — the "sharp stab" is the 4-frame gate,
			// not the ADSR.
			audio_setup_voice(2, 0xffff, SID_CTRL_NOISE,
			                  SID_ATK_2 | SID_DKY_6,
			                  0x02,
			                  0x0800);
			audio_gate_on(2, SID_CTRL_NOISE);
			break;

		case STATE_WIN_P1:
		case STATE_WIN_P2:
			// Voice 0: triangle C5-E5-G5-C6 arpeggio.
			// Voice 1: triangle a major third below each
			// note (A4, C5, E5, A5) with sustain=4 to make
			// it "very quiet" per the spec.
			// ADSR: attack=0, decay=9 (~114ms), sustain=15
			// (voice 0) or 4 (voice 1), release=15.  Note
		// durations are 11 frames per note — total 44
		// frames = 0.88s at 50 Hz.
			audio_setup_voice(0, NOTE_C5, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_114,
			                  (15 << 4) | 0x0f,
			                  0x0800);
			audio_gate_on(0, SID_CTRL_TRI);

			audio_setup_voice(1, NOTE_A4, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_114,
			                  (4 << 4) | 0x0f,
			                  0x0800);
			audio_gate_on(1, SID_CTRL_TRI);
			break;

		case STATE_GAMEOVER:
			// Voice 0: triangle G4-C5-E5-G5-C6 fanfare.
			// Voice 1: sustained C4 (a perfect fifth below
		// G4) throughout.  Note durations are 31 frames
		// — total 155 frames = 3.1s at 50 Hz.
			// ADSR: attack=0, decay=9, sustain=15, release=15
			// (a long held chord that rings out).
			audio_setup_voice(0, NOTE_G4, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_114,
			                  (15 << 4) | 0x0f,
			                  0x0800);
			audio_gate_on(0, SID_CTRL_TRI);

			audio_setup_voice(1, NOTE_C4, SID_CTRL_TRI,
			                  SID_ATK_2 | SID_DKY_114,
			                  (15 << 4) | 0x0f,
			                  0x0800);
			audio_gate_on(1, SID_CTRL_TRI);
			break;
	}
}

void audio_state_step(int state)
{
	// Per-frame work that must happen regardless of state: tick
	// down the fault-stinger counter and the transition-stinger
	// counter.  Doing this here (not in any per-state step) means
	// the stingers clean up even if we transition out of the
	// current state during the stinger's window.
	audio_advance_fault_stinger();
	audio_advance_stinger();

	// If the audio layer hasn't been initialized for this state
	// yet (e.g. audio_state_enter() wasn't called between
	// game_step() and audio_state_step()), skip the per-state
	// schedule.  This shouldn't happen in normal flow but is a
	// safety net.
	if (state != audio_state)
		return;

	audio_step++;

	switch (state) {
		case STATE_TITLE:
			// Silent
			break;

		case STATE_READY:
			// Frame 8: switch A4 -> A5
			// Frame 16: gate off (end of cue)
			if (audio_step == 8) {
				audio_gate_off(0, SID_CTRL_TRI);
				sid.voices[0].freq = NOTE_A5;
				audio_gate_on(0, SID_CTRL_TRI);
			} else if (audio_step == 16) {
				audio_gate_off(0, SID_CTRL_TRI);
			}
			break;

		case STATE_WAIT:
			// Looping retrigger.  The cue is entered with
			// voice 0 already gated on (frame 0 of the
			// schedule) and voice 1 gated off.  Every 16
			// frames we retrigger voice 0; at the half-way
			// mark (mod 16 == 8) we retrigger voice 1.
			// That gives 16-frame cycles of:
			//   frames 0-7   : voice 0 only
			//   frames 8-15  : voice 0 + voice 1
			// repeating until the state exits.
			if ((audio_step & 15) == 0) {
				audio_gate_off(0, SID_CTRL_RECT);
				sid.voices[0].freq = NOTE_C2;
				audio_gate_on(0, SID_CTRL_RECT);
			}
			if ((audio_step & 15) == 8) {
				audio_gate_off(1, SID_CTRL_TRI);
				sid.voices[1].freq = NOTE_C3;
				audio_gate_on(1, SID_CTRL_TRI);
			}
			break;

		case STATE_DRAW:
			// Gate off voice 2 after 4 frames.  The cue
			// is entered with voice 2 already gated on
			// (frame 0), so the first 4 frames of the
			// state have noise; from frame 4 onward it's
			// silent (gate off, release phase is short).
			if (audio_step >= 4) {
				audio_gate_off(2, SID_CTRL_NOISE);
			}
			break;

		case STATE_WIN_P1:
		case STATE_WIN_P2:
		// Arpeggio every 11 frames.  Both voices
		// advance in lockstep.  Voice 0 plays the
		// melody (C5, E5, G5, C6); voice 1 plays
		// the same notes a major third below
		// (A4, C5, E5, A5).  At frame 44 we gate
		// both off (end of cue).
		if (audio_step == 11) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_E5;
			audio_gate_on(0, SID_CTRL_TRI);
			audio_gate_off(1, SID_CTRL_TRI);
			sid.voices[1].freq = NOTE_C5;
			audio_gate_on(1, SID_CTRL_TRI);
		} else if (audio_step == 22) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_G5;
			audio_gate_on(0, SID_CTRL_TRI);
			audio_gate_off(1, SID_CTRL_TRI);
			sid.voices[1].freq = NOTE_E5;
			audio_gate_on(1, SID_CTRL_TRI);
		} else if (audio_step == 33) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_C6;
			audio_gate_on(0, SID_CTRL_TRI);
			audio_gate_off(1, SID_CTRL_TRI);
			sid.voices[1].freq = NOTE_A5;
			audio_gate_on(1, SID_CTRL_TRI);
		} else if (audio_step >= 44) {
			audio_gate_off(0, SID_CTRL_TRI);
			audio_gate_off(1, SID_CTRL_TRI);
		}
			break;

		case STATE_GAMEOVER:
		// Fanfare every 31 frames on voice 0; voice 1
		// sustains C4 throughout the entire cue and is
		// only gated off at the end (frame 155+).
		if (audio_step == 31) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_C5;
			audio_gate_on(0, SID_CTRL_TRI);
		} else if (audio_step == 62) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_E5;
			audio_gate_on(0, SID_CTRL_TRI);
		} else if (audio_step == 93) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_G5;
			audio_gate_on(0, SID_CTRL_TRI);
		} else if (audio_step == 124) {
			audio_gate_off(0, SID_CTRL_TRI);
			sid.voices[0].freq = NOTE_C6;
			audio_gate_on(0, SID_CTRL_TRI);
		} else if (audio_step >= 155) {
			audio_gate_off(0, SID_CTRL_TRI);
			audio_gate_off(1, SID_CTRL_TRI);
		}
			break;
	}
}
