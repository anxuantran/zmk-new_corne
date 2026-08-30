# Eyelash Corne — ZMK config

Personal ZMK firmware for an **Eyelash Corne (pre-Nov-2024 revision)**, nice!nano v2
in both halves, with a hand-written vertical OLED status screen and a bongo cat.

Vendor documentation is preserved in [`docs/`](docs/) — it is about the keyboard,
not about this fork.

> **This board is not [foostan's Corne](https://github.com/foostan/crkbd).**
> Standard `corne` firmware will not work on it.

---

## 1. Hardware facts

Verified against build output or the on-device console. These are the numbers you
need before touching anything.

| Item | Value |
|---|---|
| MCU | nice!nano v2 (nRF52840), both halves |
| Panel | I2C SSD1306 **128×32** @ `0x3c`, one per half |
| ZMK | `cormoran/zmk @ v0.3-branch+dya` (joystick/mouse fork) |
| Zephyr / LVGL | **3.5 / 8.3** — *not* 4.1 / 9, whatever older notes say |
| Left half | central, ZMK Studio, speaks HID to the host |
| Right half | split peripheral, relays over BLE, never types over USB |
| Left USB serial | `D57CC9A693D49955` |
| Right USB serial | `F0371BDE6D5C2213` |

Both halves share VID `0x1D50` / PID `0x615E` and flash to `0x00026000`, so any
build here is a safe overwrite of any other.

Current usage, of 792 KB flash and 256 KB RAM:

| Build | Flash | RAM |
|---|---|---|
| `eyelash_corne_right` | 32.3% | 20.1% |
| `eyelash_corne_studio_left` | 44.3% | 34.4% |

---

## 2. Everyday workflow

You do not need a local toolchain. GitHub Actions builds everything.

```bash
git clone https://github.com/anxuantran/zmk-new_corne.git
cd zmk-new_corne

# ... make a change, then:
git commit -am "..." && git push

# wait for the build, then fetch the artifacts into ./firmware
gh run watch  $(gh run list -L1 --json databaseId -q '.[0].databaseId') --exit-status
gh run download $(gh run list -L1 --json databaseId -q '.[0].databaseId')
```

### Flashing

Put a half into its bootloader by **double-tapping its reset button**. It mounts as
`NICENANO`. Then:

```bash
python3 scripts/flash.py --dry-run   # shows which half is which, writes nothing
python3 scripts/flash.py             # writes the right image to each attached half
```

Do both halves at once if you like; the script identifies each by USB serial and
writes the matching image.

**Never map a half by volume name.** With both attached they mount as `NICENANO`
and `NICENANO 1` in arbitrary order, and the order is not stable — during this
build `NICENANO 1` was the *right* half once and the *left* another time.
`scripts/flash.py` resolves volume → BSD disk → USB serial through IOKit instead.

**Use `dd`, never `cp`.** macOS fskit throws I/O errors copying to
`/Volumes/NICENANO`. `scripts/flash.py` already uses `dd`.

**An I/O error is not necessarily a failure.** The board often reboots before `dd`
accounts for the bytes, and you get `0 bytes transferred` on a write that
succeeded. Judge by what the device re-enumerates as, not by `dd`'s exit:

```bash
ls -d /Volumes/NICENANO*   # gone  -> it rebooted, i.e. it took the image
```

A UF2 bootloader only resets once it has a *complete* image, so a reboot is
positive evidence.

---

## 3. If you blow up the firmware

Nothing here is unrecoverable. The bootloader lives in a separate flash region and
a bad application image cannot damage it.

**A half won't boot, screen dead, or acting strange.** Double-tap reset to force the
bootloader and reflash. This is the whole recovery path in the normal case.

**Double-tap doesn't reach the bootloader.** Hold reset and tap it a second time
within about half a second; the timing is fussier than it looks. On these cases the
reset switch sits in a small opening — a paperclip helps.

**The halves won't pair** — console shows `bt_smp: pairing failed (peer reason 0x3)`
or `Security failed … err 4`. Their split bond is stale.

```bash
# put BOTH halves in the bootloader
python3 scripts/flash.py --reset   # writes settings_reset to both
# let BOTH boot fully (they enumerate as SETTINGS RESET while running it)
python3 scripts/flash.py           # then write real firmware to both
```

**Reset both halves together, never one.** Wiping one half's NVS while the other
keeps its bond is exactly what causes the pairing failure above. This also clears
your Bluetooth host pairings, so you will re-pair with your computer afterwards.

**You need to see what the device is actually doing.** Flash the console build and
read it. This is the *first* move on any display or pairing problem, not the last —
on the original fault four rounds of reasoning died before anything measured the
device, and one console build settled it.

```bash
python3 scripts/flash.py --logging   # console builds for whichever halves are attached
cat /dev/cu.usbmodem*              # ZMK buffers the boot log and flushes on connect
```

**Total loss of this repo.** Everything needed to rebuild is committed, including
the art generator. There are no binary blobs that cannot be regenerated:

```bash
python3 gen/gen_art.py boards/shields/eyelash_corne/display/art.c
```

That refetches the upstream bongo cat source at a pinned commit and reproduces
`art.c`/`art.h` byte for byte.

---

## 4. Changing the display

Everything lives in [`boards/shields/eyelash_corne/display/`](boards/shields/eyelash_corne/display/).

```
display/
  canvas.c/.h      rotation + drawing helpers. Read canvas.h first.
  status_screen.c  layout constants, widgets, zmk_display_status_screen()
  wpm_relay.c/.h   carries typing activity central -> peripheral
  art.c/.h         GENERATED -- edit gen/gen_art.py, not these
gen/gen_art.py     produces art.c/art.h
```

### The one idea you need

The panel is 128×32 but **mounted with its long axis running front-to-back**, so
what you actually read is a strip **32 wide and 128 tall**.

Widgets are authored in that upright strip, and the whole strip is rotated once,
in a flat blit, when `ecorne_commit()` runs. Nothing rotates at runtime beyond that
single copy.

It works this way because the escape routes are all closed: the SSD1306 driver has
no `.set_orientation`, devicetree offers 180° only, and LVGL 8.3 has no usable
software display rotation. `lv_display_set_rotation()` is LVGL **9** and does not
exist in this tree.

### Moving things around

All layout is `#define`s at the top of `status_screen.c`, in strip coordinates
(x 0–31, y 0–127):

| | Left (central) | Right (peripheral) |
|---|---|---|
| Battery bar | y 14 | y 8 |
| Battery `%` | y 30 | y 24 |
| Bluetooth + profile | y 64 | y 44 (glyph only) |
| Layer name | y 100 | — |
| Bongo cat | — | y 70 |

**Keep the right half clear of y 64–127.** That region is the cat's. The status
stack on the right is shifted up specifically so nothing crosses into it.

### Text is capped at 4 characters

`lv_font_unscii_8` is a fixed 8px per glyph and the strip is 32px, so four
characters is the hard limit. This is why layer names are truncated.

unscii is used because it is a *bitmap* font. Montserrat and the other LVGL
built-ins are antialiased, and antialiasing dithers into noise on a 1-bit panel.

### Layer names

Read from each layer's `display-name` in [`config/eyelash_corne.keymap`](config/eyelash_corne.keymap),
uppercased and truncated to 4. **Relabelling is a keymap edit, not a firmware
change** — rename `display-name = "NUMBER"` to `"NUM"` and that is what appears.
An unnamed layer falls back to its index, so the row is never blank.

### The bongo cat

Driven by WPM, with the same thresholds as the upstream QMK version: idle below
10 WPM, a settled pose to 20, alternating tap frames above that.

The right half is a split *peripheral* and never sees left-hand keypresses, so a
naive implementation only reacts to your right hand. This fork carries a generic
named-event relay (`CONFIG_ZMK_SPLIT_RELAY_EVENT`, enabled on both halves), so the
central's `zmk_wpm_state_changed` is relayed across — no bespoke split
characteristic. See `wpm_relay.c`.

Do **not** set `CONFIG_ZMK_WPM=y` on the peripheral. ZMK's `wpm.c` counts keycode
events, which are central-only, and it will fail to link:

```
wpm.c:38: undefined reference to `as_zmk_keycode_state_changed'
```

The peripheral decodes the raw relay event instead and needs no WPM subsystem.

Frames are only redrawn when the frame actually *changes*, so an idle cat costs no
redraw, no rotation and no I2C traffic. Animation on a peripheral does cost
battery — every frame is a 512-byte I2C transfer plus an LVGL refresh, and it
keeps the CPU out of deep sleep. Don't raise the frame rate casually.

### Changing the art

Edit `gen/gen_art.py` and regenerate. The Bluetooth glyph is drawn
programmatically and proportional — `BT_W, BT_H` are the only two numbers to
change. The cat is cropped from upstream and rotated 90° **at generation time**,
because geometric transforms at runtime are too expensive on the nRF52840.

A bongo cat cannot be upright here. The art is inherently landscape and the strip
is 32px wide; it is rotated to fit, which is the same thing `zmk-nice-oled` does
with its own cat, for the same reason.

---

## 5. Display troubleshooting

Each of these is a one-line change.

| Symptom | Fix |
|---|---|
| Both halves upside down | remove `segment-remap`/`com-invdir` from `oled` in `eyelash_corne.dtsi` |
| Both rotated 90° the wrong way | flip `ECORNE_ROTATE_CW` in `display/canvas.h` |
| **One** half upside down | the two panels disagree — move the devicetree properties into that half's overlay only |
| Dark-on-light | swap `ECORNE_FG`/`ECORNE_BG` in `canvas.h` **and** the palette in `gen/gen_art.py`; they must agree |
| Screen blank | flash the console build and confirm `ssd1306_init` succeeds |

`ssd1306_init` succeeding is *real* evidence: I2C requires a slave ACK. Do not
accept "the display driver initialised" from an SPI driver as proof of anything —
SPI here is write-only, so a nice!view driver will happily log successful frame
writes into an empty bus. That is precisely what hid the original fault.

---

## 6. Gotchas

- `LV_Z_VDB_SIZE` is a **percentage**, range `[1,100]` — not a byte count. Setting
  it to `128` clamps and aborts the build on a Kconfig warning.
- `CONFIG_ZMK_DISPLAY` is **not** implied. The `nice_view` shield used to supply it;
  without that shield it must be set explicitly.
- `CONFIG_LV_USE_CANVAS` and `CONFIG_LV_USE_IMG` are **off** by default in Zephyr's
  LVGL config. The status screen needs both.
- A shield's `CMakeLists.txt` is compiled automatically when that shield is
  selected. That is how the display sources get built — the same mechanism ZMK's
  own `nice_view` shield uses. `zmk-config-zen-2`, sometimes recommended as a
  template for this, ships an **empty** `CMakeLists.txt` and compiles nothing.
- CI's "Merge Output Artifacts" step can 404 on an otherwise green build. Per-build
  artifacts are still downloadable — check per-job status, not just the run.
- The right half has no USB product name. Expected: `ZMK_KEYBOARD_NAME` is set only
  inside `if SHIELD_EYELASH_CORNE_LEFT`.
- Plugging in the right half proves nothing about whether it works. It is the
  peripheral; only the left speaks HID to the host.

---

## 7. Build targets

Defined in [`build.yaml`](build.yaml):

| Artifact | Purpose |
|---|---|
| `eyelash_corne_studio_left` | left half, normal use, ZMK Studio |
| `eyelash_corne_right` | right half, normal use |
| `eyelash_corne_logging_left` | left with USB console |
| `eyelash_corne_logging_right` | right with USB console |
| `settings_reset-nice_nano_v2` | wipes NVS — flash to **both** halves |

## Credits

Bongo cat art from [dancarroll/qmk-bongo](https://github.com/dancarroll/qmk-bongo)
(GPL-2.0), originally by [@pixelbenny](https://twitter.com/pixelbenny) via
j-inc's Kyria keymap. Cropped and rotated by `gen/gen_art.py`; the upstream
commit is pinned in that script.
