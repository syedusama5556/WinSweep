"""Generates the WinSweep icon family.

One mark, three accents: blue for the WinUI app, amber for the Win32 app, green
for the console build. The mark is a W drawn as a single brush stroke, with a
sweep arc under it and dust flying off the last upstroke.

SVG is rendered by headless Chrome, then Pillow produces the PNG sizes and packs
the .ico files. Run it after changing anything here:

    python tools/make-icons.py
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ICON_DIR = ROOT / "docs" / "icons"

CHROME_CANDIDATES = [
    Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
    Path(r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"),
    Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
]

# name, accent, accent highlight, where the .ico belongs
VARIANTS = [
    ("winsweep", "#4C8DFF", "#8FBBFF", "src/winui/Assets/AppIcon.ico"),
    ("winsweep-win32", "#E8A33D", "#F7CE86", "src/win32/icon.ico"),
    ("winsweep-console", "#46D39A", "#8FEBC4", "src/batch/TempCleaner.ico"),
]

ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
PNG_SIZES = [32, 64, 128, 256, 512]

SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">
  <defs>
    <linearGradient id="tile" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#232B3A"/>
      <stop offset="55%" stop-color="#151B27"/>
      <stop offset="100%" stop-color="#0D111A"/>
    </linearGradient>
    <linearGradient id="stroke" x1="0.1" y1="0" x2="0.9" y2="1">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="100%" stop-color="#D9E2F2"/>
    </linearGradient>
    <linearGradient id="accent" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0%" stop-color="{accent}"/>
      <stop offset="100%" stop-color="{accent_hi}"/>
    </linearGradient>
    <radialGradient id="glow" cx="0.3" cy="0.22" r="0.85">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.16"/>
      <stop offset="60%" stop-color="#FFFFFF" stop-opacity="0"/>
    </radialGradient>
  </defs>

  <rect x="0" y="0" width="1024" height="1024" rx="232" fill="url(#tile)"/>
  <rect x="0" y="0" width="1024" height="1024" rx="232" fill="url(#glow)"/>
  <rect x="8" y="8" width="1008" height="1008" rx="228" fill="none"
        stroke="#FFFFFF" stroke-opacity="0.08" stroke-width="16"/>

  <g transform="translate(512 512) scale(0.9) translate(-512 -512)">
    <!-- the sweep: a tapered swoosh passing under the W, heavy on the left,
         thinning as it flicks up to the right -->
    <path d="M 168 812 C 348 908 616 900 846 704 L 902 772
             C 648 986 328 992 140 884 Z" fill="url(#accent)"/>
    <path d="M 168 812 C 348 908 616 900 846 704 L 902 772
             C 648 986 328 992 140 884 Z" fill="{accent}" opacity="0.3"
          transform="translate(-10,34)"/>

    <!-- W as one brush stroke, leaning into the sweep -->
    <g transform="rotate(-4 512 460)">
      <path d="M 250 244 L 376 640 L 512 386 L 648 640 L 774 244" fill="none"
            stroke="url(#stroke)" stroke-width="116"
            stroke-linecap="round" stroke-linejoin="round"/>
    </g>

    <!-- dust flicking off the end of the stroke -->
    <g fill="url(#accent)">
      <path d="M 864 150 l 36 84 84 36 -84 36 -36 84 -36 -84 -84 -36 84 -36 z"/>
      <circle cx="916" cy="368" r="32" opacity="0.7"/>
      <circle cx="836" cy="446" r="18" opacity="0.42"/>
    </g>
  </g>
</svg>
"""


def find_chrome() -> Path:
    for candidate in CHROME_CANDIDATES:
        if candidate.exists():
            return candidate
    sys.exit("No Chrome or Edge found to rasterise the SVG.")


def render(svg: str, out_png: Path, chrome: Path) -> None:
    """Headless Chrome screenshots the SVG on a transparent background."""
    with tempfile.TemporaryDirectory() as work:
        work_dir = Path(work)
        svg_file = work_dir / "icon.svg"
        svg_file.write_text(svg, encoding="utf-8")
        shot = work_dir / "shot.png"

        subprocess.run(
            [
                str(chrome),
                "--headless=new",
                "--disable-gpu",
                "--hide-scrollbars",
                "--force-device-scale-factor=1",
                "--default-background-color=00000000",
                f"--screenshot={shot}",
                "--window-size=1024,1024",
                f"--user-data-dir={work_dir / 'profile'}",
                svg_file.as_uri(),
            ],
            check=True,
            capture_output=True,
        )
        if not shot.exists():
            sys.exit("Chrome produced no screenshot.")
        out_png.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(shot, out_png)


def main() -> None:
    chrome = find_chrome()
    ICON_DIR.mkdir(parents=True, exist_ok=True)

    for name, accent, accent_hi, ico_target in VARIANTS:
        svg = SVG.format(accent=accent, accent_hi=accent_hi)
        (ICON_DIR / f"{name}.svg").write_text(svg, encoding="utf-8")

        master_png = ICON_DIR / f"{name}-1024.png"
        render(svg, master_png, chrome)
        master = Image.open(master_png).convert("RGBA")

        for size in PNG_SIZES:
            master.resize((size, size), Image.LANCZOS).save(ICON_DIR / f"{name}-{size}.png")

        ico_path = ROOT / ico_target
        ico_path.parent.mkdir(parents=True, exist_ok=True)
        master.save(ico_path, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
        print(f"{name}: {ico_path.relative_to(ROOT)}  ({ico_path.stat().st_size:,} bytes)")

    # Store tiles for the WinUI package manifest, drawn from the blue variant.
    master = Image.open(ICON_DIR / "winsweep-1024.png").convert("RGBA")
    assets = ROOT / "src" / "winui" / "Assets"
    tiles = {
        "Square44x44Logo.scale-200.png": (88, 88),
        "Square44x44Logo.targetsize-24_altform-unplated.png": (24, 24),
        "Square44x44Logo.targetsize-48_altform-lightunplated.png": (48, 48),
        "Square150x150Logo.scale-200.png": (300, 300),
        "Wide310x150Logo.scale-200.png": (620, 300),
        "StoreLogo.png": (50, 50),
        "LockScreenLogo.scale-200.png": (48, 48),
        "SplashScreen.scale-200.png": (1240, 600),
    }
    for filename, (width, height) in tiles.items():
        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        side = min(width, height)
        art = master.resize((side, side), Image.LANCZOS)
        canvas.paste(art, ((width - side) // 2, (height - side) // 2), art)
        canvas.save(assets / filename)
    print(f"tiles: {len(tiles)} written to {assets.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
