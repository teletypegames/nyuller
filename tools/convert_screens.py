#!/usr/bin/env python3
"""Convert a C64 game source PNG into multicolor-bitmap .bin + .attr files.

Phase 1 of the "Nyuller" C64 project. See tasks.md for the full spec.

The C64 multicolor bitmap mode (VIC-II $D011 BMM=1, $D016 MCM=1) is a
160x200-pixel, 2-bits-per-pixel, 4-colors-per-4x8-cell format. The
bitmap is 8000 bytes and the "screen memory" (color attribute table) is
1000 bytes (one byte per 4x8 cell).

Per c64-wiki.com/wiki/Multicolor_Bitmap_Mode, the 2-bit pixel value
selects:
    00 = $D021 (background color 0, the global "00" color)
    01 = upper nibble of the cell's screen-memory byte
    10 = lower nibble of the cell's screen-memory byte
    11 = lower nibble of the cell's color RAM ($D800+cell)

So our .attr byte layout per cell is:
    bits 0-3 = "10" pixel color (C64 palette index 0..15)
    bits 4-7 = "01" pixel color (C64 palette index 0..15)

Each cell's "11" color would be stored in color RAM at runtime (4 bits
per cell = 1000 nibbles at $D800-$DBE7). For Phase 1 we do not store
the "11" color in the .attr file; the Phase 2 C code is expected to
program color RAM as needed. By default we set the global "00" color
$D021 to the most common color across the whole screen and leave color
RAM zeroed (which gives a black "11" color) - this loses one color per
cell but keeps the format to two files (8000 + 1000 = 9000 bytes).

The C64 multicolor bitmap is laid out in memory as 320 bytes per
"character row" (8 pixel rows), 40 cells per row, 25 character rows
total. Within a cell, the 8 bytes correspond to the 8 pixel rows, and
each byte holds 4 pixels (2 bits per pixel) with the leftmost pixel
in the high bits (MSB-first).

CROP / DOWNSCALE STRATEGY CHOICE
--------------------------------
Source PNGs are ~1400x1100; the target is 160x200 (a ~9x downscale).
We tried two strategies on all 5 source images:

  A. Direct resize to 160x200 with LANCZOS (1 pass).
  B. Resize to 320x200 with LANCZOS, then 2x downscale to 160x200
     with BOX (the integer-downscale "area averaging" filter).

Both produce visually equivalent results at this scale factor - the
2x BOX step in B is the "textbook" approach for large downscaling
but at ~9x the difference vs A is negligible. We pick A as default
because it's simpler (one LANCZOS pass) and a touch faster. Both
remain available via --strategy; the C side doesn't care which was
used.

CROP REGION
-----------
The full source image is used (no manual cropping). The source aspect
ratio (~1.23:1) doesn't match the 160x200 target (0.8:1), so the
image is stretched vertically by ~1.5x. We accept this for Phase 1;
a follow-up could either letterbox (fit by height, add black bars
top/bottom) or crop (fit by width, drop sides). The current result
is recognisable for all 5 source images.

$D021 SELECTION
---------------
We pick the most common C64 palette color in the (downscaled) image
as the global $D021 value. For our 5 sunset-themed screens this
turns out to be black (0), which is also the C64 community's default.
Black is a good d021 because (a) every cell is likely to contain it
(outlines, shadows), (b) it makes the C64's border $D020 look
natural, and (c) the "11" color in color RAM defaults to black at
boot, so unused "11" pixels are also black. Override with --d021
if a different global background is desired.

Usage:
    python3 convert_screens.py <input.png> <output_base> [--strategy A|B]
    python3 convert_screens.py source_images/screen_title.png src/data/processed/title
"""

import argparse
import sys
from collections import Counter
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("ERROR: Pillow not installed. Try: pip install pillow\n")
    sys.exit(1)


