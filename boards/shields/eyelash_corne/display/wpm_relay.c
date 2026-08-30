/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * The right half is a split peripheral: key positions travel peripheral ->
 * central and HID indicators travel central -> peripheral, but nothing tells
 * the peripheral that the *left* hand is typing. Left alone, a WPM-driven
 * bongo cat on the right screen would only respond to right-hand keys.
 *
 * This fork carries a generic named-event relay (CONFIG_ZMK_SPLIT_RELAY_EVENT,
 * already enabled on both halves), so the fix is to relay the central's
 * zmk_wpm_state_changed rather than to invent a split characteristic.
 *
 * The peripheral deliberately does NOT enable CONFIG_ZMK_WPM. ZMK's wpm.c
 * counts zmk_keycode_state_changed, which is a central-only event, so building
 * it on the peripheral fails to link. It would also be pointless: the
 * peripheral never sees keycodes, so its own figure would sit at 0 forever
 * while running a 1 Hz work item. Instead the raw relay event is decoded here.
 */

#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
/* Declarations only -- this pulls in no central-only symbols. */
#include <zmk/events/wpm_state_changed.h>

#include "wpm_relay.h"

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/*
 * Budget check against the fork's defaults, both BUILD_ASSERTed by the macro:
 *   sizeof(struct zmk_wpm_state_changed) == 4  <= RELAY_EVENT_DATA_LEN      (14)
 *   sizeof("wpm")                        == 4  <= RELAY_EVENT_TYPE_NAME_LEN (4)
 */
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(zmk_wpm_state_changed, wpm, )

uint8_t ecorne_relayed_wpm(void) { return 0; }

#else

static volatile uint8_t relayed_wpm;

uint8_t ecorne_relayed_wpm(void) { return relayed_wpm; }

static int ecorne_wpm_relay_cb(const zmk_event_t *eh) {
    const struct zmk_relay_event_received *ev = as_zmk_relay_event_received(eh);
    if (ev == NULL || strcmp(ev->event_name, ECORNE_WPM_RELAY_ID) != 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->event_data_size != sizeof(struct zmk_wpm_state_changed)) {
        LOG_WRN("relayed wpm payload was %d bytes, expected %d", ev->event_data_size,
                (int)sizeof(struct zmk_wpm_state_changed));
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_wpm_state_changed payload;
    memcpy(&payload, ev->event_data, sizeof(payload));

    /* A single byte, read on the display queue by the cat's frame timer. */
    relayed_wpm = payload.state < 0 ? 0 : (payload.state > 255 ? 255 : (uint8_t)payload.state);

    return ZMK_EV_EVENT_HANDLED;
}

ZMK_LISTENER(ecorne_wpm_relay, ecorne_wpm_relay_cb);
ZMK_SUBSCRIPTION(ecorne_wpm_relay, zmk_relay_event_received);

#endif
