#!/usr/bin/env python3
"""Generate pre-rotated 1bpp LVGL 8 art for the Eyelash Corne vertical OLED.

Bongo cat frames come from dancarroll/qmk-bongo (GPL-2.0), art by @pixelbenny.
They are authored 128x32 landscape; the strip is 32 wide, so they are cropped
to the cat's body and rotated 90 deg at build time -- never at runtime.
"""
import re, sys, os, subprocess

# Pinned so regenerating the art years from now produces the same bytes.
UPSTREAM = ('https://raw.githubusercontent.com/dancarroll/qmk-bongo/'
            'cb60a2789960ca934254452848c4d1b6871f0cd4/bongo_cat.c')
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bongo_cat.c.cache')

def source():
    """Local --src wins; otherwise fetch upstream once and cache it."""
    if '--src' in sys.argv:
        return open(sys.argv[sys.argv.index('--src') + 1]).read()
    if not os.path.exists(CACHE):
        print('fetching %s' % UPSTREAM)
        subprocess.run(['curl', '-fsSL', '-o', CACHE, UPSTREAM], check=True)
    return open(CACHE).read()

# --- crop window over the 128x32 source, chosen to hold the cat body and
# --- drop the long table diagonal that runs off both edges.
CX0, CX1 = 44, 100     # 56 wide  -> 56 tall once rotated (box allows 60)
CY0, CY1 = 0, 28       # 28 tall  -> 28 wide once rotated (box allows 32)
ROT = 'CW'             # flip to 'CCW' if the cat faces the wrong way on hardware

def qmk_frames():
    src = source()
    def block(name):
        i = src.index(name); j = src.index('{', i); d = 0
        for k in range(j, len(src)):
            if src[k] == '{': d += 1
            elif src[k] == '}':
                d -= 1
                if d == 0: return src[j:k+1]
        raise SystemExit('unterminated array: ' + name)
    nums = lambda t: [int(x) for x in re.findall(r'\b\d+\b', t)]
    anim = re.findall(r'\{([^{}]*)\}', block('animation_frames[TAP_FRAMES]'))
    return {'tap0': nums(anim[0]), 'tap1': nums(anim[1]),
            'ready': nums(block('ready_frame[ANIM_SIZE]')),
            'waiting': nums(block('waiting_frame[]'))}

def unpack(data, W=128, PAGES=4):
    """QMK page format -> [row][col] bit grid. Byte = 8 vertical px, LSB top."""
    g = [[0] * W for _ in range(PAGES * 8)]
    for p in range(PAGES):
        for c in range(W):
            b = data[p * W + c]
            for bit in range(8):
                if b >> bit & 1: g[p * 8 + bit][c] = 1
    return g

def crop(g): return [row[CX0:CX1] for row in g[CY0:CY1]]
def rot_cw(g):
    h, w = len(g), len(g[0]); return [[g[h - 1 - r][c] for r in range(h)] for c in range(w)]
def rot_ccw(g):
    h, w = len(g), len(g[0]); return [[g[r][w - 1 - c] for r in range(h)] for c in range(w)]

def bresenham(g, x0, y0, x1, y1):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        if 0 <= y0 < len(g) and 0 <= x0 < len(g[0]): g[y0][x0] = 1
        if x0 == x1 and y0 == y1: break
        e2 = 2 * err
        if e2 >= dy: err += dy; x0 += sx
        if e2 <= dx: err += dx; y0 += sy

# Bluetooth glyph size. The strip is only 32 px wide, so this is deliberately
# small; the geometry below is proportional, so these are the only two numbers
# to change if it wants resizing again.
BT_W, BT_H = 9, 12

def bt_glyph(slashed):
    """Bluetooth rune, authored upright (the strip's own axis).

    Only the top half is drawn; the bottom is mirrored, which is what keeps the
    two lobes identical at these sizes."""
    W, H = BT_W, BT_H
    half = H // 2
    g = [[0] * W for _ in range(H)]
    cx, right, left = (W - 1) // 2, W - 2, 1
    lobe_y = max(1, round(H * 0.22))

    bresenham(g, cx, 0, cx, half - 1)          # spine, top half
    bresenham(g, cx, 0, right, lobe_y)         # lobe out
    bresenham(g, right, lobe_y, cx, half - 1)  # lobe back to the waist
    bresenham(g, left, lobe_y, cx - 1, half - 1)  # cross stroke into the waist

    for y in range(half):                      # mirror -> guaranteed symmetry
        g[H - 1 - y] = list(g[y])

    if slashed:
        # One pixel of clearance either side. More than that eats the rune at
        # this size; less and the strike merges into the strokes.
        for y in range(H):
            x = int(round((W - 1) * y / (H - 1)))
            for dx in (-1, 0, 1):
                if 0 <= x + dx < W: g[y][x + dx] = 0
            g[y][x] = 1
    return g

