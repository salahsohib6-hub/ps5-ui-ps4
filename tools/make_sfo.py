#!/usr/bin/env python3
"""Write a PS4 param.sfo (PSF container) for a homebrew app.

No Sony files or keys are involved: param.sfo is a plain key/value container.
Usage: python3 tools/make_sfo.py OUT.sfo [--title T --title-id ABCD12345 ...]
"""
import argparse, struct, sys

def build_sfo(entries):
    """entries: dict name -> ('utf8', value, maxlen) | ('int', value)"""
    names = sorted(entries)
    key_table = bytearray()
    key_offsets = []
    for n in names:
        key_offsets.append(len(key_table))
        key_table += n.encode("ascii") + b"\0"
    while len(key_table) % 4:
        key_table.append(0)

    data_table = bytearray()
    index = bytearray()
    header_size = 20
    key_start = header_size + 16 * len(names)
    data_start = key_start + len(key_table)
    for n, koff in zip(names, key_offsets):
        e = entries[n]
        doff = len(data_table)
        if e[0] == "utf8":
            raw = e[1].encode("utf-8") + b"\0"
            maxlen = e[2]
            if len(raw) > maxlen:
                raise ValueError(f"{n}: value longer than {maxlen} bytes")
            data_table += raw + b"\0" * (maxlen - len(raw))
            index += struct.pack("<HHIII", koff, 0x0204, len(raw), maxlen, doff)
        elif e[0] == "int":
            data_table += struct.pack("<I", e[1] & 0xFFFFFFFF)
            index += struct.pack("<HHIII", koff, 0x0404, 4, 4, doff)
        else:
            raise ValueError(e[0])
    header = struct.pack("<4sIIII", b"\0PSF", 0x101, key_start, data_start, len(names))
    return bytes(header + index + key_table + data_table)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--title", default="PS5 Emulator UI for PS4")
    ap.add_argument("--title-id", default="PSUI00001")
    ap.add_argument("--content-id", default="IV0000-PSUI00001_00-0000000000000000")
    ap.add_argument("--version", default="01.00")
    ap.add_argument("--app-ver", default="01.00")
    ap.add_argument("--system-ver", type=lambda s: int(s, 0), default=0)
    ap.add_argument("--pubtoolinfo", default="c_date=20260101,sdk_ver=05050000,img0_l0_size=0,img0_l1_size=0,img0_sc_ms=0,img1_l0_size=0,img1_l1_size=0")
    a = ap.parse_args()
    if len(a.title_id) != 9:
        sys.exit("TITLE_ID must be 9 characters (4 letters + 5 digits)")
    if len(a.content_id) != 36:
        sys.exit("CONTENT_ID must be 36 characters")
    entries = {
        "APP_TYPE": ("int", 0),
        "APP_VER": ("utf8", a.app_ver, 8),
        "ATTRIBUTE": ("int", 0),
        "CATEGORY": ("utf8", "gd", 4),
        "CONTENT_ID": ("utf8", a.content_id, 48),
        "DOWNLOAD_DATA_SIZE": ("int", 0),
        "FORMAT": ("utf8", "obs", 4),
        "PARENTAL_LEVEL": ("int", 1),
        "PUBTOOLINFO": ("utf8", a.pubtoolinfo, 512),
        "SYSTEM_VER": ("int", a.system_ver),
        "TITLE": ("utf8", a.title, 128),
        "TITLE_ID": ("utf8", a.title_id, 12),
        "VERSION": ("utf8", a.version, 8),
    }
    with open(a.out, "wb") as f:
        f.write(build_sfo(entries))
    print("wrote", a.out)

if __name__ == "__main__":
    main()
