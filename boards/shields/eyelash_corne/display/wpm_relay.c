/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * The right half is a split peripheral: key positions travel peripheral ->
 * central, and HID indicators travel central -> peripheral, but nothing tells
 * the peripheral that the *left* hand is typing. Left alone, a WPM-driven bongo
 * cat on the right screen would only respond to right-hand keys.
 *
 * This fork carries a generic named-event relay (CONFIG_ZMK_SPLIT_RELAY_EVENT,
 * already enabled on both halves), so the fix is to relay the central's
 * zmk_wpm_state_changed rather than to invent a split characteristic.
 *
 * Budget check against the fork's defaults:
 *   sizeof(struct zmk_wpm_state_changed) == 4  <= ZMK_SPLIT_RELAY_EVENT_DATA_LEN (14)
 *   sizeof("wpm")                        == 4  <= ZMK_SPLIT_RELAY_EVENT_TYPE_NAME_LEN (4)
 * Both are BUILD_ASSERTed by the macros below, so a future rename that outgrows
 * the identifier will fail the build rather than silently stop animating.
 */

#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/* Central: forward every WPM change to the peripheral. */
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(zmk_wpm_state_changed, wpm, )

#else

/* Peripheral: re-raise the relayed event locally so the status screen's
 * ordinary zmk_wpm_state_changed subscription picks it up unchanged. */
ZMK_RELAY_EVENT_HANDLE(zmk_wpm_state_changed, wpm, )

#endif
