#!/usr/bin/env python3
"""NUFLI delta encoder — extract shared base + per-screen deltas.

Given multiple .nuf files, finds the consensus base (most common byte
at each position) and generates per-screen delta files.

Output format:
  base.bin    - consensus data (23040 bytes)
  *.delta     - per-screen patches using bitmask:
                  2880 bytes bitmask (1 bit per byte position)
                  N bytes of differing values (in order)

Usage:
    python3 nufli_delta.py output_base screen1.nuf screen2.nuf ...
"""

import argparse
import sys
from pathlib import Path
from collections import Counter


NUFLI_SIZE = 23040
BITMASK_SIZE = (NUFLI_SIZE + 7) // 8  # 2880 bytes


def load_nuf(path):
    """Load .nuf file, skip 2-byte load address."""
    data = Path(path).read_bytes()
    if len(data) == NUFLI_SIZE + 2:
        return data[2:]
    elif len(data) == NUFLI_SIZE:
        return data
    else:
        print(f"Warning: {path} is {len(data)} bytes, expected {NUFLI_SIZE} or {NUFLI_SIZE + 2}")
        return data[:NUFLI_SIZE]


def find_base(screens):
    """Find consensus base: most common byte at each position."""
    base = bytearray(NUFLI_SIZE)
    for i in range(NUFLI_SIZE):
        counts = Counter()
        for s in screens:
            counts[s[i]] += 1
        base[i] = counts.most_common(1)[0][0]
    return bytes(base)


def compute_delta_bitmask(base, screen):
    """Compute delta between base and screen using bitmask format.
    Returns (bitmask, values) where bitmask indicates which bytes differ."""
    bitmask = bytearray(BITMASK_SIZE)
    values = bytearray()
    for i in range(NUFLI_SIZE):
        if base[i] != screen[i]:
            byte_idx = i // 8
            bit_idx = i % 8
            bitmask[byte_idx] |= (1 << bit_idx)
            values.append(screen[i])
    return bytes(bitmask), bytes(values)


def main():
    ap = argparse.ArgumentParser(description="NUFLI delta encoder")
    ap.add_argument("output_base", help="Output base path (no extension)")
    ap.add_argument("inputs", nargs="+", help="Input .nuf files")
    args = ap.parse_args()

    if len(args.inputs) < 2:
        print("Error: need at least 2 input files for delta encoding")
        sys.exit(1)

    # Load all screens
    screens = []
    names = []
    for path in args.inputs:
        screens.append(load_nuf(path))
        names.append(Path(path).stem)

    # Find consensus base
    base = find_base(screens)
    out_path = Path(args.output_base)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Write base
    base_file = out_path.with_suffix(".base")
    base_file.write_bytes(base)
    print(f"Base: {base_file} ({len(base)} bytes)")

    # Compute and write deltas
    total_delta_bytes = 0
    for name, screen in zip(names, screens):
        bitmask, values = compute_delta_bitmask(base, screen)
        delta_data = bitmask + values
        delta_file = out_path.parent / f"{name}.delta"
        delta_file.write_bytes(delta_data)
        total_delta_bytes += len(delta_data)
        print(f"  {name}.delta: {len(values)} patches, {len(delta_data)} bytes "
              f"(bitmask={len(bitmask)} + values={len(values)})")

    # Summary
    total_raw = len(screens) * NUFLI_SIZE
    total_encoded = len(base) + total_delta_bytes
    print(f"\nTotal raw: {total_raw} bytes")
    print(f"Total encoded: {total_encoded} bytes (base={len(base)} + deltas={total_delta_bytes})")
    print(f"Savings: {total_raw - total_encoded} bytes ({100 * (1 - total_encoded / total_raw):.1f}%)")


if __name__ == "__main__":
    main()