# C64 "Pepto PAL" palette (RGB).  Indices match the VIC-II color number
# ($D021..$D024 background regs and $D025-$D026 sprite MC regs use the
# same 16 colors).  The order below is index 0=black, 1=white, etc.
PALETTE_RGB = (
    (0x00, 0x00, 0x00),  # 0  black
    (0xFF, 0xFF, 0xFF),  # 1  white
    (0x88, 0x00, 0x00),  # 2  red
    (0xAA, 0xFF, 0xEE),  # 3  cyan
    (0xCC, 0x44, 0xCC),  # 4  purple
    (0x00, 0xCC, 0x55),  # 5  green
    (0x00, 0x00, 0xAA),  # 6  blue
    (0xEE, 0xEE, 0x77),  # 7  yellow
    (0xDD, 0x88, 0x55),  # 8  orange
    (0x66, 0x44, 0x00),  # 9  brown
    (0xFF, 0x77, 0x77),  # 10 light red
    (0x33, 0x33, 0x33),  # 11 dark grey
    (0x77, 0x77, 0x77),  # 12 medium grey
    (0xAA, 0xFF, 0x66),  # 13 light green
    (0x00, 0x88, 0xFF),  # 14 light blue
    (0xCC, 0xCC, 0xCC),  # 15 light grey
)


def _dist2(a, b):
    """Squared RGB distance between two 3-tuples. Faster than sqrt."""
    dr = a[0] - b[0]
    dg = a[1] - b[1]
    db = a[2] - b[2]
    return dr * dr + dg * dg + db * db


def nearest_palette_index(rgb):
    """Return the index (0..15) of the nearest C64 palette color to rgb."""
    best_i = 0
    best_d = _dist2(rgb, PALETTE_RGB[0])
    for i in range(1, 16):
        d = _dist2(rgb, PALETTE_RGB[i])
        if d < best_d:
            best_d = d
            best_i = i
    return best_i


def palette_snap_image(img):
    """Snap every pixel of a PIL image to the nearest C64 palette color.

    Returns a new image where each pixel is a tuple from PALETTE_RGB.
    Done by quantizing to the 16-color C64 palette globally - this is
    a useful intermediate step before the per-cell quantization, but
    the per-cell 4-color selection is the real work.
    """
    out = Image.new("RGB", img.size)
    src = img.load()
    dst = out.load()
    for y in range(img.height):
        for x in range(img.width):
            dst[x, y] = PALETTE_RGB[nearest_palette_index(src[x, y])]
    return out


def _cell_palette(pixels_rgb, d021_index, k=3):
    """Pick k extra palette colors for a 4x8 cell, excluding d021_index.

    Returns a list of exactly k C64 palette indices (0..15) sorted by
    frequency in pixels_rgb (most common first).  If the cell has
    fewer than k distinct non-d021 colors, pad by repeating the last
    one.  These k colors plus d021_index form the cell's 4-color set.
    """
    counts = Counter()
    for p in pixels_rgb:
        idx = nearest_palette_index(p)
        if idx != d021_index:
            counts[idx] += 1
    most = [idx for idx, _ in counts.most_common(k)]
    while len(most) < k:
        most.append(most[-1] if most else d021_index)
    return most


def _snap_pixels_to_indices(pixels_rgb, palette_indices):
    """Snap each pixel to the nearest of the given palette indices.

    Returns a list of 0..3 (the index into palette_indices, not the
    palette index itself).  palette_indices[0] is treated as the "00"
    color and is what the C64 will display for 2-bit pixel value 00.
    """
    pal_rgb = [PALETTE_RGB[i] for i in palette_indices]
    snapped = []
    for p in pixels_rgb:
        best_local = 0
        best_d = _dist2(p, pal_rgb[0])
        for j in range(1, len(pal_rgb)):
            d = _dist2(p, pal_rgb[j])
            if d < best_d:
                best_d = d
                best_local = j
        snapped.append(best_local)
    return snapped


