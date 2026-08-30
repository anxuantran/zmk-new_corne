#!/usr/bin/env python3
"""Flash the Eyelash Corne halves, identifying each by USB serial.

Usage:  python3 scripts/flash.py [--dry-run] [--logging] [--reset]
Reads .uf2 files from ./firmware, or $FW_DIR.

Both halves report Board-ID nRF52840-nicenano and mount as NICENANO and
NICENANO 1 in arbitrary order, so volume name is never a safe way to tell them
apart. This maps volume -> BSD disk -> USB serial through IOKit first.
"""
import plistlib, re, subprocess, sys, os

LEFT = "D57CC9A693D49955"
RIGHT = "F0371BDE6D5C2213"
FW = os.environ.get("FW_DIR", os.path.join(os.getcwd(), "firmware"))

def serial_to_disks():
    out = subprocess.run(["ioreg", "-w0", "-l", "-p", "IOService", "-r",
                          "-c", "IOUSBHostDevice"], capture_output=True, text=True).stdout
    found, serial = {}, None
    for line in out.splitlines():
        m = re.search(r'"USB Serial Number" = "([^"]+)"', line)
        if m:
            serial = m.group(1)
            found.setdefault(serial, set())
        m = re.search(r'"BSD Name" = "(disk\d+)"', line)
        if m and serial:
            found[serial].add(m.group(1))
    return found

def bootloader_volumes():
    vols = []
    for name in os.listdir("/Volumes"):
        path = os.path.join("/Volumes", name)
        info = os.path.join(path, "INFO_UF2.TXT")
        if not os.path.exists(info):
            continue
        try:
            if "nicenano" not in open(info, errors="ignore").read().lower():
                continue
            plist = plistlib.loads(subprocess.run(
                ["diskutil", "info", "-plist", path], capture_output=True).stdout)
            vols.append((path, re.sub(r"s\d+$", "", plist["DeviceIdentifier"])))
        except Exception as e:
            print("  ! skipping %s: %s" % (path, e))
    return vols

def main():
    smap = serial_to_disks()
    vols = bootloader_volumes()
    if not vols:
        print("No nice!nano bootloader volumes mounted.")
        print("Double-tap the reset button on a half to enter the bootloader.")
        return 1

    targets = {LEFT: "eyelash_corne_studio_left.uf2", RIGHT: "eyelash_corne_right.uf2"}
    if "--logging" in sys.argv:
        targets = {LEFT: "eyelash_corne_logging_left.uf2",
                   RIGHT: "eyelash_corne_logging_right.uf2"}
    if "--reset" in sys.argv:
        targets = {LEFT: "settings_reset-nice_nano_v2-zmk.uf2",
                   RIGHT: "settings_reset-nice_nano_v2-zmk.uf2"}

    rc = 0
    for path, disk in vols:
        serial = next((s for s, d in smap.items() if disk in d), None)
        side = {LEFT: "LEFT", RIGHT: "RIGHT"}.get(serial)
        print("\n%s  ->  %s  serial=%s  %s" % (path, disk, serial, side or "UNKNOWN"))
        if side is None:
            print("  ! serial not recognised -- refusing to write")
            rc = 1
            continue
        if "--dry-run" in sys.argv:
            print("  would write %s" % targets[serial])
            continue
        src = os.path.join(FW, targets[serial])
        print("  writing %s" % os.path.basename(src))
        # An I/O error here is not always a failure: the board reboots before dd
        # accounts for the bytes. Confirm by what the device re-enumerates as.
        p = subprocess.run(["dd", "if=src".replace("src", src),
                            "of=%s/fw.uf2" % path, "bs=1m"], capture_output=True, text=True)
        print("  " + p.stderr.strip().replace("\n", "\n  "))
        subprocess.run(["sync"])
    return rc

sys.exit(main())
