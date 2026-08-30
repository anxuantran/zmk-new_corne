/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Strong override of ZMK's weak zmk_display_status_screen(), selected by
 * CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM.
 *
 * Layout is specified in an upright 32x128 strip; see canvas.h for why.
 * The two halves share the widget code and differ only in their y offsets and
 * in what occupies the lower half of the strip.
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include "art.h"
#include "canvas.h"

#define IS_CENTRAL IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#if IS_CENTRAL
#include <zmk/ble.h>
#include <zmk/events/ble_active_profile_changed.h>
#else
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>

#include "wpm_relay.h"
#endif

/* ------------------------------------------------------------------ layout */

#if IS_CENTRAL
#define BATTERY_Y 14
#define BATTERY_TEXT_Y 30
#define BLUETOOTH_Y 64
#define PROFILE_Y 100
#else
/* Shifted up so nothing crosses y 64. Everything from y 68 down belongs to
 * the cat, and the status stack must not encroach on it. */
#define BATTERY_Y 8
#define BATTERY_TEXT_Y 24
#define BLUETOOTH_Y 44
#define BONGO_Y 70
#endif

/* Battery bar: outline x 4..27, terminal nub x 28..29, 22px of fill from x 5. */
#define BATTERY_X 4
#define BATTERY_W 24
#define BATTERY_H 11
#define BATTERY_FILL_X 5
#define BATTERY_FILL_W 22

/* The glyph is centred in an 18px band so that resizing the art does not
 * shift the spacing of everything around it. */
#define BLUETOOTH_BAND_H 18
#define BLUETOOTH_X ((ECORNE_STRIP_W - BT_ON_W) / 2)
#define BLUETOOTH_GLYPH_Y (BLUETOOTH_Y + (BLUETOOTH_BAND_H - BT_ON_H) / 2)

/* ------------------------------------------------------------------- state */

static struct {
    uint8_t battery;
    bool connected;
#if IS_CENTRAL
    uint8_t profile;
#endif
} state;

static void draw_battery(void) {
    ecorne_rect(BATTERY_X, BATTERY_Y, BATTERY_W, BATTERY_H, false);
    ecorne_rect(BATTERY_X + BATTERY_W, BATTERY_Y + 3, 2, 5, true);

    uint8_t pct = state.battery > 100 ? 100 : state.battery;
    lv_coord_t fill = (BATTERY_FILL_W * pct + 50) / 100;
    if (fill > 0) {
        ecorne_rect(BATTERY_FILL_X, BATTERY_Y + 2, fill, BATTERY_H - 4, true);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", pct);
    ecorne_text(buf, BATTERY_TEXT_Y + 1, ECORNE_FG);
}

static void draw_bluetooth(void) {
    ecorne_img(state.connected ? &bt_on : &bt_off, BLUETOOTH_X, BLUETOOTH_GLYPH_Y);
}

#if IS_CENTRAL
static void draw_profile(void) {
    /* Inverted row: lit block, unlit text. */
    ecorne_rect(1, PROFILE_Y, 30, 17, true);

    char buf[8];
    snprintf(buf, sizeof(buf), "BT%u", state.profile + 1);
    ecorne_text(buf, PROFILE_Y + 4, ECORNE_BG);
}
#else
/* Thresholds and frame duration match dancarroll/qmk-bongo so the cat behaves
 * the way the original does. */
#define BONGO_READY_WPM 10
#define BONGO_TAP_WPM 20
#define BONGO_FRAME_MS 200

static const lv_img_dsc_t *bongo_frame;
static uint8_t bongo_phase;

static const lv_img_dsc_t *bongo_pick(void) {
    /* Pushed over the split link by the central; see wpm_relay.c. Polling it
     * here means the cat needs no event subscription of its own. */
    uint8_t wpm = ecorne_relayed_wpm();

    if (wpm < BONGO_READY_WPM) {
        return &bongo_ready;
    }
    if (wpm >= BONGO_TAP_WPM) {
        return bongo_phase ? &bongo_tap1 : &bongo_tap0;
    }
    return &bongo_waiting;
}

static void draw_bongo(void) {
    ecorne_img(bongo_frame ? bongo_frame : &bongo_ready, (ECORNE_STRIP_W - BONGO_READY_W) / 2,
               BONGO_Y);
}
#endif

static void redraw(void) {
    ecorne_clear();
    draw_battery();
    draw_bluetooth();
#if IS_CENTRAL
    draw_profile();
#else
    draw_bongo();
#endif
    ecorne_commit();
}

/* --------------------------------------------------------------- listeners */

struct battery_state {
    uint8_t level;
};

static struct battery_state battery_get_state(const zmk_event_t *eh) {
    /* The listener macro calls this with NULL once at init. */
    const struct zmk_battery_state_changed *ev = eh ? as_zmk_battery_state_changed(eh) : NULL;
    return (struct battery_state){.level = ev ? ev->state_of_charge : zmk_battery_state_of_charge()};
}

static void battery_update_cb(struct battery_state s) {
    state.battery = s.level;
    redraw();
}

ZMK_DISPLAY_WIDGET_LISTENER(ecorne_battery, struct battery_state, battery_update_cb,
                            battery_get_state)
ZMK_SUBSCRIPTION(ecorne_battery, zmk_battery_state_changed);

struct output_state {
    bool connected;
#if IS_CENTRAL
    uint8_t profile;
#endif
};

static struct output_state output_get_state(const zmk_event_t *eh) {
#if IS_CENTRAL
    return (struct output_state){.connected = zmk_ble_active_profile_is_connected(),
                                 .profile = zmk_ble_active_profile_index()};
#else
    return (struct output_state){.connected = zmk_split_bt_peripheral_is_connected()};
#endif
}

static void output_update_cb(struct output_state s) {
    state.connected = s.connected;
#if IS_CENTRAL
    state.profile = s.profile;
#endif
    redraw();
}

ZMK_DISPLAY_WIDGET_LISTENER(ecorne_output, struct output_state, output_update_cb, output_get_state)
#if IS_CENTRAL
ZMK_SUBSCRIPTION(ecorne_output, zmk_ble_active_profile_changed);
#else
ZMK_SUBSCRIPTION(ecorne_output, zmk_split_peripheral_status_changed);
#endif

#if !IS_CENTRAL
static void bongo_tick(lv_timer_t *timer) {
    bongo_phase ^= 1;

    const lv_img_dsc_t *next = bongo_pick();
    if (next == bongo_frame) {
        /* Idle and mid tiers are single frames, so a still cat costs no redraw,
         * no rotation and no I2C traffic at all. */
        return;
    }

    bongo_frame = next;
    redraw();
}
#endif

/* ------------------------------------------------------------------ screen */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, ECORNE_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);

    ecorne_canvas_init(screen);

    ecorne_battery_init();
    ecorne_output_init();
#if !IS_CENTRAL
    bongo_frame = bongo_pick();
    lv_timer_create(bongo_tick, BONGO_FRAME_MS, NULL);
#endif

    redraw();

    return screen;
}