def quantize_cell(pixels_rgb, d021_index):
    """Pick 4 colors for a 4x8 cell and snap each pixel to one of them.

    The C64 multicolor bitmap's "00" color is the global $D021 register,
    so d021_index must be one of the cell's 4 colors (at position 0).
    The other 3 colors are the 3 most common non-d021 colors in the
    cell, sorted by frequency.

    pixels_rgb is a list of 32 (R,G,B) tuples (4 wide x 8 tall).
    Returns (palette_indices, snapped_local):
      palette_indices - list of exactly 4 C64 palette indices:
                          [0] = d021_index (the "00" color)
                          [1] = cell's most common (excluding d021)
                          [2] = cell's 2nd most common
                          [3] = cell's 3rd most common
      snapped_local   - list of 32 ints in 0..3, the local index into
                        palette_indices for each pixel in raster order
                        (left-to-right, top-to-bottom).  Value 0 means
                        "this pixel snaps to d021_index".
    """
    extra = _cell_palette(pixels_rgb, d021_index, k=3)
    palette_indices = [d021_index] + extra
    snapped_local = _snap_pixels_to_indices(pixels_rgb, palette_indices)
    return palette_indices, snapped_local


def encode_cell_pixels(snapped_local):
    """Pack 32 snapped-local pixels (0..3 each) into 8 bytes.

    The C64 multicolor bitmap packs 4 pixels per byte, MSB-first: bits
    7-6 hold pixel 0 (leftmost), bits 5-4 hold pixel 1, bits 3-2 hold
    pixel 2, bits 1-0 hold pixel 3. The 2-bit value selects which of
    the 4 cell colors to use.

    pixels are in raster order (left-to-right, top-to-bottom) for a
    4x8 cell: rows 0..7, each with 4 pixels.
    """
    assert len(snapped_local) == 32
    out = bytearray(8)
    for row in range(8):
        b = 0
        for col in range(4):
            pix = snapped_local[row * 4 + col] & 0x03
            b = (b << 2) | pix
        out[row] = b
    return bytes(out)


def make_attr_byte(palette_indices):
    """Pack a cell's 4-color palette into a 1-byte screen-memory entry.

    bits 0-3 = "10" pixel color (C64 palette index 0..15)
    bits 4-7 = "01" pixel color (C64 palette index 0..15)

    Convention used here (see module docstring):
      palette_indices[0] = d021_index (the "00" color, global)
      palette_indices[1] = "01" color (high nibble of attr)
      palette_indices[2] = "10" color (low nibble of attr)
      palette_indices[3] = "11" color (NOT stored in .attr - it lives in
                            color RAM at $D800+cell_index, set by the
                            C code at runtime)

    This is a deliberate simplification: the "11" color is the 4th color
    in the cell, but it costs 4 bits per cell to store in color RAM and
    we only have 1000 bytes for the .attr file.  The C-side code can
    either accept the "11" = black default (color RAM is initialized to
    0 at boot) or set color RAM to a per-cell value from a separate
    table.  See tasks.md Phase 1 notes and the follow-up work for
    Phase 2.
    """
    fg10 = palette_indices[2] & 0x0F
    bg01 = palette_indices[1] & 0x0F
    return fg10 | (bg01 << 4)


def pick_global_d021(img_palette_quantized, source_rgb=None):
    """Pick the single $D021 value to use for the whole screen.

    Counts the C64 palette indices used in the already-quantized image
    and picks the most common.  This is the "00" color shared by all
    cells.  If source_rgb is given, uses it instead of the quantized
    image to do the count (a tiny bit faster than re-quantizing).

    Returns an int 0..15.

    The default uses the most common color of the whole image, which
    for our 5 sunset-themed screens ends up being black (the C64's
    natural background).  This is a deliberate choice: black is the
    safest d021 because it's a common "outline" / "shadow" color in
    the art, every cell is likely to contain it, and the C64 community
    uses it by default.  We can revisit this (e.g. pick the most common
    non-black color) if the output looks too dark in later phases.
    """
    counts = Counter()
    src = img_palette_quantized.load()
    for y in range(img_palette_quantized.height):
        for x in range(img_palette_quantized.width):
            counts[nearest_palette_index(src[x, y])] += 1
    return counts.most_common(1)[0][0]


