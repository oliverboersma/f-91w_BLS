/*
 * Character set and segment map for the classic F-91W glass are from second-movement
 * (https://github.com/joeycastillo/second-movement, watch_common_display.h/.c),
 * MIT licence, Copyright (c) 2021-2024 Joey Castillo and contributors.
 */
#include "display.h"
#include <string.h>

#define NA 0xFF
#define CS(com, seg) ((uint8_t)(((seg) << 2) | (com)))

uint32_t disp_fb[DISP_COMS];

/* Segments A-G plus the extra "H" segment on positions 0 and 1, bit 0 = A. Index = ASCII - 0x20. */
static const uint8_t font[95] = {
    0b00000000, /* [space] */
    0b01100000, /* ! (L in the top half for positions 4 and 6) */
    0b00100010, /* " */
    0b01100011, /* # (degree symbol, hash mark doesn't fit) */
    0b00101101, /* $ (S without the center segment) */
    0b00000000, /* % (unused) */
    0b01000100, /* & ("lowercase 7" for positions 4 and 6) */
    0b00100000, /* ' */
    0b00111001, /* ( */
    0b00001111, /* ) */
    0b11000000, /* * (The + sign for use in position 0) */
    0b01110000, /* + (segments E, F and G; looks like ┣╸) */
    0b00000100, /* , */
    0b01000000, /* - */
    0b01000000, /* . (same as -, semantically most useful) */
    0b00010010, /* / */
    0b00111111, /* 0 */
    0b00000110, /* 1 */
    0b01011011, /* 2 */
    0b01001111, /* 3 */
    0b01100110, /* 4 */
    0b01101101, /* 5 */
    0b01111101, /* 6 */
    0b00000111, /* 7 */
    0b01111111, /* 8 */
    0b01101111, /* 9 */
    0b00000000, /* : (unused) */
    0b00000000, /* ; (unused) */
    0b01011000, /* < */
    0b01001000, /* = */
    0b01001100, /* > */
    0b01010011, /* ? */
    0b11111111, /* @ (all segments on) */
    0b01110111, /* A */
    0b01111111, /* B */
    0b00111001, /* C */
    0b00111111, /* D */
    0b01111001, /* E */
    0b01110001, /* F */
    0b00111101, /* G */
    0b01110110, /* H */
    0b10001001, /* I (only works in position 0) */
    0b00001110, /* J */
    0b01110101, /* K */
    0b00111000, /* L */
    0b10110111, /* M (only works in position 0) */
    0b00110111, /* N */
    0b00111111, /* O */
    0b01110011, /* P */
    0b01100111, /* Q */
    0b11110111, /* R (only works in position 1) */
    0b01101101, /* S */
    0b10000001, /* T (only works in position 0; set (1, 12) to make it work in position 1) */
    0b00111110, /* U */
    0b00111110, /* V */
    0b10111110, /* W (only works in position 0) */
    0b01111110, /* X */
    0b01101110, /* Y */
    0b00011011, /* Z */
    0b00111001, /* [ */
    0b00100100, /* backslash */
    0b00001111, /* ] */
    0b00100011, /* ^ */
    0b00001000, /* _ */
    0b00000010, /* ` */
    0b01011111, /* a */
    0b01111100, /* b */
    0b01011000, /* c */
    0b01011110, /* d */
    0b01111011, /* e */
    0b01110001, /* f */
    0b01101111, /* g */
    0b01110100, /* h */
    0b00010000, /* i */
    0b01000010, /* j (appears as superscript to work in more positions) */
    0b01110101, /* k */
    0b00110000, /* l */
    0b10110111, /* m (only works in position 0) */
    0b01010100, /* n */
    0b01011100, /* o */
    0b01110011, /* p */
    0b01100111, /* q */
    0b01010000, /* r */
    0b01101101, /* s */
    0b01111000, /* t */
    0b01100010, /* u (appears in (u)pper half to work in more positions) */
    0b00011100, /* v (looks like u but in the lower half) */
    0b10111110, /* w (only works in position 0) */
    0b01111110, /* x */
    0b01101110, /* y */
    0b00011011, /* z */
    0b00010110, /* { (open brace doesn't really work; overriden to represent the two character ligature "il") */
    0b00110110, /* | (overriden to represent the two character ligature "ll") */
    0b00110100, /* } (overriden to represent the two character ligature "li") */
    0b00000001, /* ~ */
};

