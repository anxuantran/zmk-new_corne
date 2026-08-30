# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT
#
# Zephyr compiles a shield's CMakeLists.txt when that shield is selected; this
# is the same mechanism ZMK's own nice_view shield uses to ship a custom status
# screen. Sources are relative to this directory.

if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)

  zephyr_library_sources(display/canvas.c)
  zephyr_library_sources(display/art.c)
  zephyr_library_sources(display/status_screen.c)

  # Carries typing activity from the central to the peripheral so the cat on
  # the right half reacts to both hands, not just the right one. Needed on both
  # sides: the central sends, the peripheral decodes.
  if(CONFIG_ZMK_SPLIT_RELAY_EVENT)
    zephyr_library_sources(display/wpm_relay.c)
  endif()
endif()
