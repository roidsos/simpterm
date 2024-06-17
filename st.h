#ifndef __ST_H__
#define __ST_H__

typedef unsigned char st_u8;
typedef unsigned short st_u16;
typedef unsigned int st_u32;
typedef unsigned long long st_u64;

typedef unsigned char st_bool;

#ifndef true
    #define true 1
    #define false 0
#endif

#ifndef PACKED
    #define PACKED __attribute__((packed))
#endif

#ifndef NULL
    #define NULL ((void *)0)
#endif

#ifndef ST_TAB_WIDTH
#define ST_TAB_WIDTH 4
#endif

#ifndef ST_MAX_COLS
#define ST_MAX_COLS 256
#endif

#ifndef ST_MAX_ROWS
#define ST_MAX_ROWS 256
#endif

#ifndef ST_SCROLL_TRESHOLD
#define ST_SCROLL_TRESHOLD 1
#endif

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864ab572

#define PSF1_MODE_512 0x01
#define PSF1_MODE_HASTABLE 0x02
#define PSF1_MODE_SEQ 0x03

#define PSF2_FLAG_UC 0x01

typedef struct {
   st_u16 magic;
   st_u8 mode;
   st_u8 char_size; 
} PACKED psf1_header;

typedef struct {
    st_u32 magic;
    st_u32 version;
    st_u32 header_size;
    st_u32 flags;
    st_u32 glyph_count;
    st_u32 bytes_per_glyph;
    st_u32 font_height;
    st_u32 font_width;
} PACKED psf2_header;

typedef struct {
    st_u16 glyph_num : 12;
    st_u32 fg_col : 24;
    st_u32 bg_col : 24;
} PACKED st_color_cell;

typedef struct {
    //Framebuffer stuff
    st_u32*  fb_addr;
    st_u32   fb_width;
    st_u32   fb_height;
    st_u32   fb_pitch;
    st_u32   fb_bpp;
    st_u8    fb_red_mask_size;
    st_u8    fb_red_mask_shift;
    st_u8    fb_green_mask_size;
    st_u8    fb_green_mask_shift;
    st_u8    fb_blue_mask_size;
    st_u8    fb_blue_mask_shift;

    //Cursor stuff
    st_u32   cur_x;
    st_u32   cur_y;
    st_bool cur_visible;

    //Font stuff
    st_u32*  font_addr;
    st_u32   font_size;
    st_u8    font_type;
    st_u32   font_width;
    st_u32   font_height;
    st_u32   font_glyph_count;
    st_u32   font_bytes_per_glyph;
    st_u32*  font_glyphs;
    st_u32*  font_utbl;

    //State
    st_u32 color_fg;
    st_u32 color_bg;
    st_u8  uc_remaining;
    st_u64 uc_codepoint;
    st_color_cell screen_table[ST_MAX_ROWS * ST_MAX_COLS];

} st_ctx;

void st_write(st_u8 c);

void st_init(st_u32* fb_addr, st_u32 fb_width, st_u32 fb_height, st_u32 fb_pitch,
               st_u32 fb_bpp, st_u8 fb_red_mask_size, st_u8 fb_red_mask_shift, 
               st_u8 fb_green_mask_size, st_u8 fb_green_mask_shift, st_u8 fb_blue_mask_size, st_u8 fb_blue_mask_shift,

               st_u32* font_data,st_u32 font_size);

#endif // __ST_H__