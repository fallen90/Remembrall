#!/usr/bin/env python3
"""Generate Remembrall Windows .ico + transparent PNG from source artwork."""

from __future__ import annotations

import io
import shutil
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
APP = ROOT / "src" / "app"
SIZES = [16, 24, 32, 48, 64, 128, 256]


def find_source() -> Path:
    preferred = ASSETS / "remembrall-source.jpg"
    if preferred.exists():
        return preferred
    for pattern in ("*.jpg", "*.jpeg", "*.png", "*.webp"):
        matches = sorted(ASSETS.glob(pattern))
        matches = [m for m in matches if m.name not in {"remembrall.png"}]
        if matches:
            return matches[0]
    raise SystemExit(f"No source image found under {ASSETS}")


def knockout_black(img: Image.Image) -> Image.Image:
    img = img.convert("RGBA")
    pixels = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, _ = pixels[x, y]
            if r < 18 and g < 18 and b < 18:
                pixels[x, y] = (r, g, b, 0)
            elif r < 28 and g < 28 and b < 28:
                alpha = int(255 * (max(r, g, b) / 28.0))
                pixels[x, y] = (r, g, b, alpha)
    return img


def png_bytes(im: Image.Image) -> bytes:
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    return buf.getvalue()


def write_ico(path: Path, master: Image.Image, sizes: list[int]) -> None:
    """Write a multi-size PNG-compressed ICO (Vista+)."""
    entries: list[tuple[int, bytes]] = []
    for s in sizes:
        frame = master.resize((s, s), Image.Resampling.LANCZOS)
        entries.append((s, png_bytes(frame)))

    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory = b""
    blobs = b""
    for s, data in entries:
        w = 0 if s >= 256 else s
        h = 0 if s >= 256 else s
        directory += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    path.write_bytes(header + directory + blobs)


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    APP.mkdir(parents=True, exist_ok=True)
    src = find_source()
    img = knockout_black(Image.open(src))

    png_path = ASSETS / "remembrall.png"
    ico_path = ASSETS / "remembrall.ico"
    img.save(png_path, "PNG")
    write_ico(ico_path, img, SIZES)
    shutil.copy2(ico_path, APP / "remembrall.ico")
    print(f"Source: {src}")
    print(f"Wrote {png_path} ({png_path.stat().st_size} bytes)")
    print(f"Wrote {ico_path} ({ico_path.stat().st_size} bytes, {len(SIZES)} sizes)")
    print(f"Copied -> {APP / 'remembrall.ico'}")


if __name__ == "__main__":
    main()
