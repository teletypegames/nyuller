#ifndef WHACK_HARE_DRAW_H
#define WHACK_HARE_DRAW_H

// draw.h — DRAW screen counter rendering.
//
// The DRAW screen is solid white (set up by show_white_screen).
// This header exposes the per-frame counter renderer, which writes
// 3 seven-segment-style digits (each 3 cells wide × 8 cells tall,
// 1 cell per segment) into the bitmap at row 8, columns 15-23.
//
// Public API:
//   draw_render_counter(value) — render the 0..999 value as a
//     3-digit counter on the white screen.  Values outside 0..999
//     are clamped.

void draw_render_counter(int value);

#pragma compile("draw.c")

#endif
