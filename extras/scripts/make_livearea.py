#!/usr/bin/env python3
"""Regenerate extras/livearea/*.png from the original APK resources.

Usage: python3 extras/scripts/make_livearea.py [carnivoresiceage_extract]

Output follows references/livearea_assets.md: exact sizes, 8-bit indexed,
non-interlaced PNGs with no ancillary metadata (avoids VitaShell 0x8010113D).
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXTRACT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "carnivoresiceage_extract")
RES = os.path.join(EXTRACT, "res")
OUT = os.path.join(ROOT, "extras", "livearea")

ICON_SRC = os.path.join(RES, "drawable-xhdpi-v4", "ic_launcher_hd.png")
SPLASH_SRC = os.path.join(RES, "drawable-large-v4", "splash.png")


def cover(img, w, h):
    """Scale to fill w x h, then center-crop (keeps aspect ratio)."""
    scale = max(w / img.width, h / img.height)
    img = img.resize((round(img.width * scale), round(img.height * scale)), Image.LANCZOS)
    left = (img.width - w) // 2
    top = (img.height - h) // 2
    return img.crop((left, top, left + w, top + h))


def save_indexed(img, name):
    img = img.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=256)
    path = os.path.join(OUT, name)
    img.save(path, format="PNG", optimize=True)
    print(f"{path}: {img.size[0]}x{img.size[1]} indexed")


def main():
    icon = Image.open(ICON_SRC).convert("RGB")
    splash = Image.open(SPLASH_SRC).convert("RGB")

    save_indexed(icon.resize((128, 128), Image.LANCZOS), "icon0.png")
    save_indexed(cover(splash, 960, 544), "pic0.png")
    save_indexed(cover(splash, 840, 500), "bg0.png")
    save_indexed(cover(splash, 280, 158), "startup.png")


if __name__ == "__main__":
    main()
