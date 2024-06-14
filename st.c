#include "st.h"

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

//===============================Drawing functions===============================

void __st_plot_pixel(st_ctx* ctx, u32 x, u32 y, u32 color){
    if(x < 0 || x >= ctx->fb_width || y < 0 || y >= ctx->fb_height){
        return;
    }
    u32 transformed_color = 0;
    transformed_color |= (color & 0xFF) << ctx->fb_red_mask_shift;
    transformed_color |= ((color & 0xFF00) >> 8) << ctx->fb_green_mask_shift;
    transformed_color |= ((color & 0xFF0000) >> 16) << ctx->fb_blue_mask_shift;

    *(u32*)&((u8*)ctx->fb_addr)[y * ctx->fb_pitch + x * (ctx->fb_bpp / 8)] = transformed_color;
}

void __st_plot_glyph(st_ctx* ctx, u32 x, u32 y, u32 g){
    if(x < 0 || x >= ctx->fb_width/ctx->font_width || y < 0 || y >= ctx->fb_height/ctx->font_height || g >= ctx->font_glyph_count){
        return;
    }

    //TODO: optimize this
    u8 *glyph = (u8*)(ctx->font_glyphs) + g * ctx->font_bytes_per_glyph;
    for(u32 i = 0; i < ctx->font_height; i++){
        // if you know how to make it less of a mess, please let me know
        for(u32 j = 0; j < ctx->font_width; j++){
            _bool draw = (glyph[i * ((ctx->font_width / 8) + 1) + j / 8] >> (7 - j % 8)) & 1;
            __st_plot_pixel(ctx, x * ctx->font_width + j, y * ctx->font_height + i, draw ? ctx->color_fg : ctx->color_bg);
        }
    }
}

void __st_clear(st_ctx* ctx){
    for(u32 i = 0; i < ctx->fb_width * ctx->fb_height; i++){
        __st_plot_pixel(ctx, i % ctx->fb_width, i / ctx->fb_width, ctx->color_bg);
    }
}

//===============================Table functions===============================

u32 __st_get_glyph(st_ctx* ctx, u64 c) {
    if (ctx->font_type == 1 && ctx->font_utbl != NULL) { // PSF1
        u16* table = (u16*)(ctx->font_utbl);
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
    } else if (ctx->font_type == 2 && ctx->font_utbl != NULL) { // PSF2
        u8* table = (u8*)(ctx->font_utbl);
        u32 glyph_index = 0;
        u64 uc = 0;
        while (table <= (u8*)ctx->font_addr + ctx->font_size) {
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

//===============================Public functions===============================

//TODO: multiple characters and UNICODE
void st_write(st_ctx* ctx, char c){
    switch(c){
        case '\n':
            newline:
            ctx->cur_x = 0;
            ctx->cur_y++;
            if(ctx->cur_y >= (ctx->fb_height/ctx->font_height) - 5){
                //TODO: scroll
            }
            break;
        case '\b':
            ctx->cur_x--;
            break;
        default:
            __st_plot_glyph(ctx, ctx->cur_x, ctx->cur_y, __st_get_glyph(ctx, c));
            ctx->cur_x++;
            if(ctx->cur_x >= ctx->fb_width/ctx->font_width) goto newline;
            break;
    }
}

st_ctx st_init(u32* fb_addr, u32 fb_width, u32 fb_height, u32 fb_pitch,
               u32 fb_bpp, u8 fb_red_mask_size, u8 fb_red_mask_shift, 
               u8 fb_green_mask_size, u8 fb_green_mask_shift, u8 fb_blue_mask_size, u8 fb_blue_mask_shift,

               u32* font_data, u32 font_size){
    st_ctx new_ctx = {
        .fb_addr = fb_addr,
        .fb_width = fb_width,
        .fb_height = fb_height,
        .fb_pitch = fb_pitch,
        .fb_bpp = fb_bpp,
        .fb_red_mask_size = fb_red_mask_size,
        .fb_red_mask_shift = fb_red_mask_shift,
        .fb_green_mask_size = fb_green_mask_size,
        .fb_green_mask_shift = fb_green_mask_shift,
        .fb_blue_mask_size = fb_blue_mask_size,
        .fb_blue_mask_shift = fb_blue_mask_shift,

        .cur_x = 0,
        .cur_y = 0,
        .cur_visible = false,

        .color_bg = 0x000000,
        .color_fg = 0xffffff,
    };

    //interpret the font data
    new_ctx.font_addr = font_data;
    new_ctx.font_size = font_size;
    if((*(u16*)font_data) == PSF1_MAGIC){
        new_ctx.font_type = 1;
        new_ctx.font_glyphs = (u32*)((u8*)font_data + sizeof(psf1_header));
        new_ctx.font_glyph_count = ((psf1_header*)font_data)->mode & PSF1_MODE_512 ? 512 : 256;
        new_ctx.font_height = ((psf1_header*)font_data)->char_size;
        new_ctx.font_width = font_size / new_ctx.font_glyph_count / new_ctx.font_height;
        new_ctx.font_bytes_per_glyph = ((new_ctx.font_width / 8) + 1) * new_ctx.font_height;
        new_ctx.font_utbl = ((psf1_header*)font_data)->mode & (PSF1_MODE_HASTABLE | PSF1_MODE_SEQ) ?
            (u32*)((u8*)new_ctx.font_glyphs + new_ctx.font_bytes_per_glyph * new_ctx.font_glyph_count) 
            : NULL;

    }else if((*(u32*)font_data) == PSF2_MAGIC){
        new_ctx.font_type = 2;
        new_ctx.font_glyphs = (u32*)((u8*)font_data + sizeof(psf2_header));
        new_ctx.font_glyph_count = ((psf2_header*)font_data)->glyph_count;
        new_ctx.font_height = ((psf2_header*)font_data)->font_height;
        new_ctx.font_width = ((psf2_header*)font_data)->font_width;
        new_ctx.font_bytes_per_glyph = ((new_ctx.font_width / 8) + 1) * new_ctx.font_height;
        new_ctx.font_utbl = ((psf2_header*)font_data)->flags & (PSF2_FLAG_UC) ?
            (u32*)((u8*)new_ctx.font_glyphs + new_ctx.font_bytes_per_glyph * new_ctx.font_glyph_count) 
            : NULL;
    }

    return new_ctx;
}