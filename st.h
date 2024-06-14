#ifndef __ST_H__
#define __ST_H__

#include "libmacro/std.h"

typedef struct {
    //Framebuffer stuff
    u32*  fb_addr;
    u32   fb_width;
    u32   fb_height;
    u32   fb_pitch;
    u32   fb_bpp;
    u8    fb_red_mask_size;
    u8    fb_red_mask_shift;
    u8    fb_green_mask_size;
    u8    fb_green_mask_shift;
    u8    fb_blue_mask_size;
    u8    fb_blue_mask_shift;

    //Cursor stuff
    u32   cur_x;
    u32   cur_y;
    _bool cur_visible;

    //Font stuff
    u32*  font_addr;
    u32   font_size;
    u8    font_type;
    u32   font_width;
    u32   font_height;
    u32   font_glyph_count;
    u32   font_bytes_per_glyph;
    u32*  font_glyphs;
    u32*  font_utbl;

    //State
    u32 color_fg;
    u32 color_bg;

} st_ctx;


st_ctx st_init(u32* fb_addr, u32 fb_width, u32 fb_height, u32 fb_pitch,
               u32 fb_bpp, u8 fb_red_mask_size, u8 fb_red_mask_shift, 
               u8 fb_green_mask_size, u8 fb_green_mask_shift, u8 fb_blue_mask_size, u8 fb_blue_mask_shift,
               
               u32* font_data,u32 font_size);

#endif // __ST_H__