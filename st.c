#include "st.h"
#include "st_config.h"

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864ab572

#define PSF1_MODE_512 0x01
#define PSF1_MODE_HASTABLE 0x02
#define PSF1_MODE_SEQ 0x03

#define PSF2_FLAG_UC 0x01

typedef struct {
   u16 magic;
   u8 mode;
   u8 char_size; 
} psf1_header;

typedef struct {
    u32 magic;
    u32 version;
    u32 header_size;
    u32 flags;
    u32 glyph_count;
    u32 bytes_per_glyph;
    u32 font_height;
    u32 font_width;
} psf2_header;

st_ctx ctx = {
        .fb_addr = 0,
        .fb_width = 0,
        .fb_height = 0,
        .fb_pitch = 0,
        .fb_bpp = 0,
        .fb_red_mask_size = 0,
        .fb_red_mask_shift = 0,
        .fb_green_mask_size = 0,
        .fb_green_mask_shift = 0,
        .fb_blue_mask_size = 0,
        .fb_blue_mask_shift = 0,

        .cur_x = 0,
        .cur_y = 0,
        .cur_visible = true,

        .color_bg = 0x000000,
        .color_fg = 0xffffff,

        .uc_codepoint = 0,
        .uc_remaining = 0,

        .screen_table = {},
};


//===============================Helper functions===============================

void __st_small_memcpy(void* dest, const void* src, u32 n){
    for(u32 i = 0; i < n; i++){
        ((u8*)dest)[i] = ((const u8*)src)[i];
    }
}

//===============================Drawing functions===============================

void __st_plot_pixel(u32 x, u32 y, u32 color){
    if(x >= ctx.fb_width || y >= ctx.fb_height){
        return;
    }
    u32 transformed_color = 0;
    transformed_color |= (color & 0xFF) << ctx.fb_red_mask_shift;
    transformed_color |= ((color & 0xFF00) >> 8) << ctx.fb_green_mask_shift;
    transformed_color |= ((color & 0xFF0000) >> 16) << ctx.fb_blue_mask_shift;

    *(u32*)&((u8*)ctx.fb_addr)[y * ctx.fb_pitch + x * (ctx.fb_bpp / 8)] = transformed_color;
}

void __st_plot_glyph(u32 x, u32 y, u32 g, u32 color_fg, u32 color_bg){
    if(x >= ctx.fb_width/ctx.font_width || y >= ctx.fb_height/ctx.font_height || g >= ctx.font_glyph_count){
        return;
    }

    //TODO: optimize this
    u8 *glyph = (u8*)(ctx.font_glyphs) + g * ctx.font_bytes_per_glyph;
    for(u32 i = 0; i < ctx.font_height; i++){
        // if you know how to make it less of a mess, please let me know
        for(u32 j = 0; j < ctx.font_width; j++){
            _bool draw = (glyph[i * ((ctx.font_width / 8) + 1) + j / 8] >> (7 - j % 8)) & 1;
            __st_plot_pixel(x * ctx.font_width + j, y * ctx.font_height + i, draw ? color_fg : color_bg);
        }
    }
}

void __st_clear(){
    for(u32 i = 0; i < ctx.fb_width / ctx.font_width; i++){
        for(u32 j = 0; j < ctx.fb_height / ctx.font_height; j++){
            ctx.screen_table[i + j * (ctx.fb_width / ctx.font_width)].bg_col = ctx.color_bg;
            ctx.screen_table[i + j * (ctx.fb_width / ctx.font_width)].fg_col = ctx.color_fg;
            ctx.screen_table[i + j * (ctx.fb_width / ctx.font_width)].glyph_num = 0;
        }
    }
    for(u32 i = 0; i < ctx.fb_width * ctx.fb_height; i++){
        __st_plot_pixel(i % ctx.fb_width, i / ctx.fb_width, ctx.color_bg);
    }
}

void __st_render_cursor(){
    if(ctx.cur_visible){
        for(u32 i = 0; i < ctx.font_height; i++){
            for(u32 j = 0; j < ctx.font_width; j++){
                __st_plot_pixel(ctx.cur_x * ctx.font_width + j, ctx.cur_y * ctx.font_height + i, ctx.color_fg);
            }
        }
    }
}

//===============================Table functions===============================

