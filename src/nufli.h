// nufli.h — NUFLI image display with delta encoding.
#ifndef NYULLER_NUFLI_H
#define NYULLER_NUFLI_H

// Screen IDs
#define NUFLI_SCREEN_TITLE    0
#define NUFLI_SCREEN_WIN_HARE 1
#define NUFLI_SCREEN_WIN_SCOOT 2
#define NUFLI_SCREEN_GAMEOVER 3

// Display a NUFLI screen by applying its delta to the shared base.
// screen_id: 0=title, 1=win_hare, 2=win_scoot, 3=gameover
void nufli_show(int screen_id);

// Exit NUFLI mode and restore normal video.
void nufli_exit(void);

#pragma compile("nufli.c")

#endif