/* (com, seg) of segments A..H for each position; NA where the glass has no such segment. */
static const uint8_t posmap[10][8] = {
    {CS(0,13), CS(1,13), CS(2,13), CS(2,15), CS(2,14), CS(0,14), CS(1,15), CS(1,14)},
    {CS(0,11), CS(1,11), CS(1,11), CS(2,11), CS(1,12), CS(1,12), CS(2,12), CS(0,12)},
    {CS(1, 9), CS(0, 9), CS(2, 9), CS(1, 9), CS(0,10), NA, CS(1, 9), NA},
    {CS(0, 7), CS(1, 7), CS(2, 7), CS(2, 6), CS(2, 8), CS(0, 8), CS(1, 8), NA},
    {CS(1,18), CS(2,19), CS(0,19), CS(1,18), CS(0,18), CS(2,18), CS(1,19), NA},
    {CS(2,20), CS(2,21), CS(1,21), CS(0,21), CS(0,20), CS(1,17), CS(1,20), NA},
    {CS(0,22), CS(2,23), CS(0,23), CS(0,22), CS(1,22), CS(2,22), CS(1,23), NA},
    {CS(2, 1), CS(2,10), CS(0, 1), CS(0, 0), CS(1, 0), CS(2, 0), CS(1, 1), NA},
    {CS(2, 2), CS(2, 3), CS(0, 4), CS(0, 3), CS(0, 2), CS(1, 2), CS(1, 3), NA},
    {CS(2, 4), CS(2, 5), CS(1, 6), CS(0, 6), CS(0, 5), CS(1, 4), CS(1, 5), NA},
};

static const uint8_t indicators[DISP_IND_COUNT] = {
    CS(0, 17), /* signal */
    CS(0, 16), /* bell */
    CS(2, 17), /* PM */
    CS(2, 16), /* 24H */
    CS(1, 10), /* LAP */
};

void disp_clear(void) { memset(disp_fb, 0, sizeof disp_fb); }

void disp_pixel(uint8_t com, uint8_t seg, bool on)
{
    if (com >= DISP_COMS || seg >= 32) return;
    if (on) disp_fb[com] |= 1UL << seg;
    else    disp_fb[com] &= ~(1UL << seg);
}

static void put(uint8_t cs, bool on) { disp_pixel(cs & 3, cs >> 2, on); }

/* Same substitutions second-movement makes so text reads well on this glass. */
static char fixup(uint8_t pos, char c)
{
    if (pos == 0) {
        if (c == 't' || c == 'T') c = '+';
    } else {
        if (c == 'u') c = 'v';
        else if (c == 'j') c = 'J';
        else if (c == '.') c = '_';
    }
    if (pos > 1 && c == 'T') c = 't';
    if (pos == 1) {
        static const char from[] = "aoinrdvVubc", to[] = "AOlNRDUUUBC";
        const char *p = strchr(from, c);
        if (c && p) c = to[p - from];
    } else {
        if (c == 'R') c = 'r';
        if (pos != 0 && c == 'I') c = 'l';
    }
    return c;
}

void disp_char(uint8_t pos, char c)
{
    if (pos > 9) return;
    c = fixup(pos, c);
    if (c < 0x20 || c > 0x7E) c = ' ';
    uint8_t bits = font[c - 0x20];
    if (pos == 0) put(CS(0, 15), false);            /* the "ninth segment" of position 0 */
    for (int i = 0; i < 8; i++, bits >>= 1)
        if (posmap[pos][i] != NA) put(posmap[pos][i], bits & 1);
    if (c == 'T' && pos == 1) put(CS(1, 12), true);
    else if ((c == 'B' || c == 'D' || c == '@') && pos <= 1) put(pos == 0 ? CS(0, 15) : CS(0, 12), true);
}

void disp_text(uint8_t pos, const char *s)
{
    while (*s && pos <= 9) disp_char(pos++, *s++);
}

void disp_colon(bool on) { put(CS(1, 16), on); }

void disp_indicator(disp_indicator_t ind, bool on)
{
    if (ind < DISP_IND_COUNT) put(indicators[ind], on);
}