u32 __st_get_glyph(u64 c) {
    if (ctx.font_type == 1 && ctx.font_utbl != NULL) { // PSF1
        u16* table = (u16*)(ctx.font_utbl);
        u32 glyph_index = 0;
        while (*table != 0xFFFF) {
            if (*table == c) {
                return glyph_index;
            }
            if (*table == 0xFFFE) {
                glyph_index++;
            }
            table++;
        }
    } else if (ctx.font_type == 2 && ctx.font_utbl != NULL) { // PSF2
        u8* table = (u8*)(ctx.font_utbl);
        u32 glyph_index = 0;
        u64 uc = 0;
        while (table <= (u8*)ctx.font_addr + ctx.font_size) {
            uc = *table;
            if(*table == 0xff){
                glyph_index++;
                table++;
                continue;
            } else if (*table & 128) {
                if((uc & 32) == 0 ) {
                    uc = ((table[0] & 0x1F)<<6)+(table[1] & 0x3F);
                    table++;
                } else
                if((uc & 16) == 0 ) {
                    uc = ((((table[0] & 0xF)<<6)+(table[1] & 0x3F))<<6)+(table[2] & 0x3F);
                    table+=2;
                } else if((uc & 8) == 0 ) {
                    uc = ((((((table[0] & 0x7)<<6)+(table[1] & 0x3F))<<6)+(table[2] & 0x3F))<<6)+(table[3] & 0x3F);
                    table+=3;
                } else {
                    uc = 0;
                }
            }
            if(uc == c) {
                return glyph_index;
            }
            table++;
        }
    }
    return (u32)c;
}

void __st_redraw(){
    for(u32 i = 0; i < ctx.fb_width / ctx.font_width; i++){
        for(u32 j = 0; j < ctx.fb_height / ctx.font_height; j++){
            st_color_cell cell = ctx.screen_table[i + j * (ctx.fb_width / ctx.font_width)];
            __st_plot_glyph(i,j,cell.glyph_num,cell.fg_col,cell.bg_col);
        }
    }
}

//===============================Public functions===============================

//TODO: multiple characters and UNICODE
void st_write(u8 c){

    //This lump of code stitches UNICODE characters together from UTF-8 multy-byte characters.
    if (ctx.uc_remaining > 0) {
        if ((c & 0xc0) != 0x80) {
            ctx.uc_remaining = 0;
        } else {
            ctx.uc_remaining--;
            ctx.uc_codepoint |= (u64)(c & 0x3f) << (6 * ctx.uc_remaining);
            if (ctx.uc_remaining != 0) {
                return;
            }
        }
    } else {
        ctx.uc_codepoint = c;
    }

    //This lump of code detects UTF-8 multy-byte characters, and sets the ctx.uc_remaining variable to how many bytes are remaining.
    if (c >= 0xc0 && c <= 0xf7) {
        if (c >= 0xc0 && c <= 0xdf) {
            ctx.uc_remaining = 1;
            ctx.uc_codepoint = (u64)(c & 0x1f) << 6;
        } else if (c >= 0xe0 && c <= 0xef) {
            ctx.uc_remaining = 2;
            ctx.uc_codepoint = (u64)(c & 0x0f) << (6 * 2);
        } else if (c >= 0xf0 && c <= 0xf7) {
            ctx.uc_remaining = 3;
            ctx.uc_codepoint = (u64)(c & 0x07) << (6 * 3);
        }
        return;
    }

    #define ST_ERASE_CHAR __st_plot_glyph(ctx.cur_x, ctx.cur_y, __st_get_glyph(' '),0,0)
    switch(ctx.uc_codepoint){
        case 0x00:
        case 0x7f:
            return; //ignore
        case 0x0b://FALLTHROUGH
        case 0x0c://FALLTHROUGH
        case '\n':
            newline:
            ST_ERASE_CHAR;
            ctx.cur_x = 0;
            ctx.cur_y++;
            if(ctx.cur_y >= (ctx.fb_height/ctx.font_height) - ST_SCROLL_TRESHOLD){
                __st_small_memcpy(ctx.screen_table, ctx.screen_table + (ctx.fb_width / ctx.font_width), (ctx.fb_width / ctx.font_width) * ((ctx.fb_height / ctx.font_height)  - ST_SCROLL_TRESHOLD - 1) * sizeof(st_color_cell));
                __st_redraw();
               CTX.cur_y--;
            }
            __st_render_cursor();
            break;
        case '\b':
            ctx.cur_x--;
            break;
        case '\r':
            ST_ERASE_CHAR;
            ctx.cur_x = 0;
            break;
        case '\t':
            ST_ERASE_CHAR;
            ctx.cur_x += ctx.cur_x % ST_TAB_WIDTH;
            break;
        default:
            ctx.screen_table[ctx.cur_x + ctx.cur_y * (ctx.fb_width / ctx.font_width)].fg_col = ctx.color_fg & 0xFFFFFF;
            ctx.screen_table[ctx.cur_x + ctx.cur_y * (ctx.fb_width / ctx.font_width)].bg_col = ctx.color_bg & 0xFFFFFF;

            u32 g = __st_get_glyph(ctx.uc_codepoint);
            ctx.screen_table[ctx.cur_x + ctx.cur_y * (ctx.fb_width / ctx.font_width)].glyph_num = g & 0xFFF;
            __st_plot_glyph(ctx.cur_x, ctx.cur_y, g, ctx.color_fg, ctx.color_bg);

            ctx.cur_x++;
            if(ctx.cur_x >= ctx.fb_width/ctx.font_width) goto newline;
            __st_render_cursor();
            break;
    }
}

