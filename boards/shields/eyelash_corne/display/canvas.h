/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

/*
 * The panel is 128x32, but it is mounted with its long axis running
 * front-to-back, so the surface the user actually reads is 32 wide and 128
 * tall. Every widget in this directory is authored in that upright "strip"
 * space, and the whole strip is rotated exactly once, on commit, into the
 * panel's native orientation.
 *
 * This is deliberate. The SSD1306 driver has no .set_orientation, devicetree
 * only offers 180 degrees, and this tree runs LVGL 8.3 (Zephyr 3.5), which has
 * no usable software display rotation. One flat blit of 4096 bytes is both
 * cheaper and more predictable than anything LVGL would do per frame.
 */
#define ECORNE_PANEL_W 128
#define ECORNE_PANEL_H 32
#define ECORNE_STRIP_W 32
#define ECORNE_STRIP_H 128

/*
 * Direction of that single rotation. The two halves are 180 degrees apart in
 * hardware (mirrored PCBs); that is cancelled in the right half's overlay with
 * segment-remap/com-invdir, so one direction serves both. If both halves come
 * back upside down, move the overlay block to the other half. If both come back
 * rotated the wrong way, flip this.
 */
#define ECORNE_ROTATE_CW 1

/*
 * Foreground/background. CONFIG_ZMK_DISPLAY_INVERT=y already flips polarity in
 * the driver; these are the only two places LVGL-side polarity is decided, and
 * they must stay in step with ECORNE_ART_PALETTE in art.c.
 */
#define ECORNE_FG lv_color_white()
#define ECORNE_BG lv_color_black()

void ecorne_canvas_init(lv_obj_t *parent);

/* Rotate the strip onto the panel and mark it dirty. Widgets never call the
 * LVGL flush path themselves; they draw, then commit. */
void ecorne_commit(void);

/* Drawing helpers. All coordinates are strip coordinates: x 0..31, y 0..127. */
void ecorne_clear(void);
void ecorne_rect(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, bool filled);
void ecorne_img(const lv_img_dsc_t *src, lv_coord_t x, lv_coord_t y);
void ecorne_text(const char *txt, lv_coord_t y, lv_color_t color);
