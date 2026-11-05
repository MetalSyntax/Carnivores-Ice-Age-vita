#!/usr/bin/env python3
"""
Build extras/trophy/CRIA00001_00/TROPHY.TRP for Carnivores Ice Age.

Trophy N (1..18) = engine achievement N-1 (SocialUtils.unlockAchievement(id),
see source/java.c); 0 is the platinum. Same TRP layout, signature placeholder
and 8-bit icons as the Prince of Persia Classic port (works with NoTrpDrm).

Icons come from the APK's help pages (carnivoresiceage_extract/assets/Help)
and the cover art in the repo root. Usage (from the repo root):
    python3 extras/scripts/generate_trophies.py
"""

import io
import os
import struct
import hashlib
from PIL import Image, ImageChops, ImageDraw

COMM_ID = "CRIA00001_00"
HELP = "carnivoresiceage_extract/assets/Help"
COVER = "Carnivores- Ice Age.webp"

# (ttype, hidden, name, detail, icon) -- order = engine achievement id 0..17.
ACHIEVEMENTS = [
    ("B", "no",  "Scientist",          "Read the whole hunting guide.",                    "button_binocular_ipad.png"),
    ("B", "no",  "First Blood",        "Bring down your first animal.",                    "animal_01_ipad.png"),
    ("B", "no",  "Ornithologist",      "Shoot a bird.",                                    "other_animal_01_ipad.png"),
    ("B", "no",  "Swimmer",            "Swim 100 meters in a single hunt.",                "other_animal_02_ipad.png"),
    ("B", "no",  "Animal Activist",    "Take an animal down in tranquilizer mode.",        "animal_02_ipad.png"),
    ("B", "no",  "Hunter",             "Bag one of the great Ice Age beasts.",             "animal_03_ipad.png"),
    ("S", "no",  "Explorer",           "Hunt in every area.",                              "button_compass_ipad.png"),
    ("S", "no",  "Marathoner",         "Cover a marathon (42,195 m) on foot in total.",    "animal_04_ipad.png"),
    ("S", "no",  "Sniper",             "Finish a hunt with a kill and no missed shots.",   "button_fire_ipad.png"),
    ("B", "no",  "Sharpshooter",       "Make two kills within 20 seconds.",                "button_weapon_ipad.png"),
    ("B", "no",  "Last Second Save",   "Bring down a great beast at close range.",         "animal_05_ipad.png"),
    ("B", "no",  "To the Bitter End",  "Make a kill with your last round.",                "animal_06_ipad.png"),
    ("G", "no",  "Poacher",            "Hunt every species many times over.",              "animal_07_ipad.png"),
    ("B", "no",  "Amateur",            "Reach 200 total hunting points.",                  "animal_08_ipad.png"),
    ("S", "no",  "Advanced",           "Reach 500 total hunting points.",                  "animal_09_ipad.png"),
    ("G", "no",  "Expert",             "Reach 1000 total hunting points.",                 "animal_11_ipad.png"),
    ("S", "no",  "Total Annihilation", "Leave no animal alive on the map in a hunt.",      "animal_12_ipad.png"),
    ("G", "no",  "Collector",          "Bag every one of the great Ice Age species.",      "animal_13_ipad.png"),
]

TROPHIES = [{"id": "000", "ttype": "P", "hidden": "no", "pid": "-1",
             "name": "Carnivores: Ice Age", "detail": "Unlock every other trophy."}]
for i, (tt, hid, name, detail, _) in enumerate(ACHIEVEMENTS, 1):
    TROPHIES.append({"id": f"{i:03d}", "ttype": tt, "hidden": hid, "pid": "000",
                     "name": name, "detail": detail})

RING = {"P": (200, 220, 235), "G": (222, 180, 70), "S": (190, 195, 205), "B": (176, 110, 60)}


def sfm(with_text):
    sig = "x" * 320  # same placeholder length as working NoTrpDrm packs
    lines = [f"<!--Sce-Np-Trophy-Signature: {sig}-->",
             '<trophyconf version="1.1" platform="psp2" policy="large">',
             f" <npcommid>{COMM_ID}</npcommid>",
             " <trophyset-version>01.00</trophyset-version>",
             ' <parental-level license-area="default">0</parental-level>']
    if with_text:
        lines += [" <title-name>Carnivores: Ice Age</title-name>",
                  " <title-detail>Carnivores: Ice Age PlayStation Vita Port</title-detail>"]
    for t in TROPHIES:
        attrs = f'id="{t["id"]}" hidden="{t["hidden"]}" ttype="{t["ttype"]}" pid="{t["pid"]}"'
        if with_text:
            lines += [f" <trophy {attrs}>", f'  <name>{t["name"]}</name>',
                      f'  <detail>{t["detail"]}</detail>', " </trophy>"]
        else:
            lines.append(f" <trophy {attrs}/>")
    lines.append("</trophyconf>\n")
    return "\n".join(lines).encode("utf-8")


