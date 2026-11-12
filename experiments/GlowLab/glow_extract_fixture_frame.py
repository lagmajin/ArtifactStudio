"""Extract one reproducible still frame from the documented CC0 fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import cv2


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--frame-index", type=int, default=20)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.frame_index < 0:
        raise ValueError("frame index must be non-negative")
    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise RuntimeError(f"could not open fixture: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    if args.frame_index >= frame_count:
        capture.release()
        raise ValueError(f"frame {args.frame_index} outside frame count {frame_count}")
    capture.set(cv2.CAP_PROP_POS_FRAMES, args.frame_index)
    ok, frame = capture.read()
    capture.release()
    if not ok or frame is None:
        raise RuntimeError(f"could not decode frame {args.frame_index}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(args.output), frame):
        raise RuntimeError(f"could not write frame: {args.output}")
    metadata = {
        "source_fixture": str(args.input.relative_to(ROOT)) if args.input.is_relative_to(ROOT) else str(args.input),
        "source_sha256": digest(args.input),
        "source_license_reference": "tests/ArtifactCore/fixtures/README.md (CC0/Public Domain)",
        "frame_index_zero_based": args.frame_index,
        "frame_time_seconds_within_excerpt": args.frame_index / fps if fps > 0 else None,
        "fps": fps,
        "frame_count": frame_count,
        "dimensions": [int(frame.shape[1]), int(frame.shape[0])],
        "output": args.output.name,
        "output_sha256": digest(args.output),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    metadata_path = args.output.with_suffix(".json")
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {args.output} and {metadata_path}")


if __name__ == "__main__":
    main()
