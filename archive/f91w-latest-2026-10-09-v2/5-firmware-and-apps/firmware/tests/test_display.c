/* Host test for app/display.c: check a few glyphs land on the segments second-movement uses. */
#include <stdio.h>
#include "display.h"

void disp_flush(void) {}

static int on(int com, int seg) { return (disp_fb[com] >> seg) & 1; }

int main(void)
{
    int ok = 1;
    disp_clear();
    disp_char(9, '8');               /* all seven segments of position 9 */
    ok &= on(2,4) && on(2,5) && on(1,6) && on(0,6) && on(0,5) && on(1,4) && on(1,5);
    disp_char(9, '1');               /* only B and C */
    ok &= !on(2,4) && on(2,5) && on(1,6) && !on(0,6) && !on(1,5);
    disp_clear();
    disp_text(4, "123456");
    int count = 0;
    for (int c = 0; c < 3; c++) for (int s = 0; s < 32; s++) count += on(c, s);
    ok &= count > 20;
    for (int c = 0; c < 3; c++) ok &= (disp_fb[c] >> 24) == 0;   /* nothing beyond glass SEG23 */
    disp_clear();
    for (int p = 0; p < 10; p++) for (int ch = 0x20; ch < 0x7F; ch++) disp_char(p, (char)ch);
    disp_colon(1); ok &= on(1,16);
    printf(ok ? "PASS\n" : "FAIL\n");
    return !ok;
}
