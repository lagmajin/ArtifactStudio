"""Measure per-channel and combined RGB L1 against a supplied PFM reference."""

import argparse
import math
import struct
from pathlib import Path


def read_pfm(path: Path):
    with path.open("rb") as image:
        if image.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, image.readline().split())
        scale = float(image.readline())
        if not math.isfinite(scale) or scale == 0.0:
            raise ValueError(f"invalid PFM scale: {path}")
        data = image.read()
    count = width * height * 3
    if len(data) != count * 4:
        raise ValueError(f"unexpected PFM payload size: {path}")
    endian = "<" if scale < 0 else ">"
    values = struct.unpack(f"{endian}{count}f", data)
    multiplier = abs(scale)
    return width, height, tuple(value * multiplier for value in values)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="unprocessed source PFM")
    parser.add_argument("reference", type=Path, help="dense reference render PFM")
    parser.add_argument("--case", action="append", nargs=2, required=True,
                        metavar=("LABEL", "PFM"))
    args = parser.parse_args()
    source = read_pfm(args.input)
    reference = read_pfm(args.reference)
    if source[:2] != reference[:2]:
        parser.error("input and reference dimensions differ")

    for label, path in args.case:
        candidate = read_pfm(Path(path))
        if candidate[:2] != reference[:2] or candidate[:2] != source[:2]:
            parser.error(f"dimensions differ for {path}")
        abs_error = [0.0, 0.0, 0.0]
        ref_energy = [0.0, 0.0, 0.0]
        for index in range(0, len(reference[2]), 3):
            for channel in range(3):
                original = source[2][index + channel]
                expected = max(0.0, reference[2][index + channel] - original)
                measured = max(0.0, candidate[2][index + channel] - original)
                abs_error[channel] += abs(measured - expected)
                ref_energy[channel] += abs(expected)
        ratios = [error / max(energy, 1e-30)
                  for error, energy in zip(abs_error, ref_energy)]
        total_ratio = sum(abs_error) / max(sum(ref_energy), 1e-30)
        print(f"{label}: RGB-L1={total_ratio:.6%} "
              f"R={ratios[0]:.6%} G={ratios[1]:.6%} B={ratios[2]:.6%}")


if __name__ == "__main__":
    main()
