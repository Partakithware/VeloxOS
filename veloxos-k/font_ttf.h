#ifndef FONT_TTF_H
#define FONT_TTF_H

#include <efi.h>

void init_ttf_font(int pixel_height);
void ttf_put_char(int x, int y, char c, uint32_t color);
void ttf_put_char_buf(uint32_t* buffer, int buf_width, int x, int y, char c, uint32_t color);
int get_ttf_char_width(char c); // New helper

extern float font_scale;
extern int font_ascent, font_descent, font_lineGap;

#endif