def icon(art, ttype, fit=176):
    """240x240: art on a dark ice-blue disc with a grade-colored ring."""
    c = Image.new("RGBA", (240, 240), (0, 0, 0, 0))
    d = ImageDraw.Draw(c)
    d.ellipse((4, 4, 235, 235), fill=RING[ttype])
    d.ellipse((14, 14, 225, 225), fill=(18, 34, 52))
    art = art.convert("RGBA")
    k = min(fit / art.width, fit / art.height, 2.0)  # small HUD icons: 2x at most
    art = art.resize((round(art.width * k), round(art.height * k)), Image.Resampling.LANCZOS)
    layer = Image.new("RGBA", (240, 240), (0, 0, 0, 0))
    layer.paste(art, ((240 - art.width) // 2, (240 - art.height) // 2), art)
    disc = Image.new("L", (240, 240), 0)
    ImageDraw.Draw(disc).ellipse((14, 14, 225, 225), fill=255)
    c.paste(layer, (0, 0), ImageChops.multiply(layer.split()[3], disc))
    return c


def to_png8(img):
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    buf.seek(0)
    return png8(buf)


def png8(path):
    """8-bit palette PNG with transparency, like working NoTrpDrm packs.
    The full-RGBA ~100 KB icons are far above what retail trophy packs ship
    and the set install failed with NP-6185-0 ("data corrupted")."""
    img = Image.open(path).convert("RGBA").quantize(256, method=Image.Quantize.FASTOCTREE)
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()

def build_trp(file_dict, output_path):
    def align64(v):
        return (v + 63) & ~63

    num_files = len(file_dict)
    header_size = 64
    entries_size = num_files * 64
    first_file_offset = align64(header_size + entries_size)

    current_offset = first_file_offset
    file_offsets = {}
    for name, data in file_dict.items():
        file_offsets[name] = (current_offset, len(data))
        current_offset = align64(current_offset + len(data))

    total_size = current_offset

    # The 20-byte header field is a plain SHA-1 CONTENT hash, not a Sony
    # signature: it is SHA-1 over the whole file with this field zeroed.
    # Verified against Rinnegatamante's working Eldritch TROPHY.TRP, whose
    # header hash recomputes exactly. NoTrpDrm bypasses SIGNATURE checks,
    # but the firmware still verifies this hash during
    # sceNpTrophySetupDialogInit -- a zeroed field fails install (0x800204C4
    # -> SETUP_REQUIRED, no system notification). Assemble with zeros first,
    # then hash and patch.
    sha_dummy = b"\x00" * 20
    header = struct.pack(">4sIQIII20s16s",
                         b"\xdc\xa2\x4d\x00",
                         2,
                         total_size,
                         num_files,
                         64,
                         0,
                         sha_dummy,
                         b"\x00" * 16)

    entries = bytearray()
    for name, data in file_dict.items():
        offset, size = file_offsets[name]
        entry = struct.pack(">32sQQI12s",
                            name.encode("latin1"),
                            offset,
                            size,
                            0,
                            b"\x00" * 12)
        entries.extend(entry)

    out = bytearray()
    out.extend(header)
    out.extend(entries)

    pad_len = first_file_offset - len(out)
    out.extend(b"\x00" * pad_len)

    for name, data in file_dict.items():
        target_offset, size = file_offsets[name]
        current_pos = len(out)
        if target_offset > current_pos:
            out.extend(b"\x00" * (target_offset - current_pos))
        out.extend(data)

    final_pad = total_size - len(out)
    if final_pad > 0:
        out.extend(b"\x00" * final_pad)

    # Patch the real content hash into the header (bytes 28..48).
    real_sha = hashlib.sha1(bytes(out)).digest()
    out[28:48] = real_sha

    with open(output_path, "wb") as f:
        f.write(out)
    print(f"TRP written to {output_path} ({len(out)} bytes, header sha1={real_sha.hex()})")


def main():
    cover = Image.open(COVER).convert("RGBA")
    files = {"TROPCONF.SFM": sfm(False), "TROP.SFM": sfm(True)}
    banner = Image.new("RGBA", (320, 176), (0, 0, 0, 0))
    art = cover.copy()
    art.thumbnail((176, 176), Image.Resampling.LANCZOS)
    banner.paste(art, ((320 - art.width) // 2, 0))
    files["ICON0.PNG"] = to_png8(banner)
    files["TROP000.PNG"] = to_png8(icon(cover, "P", fit=212))
    for i, (tt, _, _, _, img) in enumerate(ACHIEVEMENTS, 1):
        files[f"TROP{i:03d}.PNG"] = to_png8(icon(Image.open(os.path.join(HELP, img)), tt))
    outdir = f"extras/trophy/{COMM_ID}"
    os.makedirs(outdir, exist_ok=True)
    build_trp(files, os.path.join(outdir, "TROPHY.TRP"))


if __name__ == "__main__":
    main()
