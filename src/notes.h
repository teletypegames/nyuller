#ifndef NYULLER_NOTES_H
#define NYULLER_NOTES_H

// notes.h — 16-bit SID frequency values for the musical notes used by
// the game.  PAL C64 (985248 Hz system clock); see the oscar64 sid.h
// header for the formula.  The values below are pre-computed with
// the SID_FREQ_PAL() macro (i.e. note_hz * 1115974 / 65536, rounded
// to nearest int).
//
// Equal-tempered scale, A4 = 440 Hz reference.  Only the notes we
// actually play are listed — the game uses ~9 specific notes (plus a
// few helpers for the WIN voice-2 thirds and the GAMEOVER voice-2
// fifth), so a flat list is more readable than a full octave table.

// Octave 1
#define NOTE_G1     834       // 49.00 Hz

// Octave 2
#define NOTE_C2     1114      // 65.41 Hz (WAIT voice 0 square)

// Octave 3
#define NOTE_C3     2227      // 130.81 Hz (WAIT voice 1 triangle, 1 octave up)

// Octave 4
#define NOTE_C4     4455      // 261.63 Hz (GAMEOVER voice 1 fifth below)
#define NOTE_G4     6675      // 392.00 Hz (GAMEOVER voice 0 fanfare start)
#define NOTE_A4     7493      // 440.00 Hz (READY A4, WIN voice 1 third below C5)

// Octave 5
#define NOTE_C5     8909      // 523.25 Hz (WIN voice 0 arpeggio start)
#define NOTE_E5     11227     // 659.25 Hz
#define NOTE_G5     13350     // 783.99 Hz
#define NOTE_A5     14985     // 880.00 Hz (READY A5, WIN voice 1 third below C6)

// Octave 6
#define NOTE_C6     17821     // 1046.50 Hz (WIN arpeggio top note)

#endif