def convert_image(img, strategy="A", verbose=False, d021_override=None):
    """Convert a PIL Image to (bitmap_bytes, attr_bytes, d021_index).

    img           - PIL Image (any mode; will be converted to RGB)
    strategy      - "A" (resize directly to 160x200) or
                    "B" (resize to 320x200 then 2x downscale to 160x200)
    d021_override - if not None, force this as the $D021 value
                    (skip the auto-pick).  int 0..15.
    Returns (bitmap, attr, d021_index) where:
      bitmap     - bytes of length 8000
      attr       - bytes of length 1000
      d021_index - int 0..15, the $D021 value to program on the C64
    """
    if img.mode != "RGB":
        img = img.convert("RGB")

    if strategy == "A":
        # Direct 160x200 downscale with LANCZOS.  One pass, simple.
        # For these ~1400x1100 source images, this is an 8.7x
        # horizontal / 5.6x vertical downscale.  LANCZOS handles
        # this OK but is at the edge of its comfort zone; a 2x
        # downscale is more in LANCZOS's sweet spot.
        small = img.resize((160, 200), Image.LANCZOS)
    elif strategy == "B":
        # 320x200 with LANCZOS, then 2x downscale with BOX.
        # The first pass is 4.3x (LANCZOS does this well), the
        # second is a clean 2x box filter (Pillow's BOX is the
        # integer-downscale equivalent of "area averaging" - each
        # output pixel is the mean of a 2x2 block of input pixels,
        # which is what you want for downscaling).  This is the
        # textbook high-quality downscale recipe.
        mid = img.resize((320, 200), Image.LANCZOS)
        small = mid.resize((160, 200), Image.BOX)
    else:
        raise ValueError("strategy must be 'A' or 'B'")

    # First, snap every pixel to the nearest C64 palette color. This
    # gives a stable RGB for downstream analysis (no antialiasing
    # artifacts in the count).
    quantized = palette_snap_image(small)
    if d021_override is not None:
        d021_index = d021_override & 0x0F
    else:
        d021_index = pick_global_d021(quantized)
    if verbose:
        print(f"  d021 (background) = {d021_index} "
              f"({PALETTE_RGB[d021_index]})")

    bitmap = bytearray(8000)
    attr = bytearray(1000)
    src = quantized.load()

    # 1000 cells: 40 across (160/4), 25 down (200/8).
    for cy in range(25):
        for cx in range(40):
            # Gather the 32 pixel colors in this 4x8 cell.
            pixels = []
            for row in range(8):
                py = cy * 8 + row
                for col in range(4):
                    px = cx * 4 + col
                    pixels.append(src[px, py])

            palette_indices, snapped_local = quantize_cell(pixels,
                                                           d021_index)
            attr_byte = make_attr_byte(palette_indices)
            cell_bytes = encode_cell_pixels(snapped_local)

            # Place the 8 bytes for this cell in the bitmap.
            # C64 multicolor bitmap memory layout:
            #   offset = (cy * 40 + cx) * 8
            # because 40 cells per character row, 8 bytes per cell.
            cell_offset = (cy * 40 + cx) * 8
            bitmap[cell_offset:cell_offset + 8] = cell_bytes
            attr[cy * 40 + cx] = attr_byte

    return bytes(bitmap), bytes(attr), d021_index


