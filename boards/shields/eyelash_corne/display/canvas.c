/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include "canvas.h"

/*
 * 4096 bytes each at LV_COLOR_DEPTH_1, where lv_color_t is one byte. Two full
 * framebuffers is the entire cost of doing rotation ourselves.
 */
static lv_color_t strip_buf[ECORNE_STRIP_W * ECORNE_STRIP_H];
static lv_color_t panel_buf[ECORNE_PANEL_W * ECORNE_PANEL_H];

static lv_obj_t *strip_canvas; /* upright, off-screen: widgets draw here */
static lv_obj_t *panel_canvas; /* native, on-screen: only ecorne_commit writes here */

void ecorne_canvas_init(lv_obj_t *parent) {
    panel_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(panel_canvas, panel_buf, ECORNE_PANEL_W, ECORNE_PANEL_H,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(panel_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Never shown. lv_canvas_draw_* write straight into the buffer, so a hidden
     * canvas is a perfectly good off-screen drawing target. */
    strip_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(strip_canvas, strip_buf, ECORNE_STRIP_W, ECORNE_STRIP_H,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_add_flag(strip_canvas, LV_OBJ_FLAG_HIDDEN);

    ecorne_clear();
    lv_canvas_fill_bg(panel_canvas, ECORNE_BG, LV_OPA_COVER);
}

void ecorne_commit(void) {
    for (int y = 0; y < ECORNE_STRIP_H; y++) {
        const lv_color_t *src = &strip_buf[y * ECORNE_STRIP_W];
#if ECORNE_ROTATE_CW
        /* strip(x,y) -> panel(STRIP_H-1-y, x) */
        lv_color_t *col = &panel_buf[ECORNE_PANEL_W - 1 - y];
        for (int x = 0; x < ECORNE_STRIP_W; x++) {
            col[x * ECORNE_PANEL_W] = src[x];
        }
#else
        /* strip(x,y) -> panel(y, STRIP_W-1-x) */
        lv_color_t *col = &panel_buf[y];
        for (int x = 0; x < ECORNE_STRIP_W; x++) {
            col[(ECORNE_STRIP_W - 1 - x) * ECORNE_PANEL_W] = src[x];
        }
#endif
    }

    lv_obj_invalidate(panel_canvas);
}

void ecorne_clear(void) { lv_canvas_fill_bg(strip_canvas, ECORNE_BG, LV_OPA_COVER); }

void ecorne_rect(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, bool filled) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = 0;

    if (filled) {
        dsc.bg_color = ECORNE_FG;
        dsc.bg_opa = LV_OPA_COVER;
        dsc.border_width = 0;
    } else {
        dsc.bg_opa = LV_OPA_TRANSP;
        dsc.border_color = ECORNE_FG;
        dsc.border_opa = LV_OPA_COVER;
        dsc.border_width = 1;
    }

    lv_canvas_draw_rect(strip_canvas, x, y, w, h, &dsc);
}

void ecorne_img(const lv_img_dsc_t *src, lv_coord_t x, lv_coord_t y) {
    lv_draw_img_dsc_t dsc;
    lv_draw_img_dsc_init(&dsc);

    lv_canvas_draw_img(strip_canvas, x, y, src, &dsc);
}

void ecorne_text(const char *txt, lv_coord_t y, lv_color_t color) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = &lv_font_unscii_8; /* 8x8 bitmap: no antialiasing to dither at 1bpp */
    dsc.align = LV_TEXT_ALIGN_CENTER;

    lv_canvas_draw_text(strip_canvas, 0, y, ECORNE_STRIP_W, &dsc, txt);
}