def emit(name, g, out):
    h, w = len(g), len(g[0]); stride = (w + 7) // 8
    body = []
    for row in g:
        bits = ''.join(str(v) for v in row).ljust(stride * 8, '0')
        body.append([int(bits[i:i + 8], 2) for i in range(0, stride * 8, 8)])
    flat = [b for row in body for b in row]
    out.write(f"\n/* {name}: {w}x{h}, 1bpp */\n")
    out.write(f"static const uint8_t {name}_map[] = {{\n")
    out.write("    ECORNE_ART_PALETTE\n")
    for i in range(0, len(flat), 12):
        out.write('    ' + ' '.join(f'0x{b:02x},' for b in flat[i:i + 12]) + '\n')
    out.write("};\n")
    out.write(f"const lv_img_dsc_t {name} = {{\n"
              f"    .header.cf = LV_IMG_CF_INDEXED_1BIT,\n"
              f"    .header.always_zero = 0,\n"
              f"    .header.reserved = 0,\n"
              f"    .header.w = {w},\n"
              f"    .header.h = {h},\n"
              f"    .data_size = {8 + len(flat)},\n"
              f"    .data = {name}_map,\n}};\n")
    return w, h

def main():
    rot = rot_cw if ROT == 'CW' else rot_ccw
    frames = qmk_frames()
    art = {}
    for n in ('ready', 'waiting', 'tap0', 'tap1'):
        art['bongo_' + n] = rot(crop(unpack(frames[n])))
    art['bt_on'] = bt_glyph(False)
    art['bt_off'] = bt_glyph(True)

    if '--preview' in sys.argv:
        for n, g in art.items():
            print(f"--- {n}: {len(g[0])}w x {len(g)}h ---")
            for row in g: print('|' + ''.join('#' if v else '.' for v in row) + '|')
        return

    dst = [a for a in sys.argv[1:] if not a.startswith('--')
           and sys.argv[sys.argv.index(a) - 1] != '--src'][0]
    with open(dst, 'w') as out:
        out.write("""/*
 * GENERATED by gen/gen_art.py -- do not edit by hand.
 *
 * Bongo cat frames derived from https://github.com/dancarroll/qmk-bongo
 * (GPL-2.0), original art by @pixelbenny, via j-inc's Kyria keymap.
 * Cropped to the cat body and rotated 90 deg at generation time so the
 * nRF52840 never rotates anything at runtime.
 *
 * Bluetooth glyph is original to this repo.
 */

#include "art.h"

/* Index 0 = unlit, index 1 = lit. Swap these two lines if the panel comes
 * back inverted -- it is the only place polarity is decided. */
#define ECORNE_ART_PALETTE \\
    0x00, 0x00, 0x00, 0xff,  0xff, 0xff, 0xff, 0xff,
""")
        dims = {}
        for n in ('bongo_ready', 'bongo_waiting', 'bongo_tap0', 'bongo_tap1', 'bt_on', 'bt_off'):
            dims[n] = emit(n, art[n], out)
    hdr = os.path.join(os.path.dirname(dst), 'art.h')
    with open(hdr, 'w') as out:
        out.write("/* GENERATED by gen/gen_art.py -- do not edit by hand. */\n#pragma once\n\n#include <lvgl.h>\n\n")
        for n, (w, h) in dims.items():
            out.write(f"#define {n.upper()}_W {w}\n#define {n.upper()}_H {h}\n")
        out.write('\n')
        for n in dims: out.write(f"LV_IMG_DECLARE({n});\n")
    print("wrote %s and %s" % (dst, hdr))
    for n, (w, h) in dims.items(): print("  %-14s %2dw x %2dh" % (n, w, h))

main()
