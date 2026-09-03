#!/usr/bin/env python3
"""Run a protocol-level smoke test against ArtifactProxyWorker."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("worker", type=Path, help="Path to ArtifactProxyWorker executable")
    parser.add_argument("source", type=Path, help="Video fixture")
    parser.add_argument("--backend", default="ffmpeg", choices=("ffmpeg", "native", "mediaFoundation", "auto"))
    parser.add_argument("--expect-backend", choices=("ffmpeg", "native", "mediaFoundation"))
    parser.add_argument("--scale", type=float, default=0.125)
    parser.add_argument("--audio-reencode", action="store_true")
    parser.add_argument("--hardware-accel", action="store_true")
    parser.add_argument("--expect-failure", action="store_true",
                        help="Expect a non-zero exit with no completed message or output")
    parser.add_argument("--require-audio", action="store_true")
    parser.add_argument("--cancel-after-ms", type=int,
                        help="Create the cancel token after this many milliseconds")
    parser.add_argument("--timeout-seconds", type=float, default=120.0,
                        help="Fail if the worker does not exit within this many seconds")
    parser.add_argument("--ffprobe", type=Path, help="Optional ffprobe executable")
    args = parser.parse_args()

    if args.require_audio and not args.ffprobe:
        parser.error("--require-audio requires --ffprobe")
    if args.cancel_after_ms is not None and args.cancel_after_ms < 1:
        parser.error("--cancel-after-ms must be positive")
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")

    worker = args.worker.resolve()
    source = args.source.resolve()
    if not worker.is_file():
        parser.error(f"worker not found: {worker}")
    if not source.is_file():
        parser.error(f"source not found: {source}")
    if not 0.01 <= args.scale <= 1.0:
        parser.error("scale must be between 0.01 and 1.0")
    expected_quality = ("full" if args.scale >= 0.9 else
                        "eighth" if args.scale <= 0.125 else
                        "quarter" if args.scale <= 0.25 else "half")

    with tempfile.TemporaryDirectory(prefix="artifact-proxy-smoke-") as temp:
        root = Path(temp)
        output = root / "proxy.mp4"
        partial_output = root / "proxy.partial.mp4"
        request = root / "request.json"
        cancel = root / "cancel.token"
        request.write_text(json.dumps({
            "protocolVersion": 1,
            "jobId": "smoke-test",
            "sourcePath": str(source),
            "outputPath": str(output),
            "temporaryOutputPath": str(partial_output),
            "scale": args.scale,
            "qualityPreset": expected_quality,
            "backend": args.backend,
            "hardwareAccel": args.hardware_accel,
            "audioReencode": args.audio_reencode,
            "cancelPath": str(cancel),
        }), encoding="utf-8")

        process = subprocess.Popen(
            [str(worker), "--request", str(request)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if args.cancel_after_ms is not None:
            deadline = time.monotonic() + args.cancel_after_ms / 1000.0
            while process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.01)
            if process.poll() is None:
                cancel.write_text("cancelled\n", encoding="utf-8")
        timed_out = False
        try:
            stdout, stderr = process.communicate(timeout=args.timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.terminate()
            try:
                stdout, stderr = process.communicate(timeout=2.0)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()
            stderr = (stderr or "") + "\nworker timed out"
        result = subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)
        messages = []
        for line in result.stdout.splitlines():
            try:
                messages.append(json.loads(line))
            except json.JSONDecodeError:
                print(f"invalid JSONL: {line}", file=sys.stderr)

        completed = next((m for m in messages if m.get("type") == "completed"), None)
        failed = next((m for m in messages if m.get("type") == "failed"), None)
        cancelled = args.cancel_after_ms is not None
        expect_failure = args.expect_failure or cancelled
        valid = ((
            result.returncode != 0
            and completed is None
            and failed is not None
            and failed.get("jobId") == "smoke-test"
            and not output.exists()
            and not partial_output.exists()
        ) if expect_failure else (
            not timed_out
            and result.returncode == 0
            and completed is not None
            and completed.get("jobId") == "smoke-test"
            and Path(completed.get("outputPath", "")).resolve() == output.resolve()
            and output.is_file()
            and output.stat().st_size > 0
            and not partial_output.exists()
            and completed.get("outputBytes") == output.stat().st_size
            and completed.get("qualityPreset") == expected_quality
            and completed.get("audioReencodeRequested") is args.audio_reencode
            and completed.get("hardwareAccelRequested") is args.hardware_accel
            and (args.expect_backend is None or
                 str(completed.get("backend", "")).lower() == args.expect_backend.lower())
        ))
        if not valid and result.stderr.strip():
            print(result.stderr.strip(), file=sys.stderr)
        if not valid:
            print(json.dumps({
                "backend": args.backend,
                "audioReencode": args.audio_reencode,
                "returncode": result.returncode,
                "timedOut": timed_out,
                "completed": completed,
                "failure": failed,
                "outputBytes": output.stat().st_size if output.exists() else 0,
                "cancelled": cancelled,
                "passed": False,
            }, ensure_ascii=False))
            return 1

        probe_output = None
        if args.ffprobe:
            probe = subprocess.run(
                [str(args.ffprobe.resolve()), "-v", "error", "-show_entries",
                 "stream=codec_type,codec_name,width,height", "-of", "json", str(output)],
                capture_output=True,
                text=True,
                check=False,
            )
            if probe.returncode != 0:
                print(probe.stderr.strip(), file=sys.stderr)
                return 1
            if args.require_audio:
                try:
                    streams = json.loads(probe.stdout).get("streams", [])
                except json.JSONDecodeError:
                    print("ffprobe returned invalid JSON", file=sys.stderr)
                    return 1
                if not any(stream.get("codec_type") == "audio" for stream in streams):
                    print("expected an audio stream, but none was found", file=sys.stderr)
                    return 1
            probe_output = probe.stdout.strip()
        print(json.dumps({
            "backend": args.backend,
            "audioReencode": args.audio_reencode,
            "returncode": result.returncode,
            "timedOut": timed_out,
            "completed": completed,
            "failure": failed,
            "outputBytes": output.stat().st_size if output.exists() else 0,
            "cancelled": cancelled,
            "passed": True,
        }, ensure_ascii=False))
        if probe_output:
            print(probe_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