def preview_image(bitmap, attr, d021_index):
    """Render the .bin/.attr back to a PIL Image for visual inspection.

    Useful for debugging the quantizer. Not used by the C side.
    Shows what the C64 will actually display: 2-bit value 00 -> d021,
    01 -> attr high nibble, 10 -> attr low nibble, 11 -> color RAM.
    Since we don't store the "11" color in .attr, we use black (0)
    here, which is what the C64's color RAM defaults to at boot.
    """
    out = Image.new("RGB", (160, 200), PALETTE_RGB[d021_index])
    px = out.load()
    for cy in range(25):
        for cx in range(40):
            cell_offset = (cy * 40 + cx) * 8
            attr_byte = attr[cy * 40 + cx]
            fg10 = PALETTE_RGB[attr_byte & 0x0F]            # "10" color
            bg01 = PALETTE_RGB[(attr_byte >> 4) & 0x0F]     # "01" color
            for row in range(8):
                b = bitmap[cell_offset + row]
                for col in range(4):
                    v = (b >> (6 - col * 2)) & 0x03
                    if v == 0:
                        c = PALETTE_RGB[d021_index]
                    elif v == 1:
                        c = bg01
                    elif v == 2:
                        c = fg10
                    else:
                        # "11" - color RAM.  Not stored in .attr.
                        c = PALETTE_RGB[0]
                    px[cx * 4 + col, cy * 8 + row] = c
    return out


def convert_file(input_path, output_base, strategy="A", verbose=False,
                 write_preview=False, d021_override=None):
    """Convert input_path to (output_base + ".bin", output_base + ".attr").

    output_base is a path WITHOUT the .bin / .attr extension.
    """
    in_path = Path(input_path)
    if not in_path.exists():
        raise FileNotFoundError(in_path)
    if verbose:
        print(f"Converting {in_path} -> {output_base}.{{bin,attr}} "
              f"(strategy {strategy})")
    img = Image.open(in_path)
    bitmap, attr, d021_index = convert_image(img, strategy=strategy,
                                             verbose=verbose,
                                             d021_override=d021_override)

    out_path = Path(output_base)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path.with_suffix(".bin"), "wb") as f:
        f.write(bitmap)
    with open(out_path.with_suffix(".attr"), "wb") as f:
        f.write(attr)
    # Also write a tiny sidecar with the $D021 value, so the C code
    # doesn't have to guess.  This is checked into the repo too.
    with open(out_path.with_suffix(".d021"), "w") as f:
        f.write(f"{d021_index}\n")

    if verbose:
        print(f"  wrote {out_path.with_suffix('.bin')} "
              f"({len(bitmap)} bytes)")
        print(f"  wrote {out_path.with_suffix('.attr')} "
              f"({len(attr)} bytes)")
        print(f"  wrote {out_path.with_suffix('.d021')} ({d021_index})")

    if write_preview:
        prev = preview_image(bitmap, attr, d021_index)
        prev_path = out_path.with_suffix(".preview.png")
        prev.save(prev_path)
        if verbose:
            print(f"  wrote {prev_path}")

    return bitmap, attr, d021_index


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("input", help="Source PNG path")
    ap.add_argument("output_base",
                    help="Output base path (no .bin/.attr extension)")
    ap.add_argument("--strategy", choices=["A", "B"], default="A",
                    help="A: 160x200 direct LANCZOS downscale (default). "
                         "B: 320x200 LANCZOS then 2x BOX downscale. "
                         "Both produce visually equivalent output on the "
                         "5 source images; A is simpler and faster.")
    ap.add_argument("--preview", action="store_true",
                    help="Also write a <base>.preview.png for inspection")
    ap.add_argument("--d021", type=int, default=None,
                    help="Force the $D021 (background) color, 0..15. "
                         "Default: auto-pick the most common color in "
                         "the image.")
    ap.add_argument("--quiet", action="store_true",
                    help="Suppress per-file progress output")
    args = ap.parse_args(argv)

    if args.d021 is not None and not (0 <= args.d021 <= 15):
        ap.error("--d021 must be in 0..15")

    convert_file(args.input, args.output_base,
                 strategy=args.strategy,
                 verbose=not args.quiet,
                 write_preview=args.preview,
                 d021_override=args.d021)


if __name__ == "__main__":
    main()