void st_init(u32* fb_addr, u32 fb_width, u32 fb_height, u32 fb_pitch,
               u32 fb_bpp, u8 fb_red_mask_size, u8 fb_red_mask_shift, 
               u8 fb_green_mask_size, u8 fb_green_mask_shift, u8 fb_blue_mask_size, u8 fb_blue_mask_shift,

               u32* font_data, u32 font_size){
    ctx.fb_addr = fb_addr,
    ctx.fb_width = fb_width,
    ctx.fb_height = fb_height,
    ctx.fb_pitch = fb_pitch,
    ctx.fb_bpp = fb_bpp,
    ctx.fb_red_mask_size = fb_red_mask_size,
    ctx.fb_red_mask_shift = fb_red_mask_shift,
    ctx.fb_green_mask_size = fb_green_mask_size,
    ctx.fb_green_mask_shift = fb_green_mask_shift,
    ctx.fb_blue_mask_size = fb_blue_mask_size,
    ctx.fb_blue_mask_shift = fb_blue_mask_shift,

    //interpret the font data
    ctx.font_addr = font_data;
    ctx.font_size = font_size;
    if((*(u16*)font_data) == PSF1_MAGIC){
        ctx.font_type = 1;
        ctx.font_glyphs = (u32*)((u8*)font_data + sizeof(psf1_header));
        ctx.font_glyph_count = ((psf1_header*)font_data)->mode & PSF1_MODE_512 ? 512 : 256;
        ctx.font_height = ((psf1_header*)font_data)->char_size;
        ctx.font_width = 8;
        ctx.font_bytes_per_glyph = ((ctx.font_width / 8) + 1) * ctx.font_height;
        ctx.font_utbl = ((psf1_header*)font_data)->mode & (PSF1_MODE_HASTABLE | PSF1_MODE_SEQ) ?
            (u32*)((u8*)ctx.font_glyphs + ctx.font_bytes_per_glyph * ctx.font_glyph_count) 
            : NULL;

    }else if((*(u32*)font_data) == PSF2_MAGIC){
        ctx.font_type = 2;
        ctx.font_glyphs = (u32*)((u8*)font_data + ((psf2_header*)font_data)->header_size);
        ctx.font_glyph_count = ((psf2_header*)font_data)->glyph_count;
        ctx.font_height = ((psf2_header*)font_data)->font_height;
        ctx.font_width = ((psf2_header*)font_data)->font_width;
        ctx.font_bytes_per_glyph = ((ctx.font_width / 8) + 1) * ctx.font_height;
        ctx.font_utbl = ((psf2_header*)font_data)->flags & (PSF2_FLAG_UC) ?
            (u32*)((u8*)ctx.font_glyphs + ctx.font_bytes_per_glyph * ctx.font_glyph_count) 
            : NULL;
    }

    __st_render_cursor();
}
