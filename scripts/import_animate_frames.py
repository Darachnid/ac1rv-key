#!/usr/bin/env python3
"""Replace AC1RV Key's 14 PNG animation poses from an Adobe Animate SVG ZIP.

Requires Inkscape installed on the machine running the script.
Usage: python3 scripts/import_animate_frames.py ~/Downloads/keyer-bird-frames.zip

This only updates resources/images/keyer/*.png, leaving Morse timing and app code alone.
"""
from pathlib import Path
from zipfile import ZipFile
import argparse
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "resources" / "images" / "keyer"
# 1 = home; 21 = first contact/poised; 24 = fully depressed.
# These preserve the 14-frame state machine in src/c/main.c.
SOURCE_FRAMES = [1, 4, 7, 10, 13, 16, 18, 19, 21, 22, 22, 23, 23, 24]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("zip", type=Path, help="Adobe Animate SVG-sequence ZIP")
    parser.add_argument("--inkscape", default=None, help="Path to Inkscape executable")
    args = parser.parse_args()
    ink = args.inkscape or shutil.which("inkscape")
    if not ink:
        candidate = "/Applications/Inkscape.app/Contents/MacOS/inkscape"
        ink = candidate if Path(candidate).exists() else None
    if not ink:
        parser.error("Inkscape not found. Install Inkscape or supply --inkscape /path/to/inkscape")
    if not args.zip.is_file():
        parser.error("ZIP file not found: " + str(args.zip))
    with ZipFile(args.zip) as archive:
        found = {}
        for name in archive.namelist():
            if not name.lower().endswith(".svg"):
                continue
            match = re.search(r"(\d+)\.svg$", Path(name).name, flags=re.I)
            if match:
                found[int(match.group(1))] = name
        missing = sorted(set(SOURCE_FRAMES) - found.keys())
        if missing:
            parser.error("Missing frames: " + ", ".join(map(str, missing)))
        DEST.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory() as tmp:
            tmpdir = Path(tmp)
            for output_index, source_index in enumerate(SOURCE_FRAMES, 1):
                svg = tmpdir / f"source{source_index:04d}.svg"
                svg.write_bytes(archive.read(found[source_index]))
                output = DEST / f"{output_index:03d}.png"
                subprocess.run([
                    ink, str(svg), "--export-type=png",
                    "--export-area-page", "--export-width=200", "--export-height=228",
                    f"--export-filename={output}",
                ], check=True, stdout=subprocess.DEVNULL)
                print(f"{output.relative_to(ROOT)} <- SVG frame {source_index}")
    print("Finished. Run pebble build to test the replacement animation.")


if __name__ == "__main__":
    main()
