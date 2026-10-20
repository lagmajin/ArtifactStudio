"""Drive the standalone FxStudio FX animation MCP host end to end.

Renders the Artifact::FxStudio cue animation through the same render path the
contract test uses, but over MCP: inserts presets, samples frames, renders them,
diffs frames, and saves every returned image so the animation can be inspected.

    python tools/test_fx_studio_mcp.py --exe <path to ArtifactFxStudioMcpTest.exe> \
        --output temp/fx-studio-mcp

Exit code is non-zero when any assertion fails.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import sys
import threading


def png_size(payload: bytes):
    """Return (width, height) from a PNG IHDR, or None when not a PNG."""
    if len(payload) < 24 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    if payload[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", payload[16:24])
    return width, height


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=128)
    parser.add_argument("--height", type=int, default=96)
    args = parser.parse_args()

    arts = args.output.resolve()
    arts.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"

    report = {"exe": str(args.exe.resolve()), "checks": [], "images": [], "success": False}
    failures = []

    def check(name, condition, detail=""):
        report["checks"].append({"name": name, "ok": bool(condition), "detail": str(detail)})
        print(f"{'ok  ' if condition else 'FAIL'} {name} {detail}")
        if not condition:
            failures.append(name)

    responses = queue.Queue()
    with (arts / "stderr.log").open("w", encoding="utf-8") as error_log:
        process = subprocess.Popen(
            [str(args.exe.resolve()), "--mcp-stdio",
             f"--width={args.width}", f"--height={args.height}",
             f"--capture-dir={arts / 'frames'}"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=error_log,
            text=True, encoding="utf-8", env=environment)

        def read_responses():
            for line in process.stdout:
                try:
                    responses.put(json.loads(line))
                except json.JSONDecodeError:
                    responses.put({"invalidStdout": line.rstrip()})
            responses.put({"eof": True})

        reader = threading.Thread(target=read_responses, daemon=True)
        reader.start()
        serial = 0

        def request(method, params):
            nonlocal serial
            serial += 1
            process.stdin.write(json.dumps({"jsonrpc": "2.0", "id": serial,
                                            "method": method, "params": params}) + "\n")
            process.stdin.flush()
            response = responses.get(timeout=120)
            if response.get("id") != serial:
                raise RuntimeError(f"unexpected response id: {response}")
            return response

        def call(name, arguments):
            response = request("tools/call", {"name": name, "arguments": arguments})
            if "error" in response:
                raise RuntimeError(f"{name} transport error: {response['error']}")
            return response["result"]

        def structured(name, arguments):
            result = call(name, arguments)
            return result.get("structuredContent", {})

        def save_images(tag, result):
            saved = []
            for index, item in enumerate(result.get("content", [])):
                if item.get("type") != "image":
                    continue
                payload = base64.b64decode(item["data"])
                size = png_size(payload)
                path = arts / f"{tag}-{index}-{item.get('name', 'image')}.png"
                path.write_bytes(payload)
                saved.append({"path": str(path), "bytes": len(payload),
                              "size": size, "name": item.get("name")})
            return saved

        try:
            init = request("initialize", {"protocolVersion": "2025-11-25",
                                          "capabilities": {},
                                          "clientInfo": {"name": "fx-studio-smoke",
                                                         "version": "1"}})
            check("initialize", "result" in init
                  and init["result"].get("serverInfo", {}).get("name")
                  == "artifact-fx-studio-mcp-test", init.get("result", {}).get("serverInfo"))

            tools_response = request("tools/list", {})
            names = sorted(tool["name"]
                           for tool in tools_response.get("result", {}).get("tools", []))
            report["tools"] = names
            expected = {"fx.describe", "fx.presets.list", "fx.session.new",
                        "fx.session.state", "fx.session.load", "fx.session.save",
                        "fx.event.insert", "fx.event.list", "fx.event.update",
                        "fx.event.remove", "fx.event.select", "fx.viewport.set",
                        "fx.cue.bypass", "fx.cue.solo", "fx.frame.cues",
                        "fx.frame.render", "fx.frame.strip", "fx.frame.diff",
                        "fx.capture.write", "fx.capture.directory"}
            missing = sorted(expected.difference(names))
            check("tools/list", not missing and len(names) == len(expected),
                  f"{len(names)} tools, missing={missing}")

            described = structured("fx.describe", {})
            check("fx.describe cueKinds", len(described.get("cueKinds", [])) == 10,
                  described.get("cueKinds"))

            presets = structured("fx.presets.list", {})
            preset_ids = [p["id"] for p in presets.get("presets", [])]
            check("fx.presets.list", len(preset_ids) == 5, preset_ids)
            for preset in presets.get("presets", []):
                if preset["id"] == "fx.glitch.burst":
                    kinds = [cue["kind"] for cue in preset["cues"]]
                    check("glitch burst cue set", "Freeze" in kinds and "SignalDrop" in kinds,
                          kinds)

            structured("fx.session.new", {})
            inserted = structured("fx.event.insert",
                                 {"presetId": "fx.glitch.burst", "startFrame": 0})
            events = inserted.get("events", [])
            check("fx.event.insert", len(events) == 1 and events[0]["cueCount"] == 5, events)

            cues0 = structured("fx.frame.cues", {"frame": 0})
            check("fx.frame.cues at 0", cues0.get("cueCount", 0) >= 1, cues0.get("text"))

            render0 = call("fx.frame.render", {"frame": 0})
            images = save_images("render-f0", render0)
            report["images"].extend(images)
            check("fx.frame.render images", len(images) == 3,
                  [image["name"] for image in images])
            check("fx.frame.render size",
                  all(image["size"] == (args.width, args.height) for image in images),
                  [(image["name"], image["size"]) for image in images])

            # Determinism: the same frame twice must produce identical pixels.
            render0b = call("fx.frame.render", {"frame": 0})
            a = [item["data"] for item in render0["content"] if item["type"] == "image"]
            b = [item["data"] for item in render0b["content"] if item["type"] == "image"]
            check("fx.frame.render deterministic", a == b, "base64 equality")

            strip = call("fx.frame.strip", {"firstFrame": 0, "lastFrame": 12,
                                            "columns": 6, "scale": 2})
            strip_meta = strip.get("structuredContent", {})
            sheet = save_images("strip", strip)
            report["images"].extend(sheet)
            check("fx.frame.strip frames", strip_meta.get("frameCount") == 13,
                  strip_meta.get("frameCount"))
            check("fx.frame.strip held", strip_meta.get("heldFrames", 0) >= 1,
                  strip_meta.get("heldFrames"))
            check("fx.frame.strip distinct", strip_meta.get("distinctFrames", 0) >= 3,
                  strip_meta.get("distinctFrames"))
            check("fx.frame.strip sheet", len(sheet) == 1
                  and sheet[0]["size"] == (args.width // 2 * 6, args.height // 2 * 3),
                  sheet[0]["size"] if sheet else None)
            held = [f for f in strip_meta.get("frames", []) if f.get("held")]
            check("strip marks the freeze frames", len(held) == strip_meta.get("heldFrames"),
                  [f["frame"] for f in held])

            diff = call("fx.frame.diff", {"frameA": 0, "frameB": 6})
            diff_meta = diff.get("structuredContent", {})
            mask = save_images("diff-f0-f6", diff)
            report["images"].extend(mask)
            check("fx.frame.diff changed", diff_meta.get("changedPixels", 0) > 0,
                  diff_meta.get("changedPixels"))
            check("fx.frame.diff mask", len(mask) == 1
                  and mask[0]["size"] == (args.width, args.height),
                  mask[0]["size"] if mask else None)

            # Bypass BlockDisplace (cue index 2) and confirm the sampled cue list
            # drops it. glitch.burst contains Distort-free geometry cues, and
            # BlockDisplace is present from ~frame 1 onward.
            displaced_frames = [f["frame"] for f in strip_meta.get("frames", [])
                                if "BlockDisplace" in f.get("applied", [])]
            if displaced_frames:
                frame_with_cue = displaced_frames[0]
                before = structured("fx.frame.cues", {"frame": frame_with_cue})
                structured("fx.cue.bypass", {"cue": 2, "bypassed": True})
                after = structured("fx.frame.cues", {"frame": frame_with_cue})
                check("fx.cue.bypass removes the cue",
                      "BlockDisplace" in [c["cue"] for c in before.get("cues", [])]
                      and "BlockDisplace" not in [c["cue"] for c in after.get("cues", [])],
                      f"frame {frame_with_cue}")
                structured("fx.cue.bypass", {"cue": 2, "bypassed": False})
                restored = structured("fx.frame.cues", {"frame": frame_with_cue})
                check("fx.cue.bypass restores the cue",
                      "BlockDisplace" in [c["cue"] for c in restored.get("cues", [])])
            else:
                check("fx.cue.bypass removes the cue", False,
                      "no frame applied BlockDisplace")

            before_solo = structured("fx.frame.cues", {"frame": 2})
            solo = structured("fx.cue.solo", {"cue": 7})
            solo_viewport = solo.get("viewport", {})
            check("fx.cue.solo visibleSamples", solo_viewport.get("soloCue") == "MaskProgress",
                  solo_viewport.get("soloCue"))
            soloed = structured("fx.frame.cues", {"frame": 2})
            # glitch.burst has no MaskProgress cue, so the solo filter must empty
            # the sampled list even though the frame had cues before.
            check("fx.cue.solo filters cues",
                  before_solo.get("cueCount", 0) > 0 and soloed.get("cueCount", 0) == 0,
                  f"{before_solo.get('cueCount')} -> {soloed.get('cueCount')}")
            structured("fx.cue.solo", {"cue": -1})

            structured("fx.viewport.set", {"comparison": "Split", "splitPosition": 0.5,
                                           "background": "Checkerboard"})
            split_render = call("fx.frame.render", {"frame": 6})
            split_meta = split_render.get("structuredContent", {})
            split_images = save_images("split-f6", split_render)
            report["images"].extend(split_images)
            check("fx.viewport.set Split",
                  split_meta.get("viewport", {}).get("comparison") == "Split",
                  split_meta.get("viewport", {}).get("comparison"))
            structured("fx.viewport.set", {"comparison": "Processed",
                                           "background": "Dark"})

            written = structured("fx.capture.write",
                                 {"directory": str(arts / "written"),
                                  "firstFrame": 0, "lastFrame": 5})
            files = [Path(p) for p in written.get("written", [])]
            check("fx.capture.write files",
                  written.get("failures", 1) == 0 and len(files) == 6
                  and all(p.is_file() and p.stat().st_size > 100 for p in files),
                  f"{len(files)} files, failures={written.get('failures')}")
            sheet_path = Path(written.get("sheet", ""))
            check("fx.capture.write sheet",
                  sheet_path.is_file() and sheet_path.stat().st_size > 100, str(sheet_path))
            report["captureDirectory"] = written.get("directory")

            saved = structured("fx.session.save", {})
            state_before = structured("fx.session.state", {})
            reloaded = structured("fx.session.load", {"json": saved.get("json", "")})
            check("fx.session round trip",
                  reloaded.get("loaded") is True
                  and reloaded.get("eventCount") == state_before.get("eventCount"),
                  f"{state_before.get('eventCount')} -> {reloaded.get('eventCount')}")

            updated = structured("fx.event.update", {"id": 1, "durationFrames": 7})
            check("fx.event.update", updated.get("events", [{}])[0].get("durationFrames") == 7,
                  updated.get("events"))
            removed = structured("fx.event.remove", {"id": 1})
            check("fx.event.remove", removed.get("eventCount") == 0,
                  removed.get("eventCount"))

            bad_preset = call("fx.event.insert", {"presetId": "fx.nope"})
            check("unknown preset is an error",
                  bad_preset.get("structuredContent", {}).get("error") is not None
                  or bad_preset.get("isError") is True,
                  bad_preset.get("structuredContent"))
            bad_range = call("fx.frame.strip", {"firstFrame": 5, "lastFrame": 1})
            check("inverted range is an error", bad_range.get("isError") is True,
                  bad_range.get("structuredContent"))
            bad_tool = request("tools/call", {"name": "fx.nope", "arguments": {}})
            check("unknown tool is a JSON-RPC error", "error" in bad_tool,
                  bad_tool.get("error", {}).get("message"))

            report["success"] = not failures
        except Exception as exception:  # noqa: BLE001 - driver reports everything
            failures.append(f"exception: {exception}")
            print(f"FAIL exception: {exception}")
        finally:
            try:
                process.stdin.close()
            except Exception:  # noqa: BLE001
                pass
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
            print(f"server exit code: {process.returncode}")

    report["failures"] = failures
    (arts / "fx-studio-mcp-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    print(f"images saved: {len(report['images'])}, report: {arts / 'fx-studio-mcp-report.json'}")
    if failures:
        print(f"FAILURES ({len(failures)}): {failures}")
        return 1
    print("FxStudio MCP smoke test passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
