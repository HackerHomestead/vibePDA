/* ui_box.h - Box-drawing primitives for vibePDA TUI.
 *
 * Unicode (U+2500 block) or ASCII fallback. Set VIBE_ASCII_BOX=1 for ASCII.
 */

#ifndef UI_BOX_H
#define UI_BOX_H

void ui_box_top(int row, int col, int w);
void ui_box_sep(int row, int col, int w);
void ui_box_bottom(int row, int col, int w);
void ui_box_v(void);

#endif
