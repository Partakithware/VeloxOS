#include "stb_truetype.h"
#include "console.h"

extern unsigned char AovelSansRounded_ttf[];
stbtt_fontinfo font_info;
float font_scale;
int font_ascent, font_descent, font_lineGap;

void init_ttf_font(int pixel_height) {
    stbtt_InitFont(&font_info, AovelSansRounded_ttf, 0);
    
    // Calculate the scale for the desired pixel height
    font_scale = stbtt_ScaleForPixelHeight(&font_info, (float)pixel_height);
    
    // Get vertical metrics
    stbtt_GetFontVMetrics(&font_info, &font_ascent, &font_descent, &font_lineGap);
}

// Original version - writes to framebuffer (for console use)
void ttf_put_char(int x, int y, char c, uint32_t color) {
    int width, height, xoff, yoff;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&font_info, 0, font_scale, c, &width, &height, &xoff, &yoff);

    if (bitmap) {
        int baseline_y = y + (int)(font_ascent * font_scale);

        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                unsigned char alpha = bitmap[row * width + col];
                if (alpha > 0) {
                    uint32_t* pixel = (uint32_t*)fb_base + (baseline_y + row + yoff) * PixelsPerScanLine + (x + col + xoff);
                    uint32_t intensity = alpha; 
                    *pixel = (intensity << 16) | (intensity << 8) | intensity;
                }
            }
        }
        stbtt_FreeBitmap(bitmap, NULL);
    }
    
}

// Static scratch buffer for glyph rendering (16KB is plenty for one char)
// This avoids calling kmalloc 2000 times per frame!
static unsigned char glyph_buffer[16384]; 

void ttf_put_char_buf(uint32_t* buffer, int buf_width, int x, int y, char c, uint32_t color) {
    if (!buffer) return;
    if (buf_width <= 0 || buf_width > 4096) return;
    if (c < 32 || c > 126) return; 

    int width, height, xoff, yoff;
    
    // SCALE: Re-calculate scale if needed, or rely on global 'font_scale'
    // (Assuming font_scale is already set by init_ttf_font)

    // 1. GET METRICS ONLY (No allocation)
    stbtt_GetCodepointBitmapBox(&font_info, c, font_scale, font_scale, &xoff, &yoff, &width, &height);
    
    // 2. SAFETY CHECK
    width = width - xoff; // Box coordinates to dimensions
    height = height - yoff;
    if (width * height > 16384) return; // Too big for our static buffer
    
    // 3. RENDER TO STATIC BUFFER (No malloc!)
    stbtt_MakeCodepointBitmap(&font_info, glyph_buffer, width, height, width, font_scale, font_scale, c);

    int baseline_y = y + (int)(font_ascent * font_scale);

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            // Read from our static buffer
            unsigned char alpha = glyph_buffer[row * width + col];
            
            if (alpha > 10) { 
                int draw_x = x + col + xoff;
                int draw_y = baseline_y + row + yoff;
                
                if (draw_x >= 0 && draw_x < buf_width && 
                    draw_y >= 0 && draw_y < (int)ScreenHeight) {
                    
                    uint32_t* pixel = buffer + (draw_y * buf_width) + draw_x;
                    
                    // (Your existing alpha blending logic here)
                    uint32_t bg = *pixel;
                    uint32_t bg_r = (bg >> 16) & 0xFF;
                    uint32_t bg_g = (bg >> 8) & 0xFF;
                    uint32_t bg_b = bg & 0xFF;
                    
                    uint32_t fg_r = (color >> 16) & 0xFF;
                    uint32_t fg_g = (color >> 8) & 0xFF;
                    uint32_t fg_b = color & 0xFF;
                    
                    uint32_t out_r = ((fg_r * alpha) + (bg_r * (255 - alpha))) / 255;
                    uint32_t out_g = ((fg_g * alpha) + (bg_g * (255 - alpha))) / 255;
                    uint32_t out_b = ((fg_b * alpha) + (bg_b * (255 - alpha))) / 255;
                    
                    *pixel = (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }
}

int get_ttf_char_width(char c) {
    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font_info, c, &advance, &lsb);
    return (int)(advance * font_scale) + 1; 
}