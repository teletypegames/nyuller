// screens.c — multicolor bitmap screens for the WAIT states.
//
// The WAIT states are the only dynamic states that use the multicolor
// bitmap engine.  Static states (TITLE, READY, WIN_*, GAMEOVER) are
// displayed by the NUFLI displayer in nufli.c.

#include "screens.h"
#include <c64/vic.h>
#include <c64/cia.h>
#include <oscar.h>
#include <string.h>

// Bitmap data is LZO-compressed and lives in the default data section
// (which is inside the main region).  The 8 KB bitmaps are decompressed
// to $E000-$FFFF by show_screen().
const char ScreenWaiting1Bin[] = {
	#embed 8000 0 lzo "data/processed/waiting1.bin"
};

const char ScreenWaiting2Bin[] = {
	#embed 8000 0 lzo "data/processed/waiting2.bin"
};

// Color attribute tables live in the "screens" section, placed by the
// linker in the high region ($9C00-$D000) so they do not overlap with
// the NUFLI image at $2000-$7FFF.
#pragma section( screens, 0)
#pragma data(screens)

const char ScreenWaiting1Attr[] = {
	#embed "data/processed/waiting1.attr"
};

const char ScreenWaiting2Attr[] = {
	#embed "data/processed/waiting2.attr"
};

#pragma data(data)

struct ScreenDef {
    const char *lzo;
    const char *attr;
    byte d021;
};

static const struct ScreenDef screens[SCREEN_COUNT] = {
    { ScreenWaiting1Bin,  ScreenWaiting1Attr,  0  },  // SCREEN_WAITING1
    { ScreenWaiting2Bin,  ScreenWaiting2Attr,  11 },  // SCREEN_WAITING2
};

static void copy_bytes(const char *src, char *dst, unsigned len)
{
    for (unsigned i = 0; i < len; i++)
        dst[i] = src[i];
}

static void clear_color_ram(void)
{
    __asm {
        lda #0
        ldy #0
    L0: sta $d800, y
        sta $d900, y
        sta $da00, y
        sta $db00, y
        iny
        bne L0
    }
}

static void vic_setup_mcm(void)
{
    vic.ctrl1 = VIC_CTRL1_RST8 | VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL;
    vic.ctrl2 = VIC_CTRL2_MCM | VIC_CTRL2_CSEL;
    cia2.pra  = (cia2.pra & 0xfc) | 0x00;
    vic.memptr = 0x18;
}

void show_screen(int n)
{
    if (n < 0 || n >= SCREEN_COUNT) return;
    const struct ScreenDef *s = &screens[n];

    copy_bytes(s->attr, (char *)0xC400, 1000);
    oscar_expand_lzo((char *)0xE000, s->lzo);

    clear_color_ram();

    vic.color_back  = s->d021;
    vic.color_back1 = 0;
    vic.color_back2 = 0;
    vic.color_back3 = 0;
    vic.color_border = 0;

    vic_setup_mcm();
}

void show_white_screen(void)
{
    char *p = (char *)0xE000;
    for (unsigned i = 0; i < 8000; i++)
        p[i] = 0;

    char *sm = (char *)0xC400;
    for (unsigned i = 0; i < 1000; i++)
        sm[i] = 0;

    clear_color_ram();

    vic.color_back  = 1;
    vic.color_back1 = 1;
    vic.color_back2 = 1;
    vic.color_back3 = 0;
    vic.color_border = 1;

    vic_setup_mcm();
}
