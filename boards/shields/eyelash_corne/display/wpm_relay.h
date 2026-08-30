/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* Identifier the two halves agree on. Must fit in
 * CONFIG_ZMK_SPLIT_RELAY_EVENT_TYPE_NAME_LEN, which defaults to 4. */
#define ECORNE_WPM_RELAY_ID "wpm"

/*
 * Typing speed as last reported by the central, or 0 on the central itself
 * (which has zmk_wpm_get_state() and does not need this).
 */
uint8_t ecorne_relayed_wpm(void);
