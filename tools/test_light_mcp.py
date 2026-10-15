"""Exercise the standalone light MCP host; save GPU images and crash diagnostics."""
import argparse
import base64
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--backend", choices=("vulkan", "d3d12"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--window-proof", action="store_true",
                        help="Also capture the HWND before GPU readback; this changes timing")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["ARTIFACT_RENDER_BACKEND"] = args.backend
    environment["QT_QPA_PLATFORM"] = "windows"
    environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(args.exe.resolve().parent / "platforms")
    responses = queue.Queue()
    report = {"backendRequested": args.backend, "windowProofEnabled": args.window_proof,
              "captures": [], "success": False}
    dump_path = args.output.resolve() / f"native-crash-{uuid.uuid4().hex}.dmp"
    with (args.output / "stderr.log").open("w", encoding="utf-8") as error_log:
        process = subprocess.Popen([str(args.exe.resolve()), "--mcp-stdio",
                                    f"--crash-dump={dump_path}"],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=error_log, text=True, encoding="utf-8",
                                   env=environment)

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
            response = responses.get(timeout=45)
            if response.get("id") != serial or "error" in response:
                raise RuntimeError(f"Invalid MCP response: {response}")
            return response["result"]

        def call(name, arguments):
            return request("tools/call", {"name": name, "arguments": arguments})

        try:
            request("initialize", {"protocolVersion": "2025-11-25", "capabilities": {},
                                   "clientInfo": {"name": "light-smoke", "version": "1"}})
            report["tools"] = request("tools/list", {})
            last_frame = -1
            cases = [(kind, name, rgb, dominant)
                     for kind in ("point", "spot", "parallel", "area", "ambient")
                     for name, rgb, dominant in (("red", [1, 0.02, 0.02], 0),
                                                 ("green", [0.02, 1, 0.02], 1))]
            red_reference = None
            for kind, name, rgb, dominant in cases:
                changed = call("light.set", {"type": kind, "rgb": rgb,
                                             "intensityPercent": 100 if kind == "ambient" else 1000})
                if changed.get("isError"):
                    raise RuntimeError(f"Light mutation failed: {changed}")
                captured = call("light.capture", {})
                state = captured.get("structuredContent", {})
                if captured.get("isError") or state.get("error"):
                    diagnostic_state = call("light.state", {}).get("structuredContent", {})
                    report["failedCapture"] = state
                    report["stateAtFailure"] = diagnostic_state
                    raise RuntimeError(
                        f"Capture failed: {state}; current state: {diagnostic_state}")
                for content in captured.get("content", []):
                    if content.get("type") == "image":
                        (args.output / f"{kind}_{name}.png").write_bytes(
                            base64.b64decode(content["data"], validate=True))
                        break
                else:
                    raise RuntimeError("MCP returned no GPU image")
                report["captures"].append(state)
                color = state["averageRGB"]
                if (state["backendActual"] != args.backend
                        or state["failed"] or state["diligentError"] or state["edgeContact"]
                        or state["litPixelCount"] < 100
                        or int(state["frameSerial"]) <= last_frame
                        or color[dominant] <= max(color[i] for i in range(3)
                                                 if i != dominant) * 1.2):
                    raise RuntimeError(f"GPU frame acceptance failed: {state}")
                if name == "red":
                    red_reference = color
                elif (color[1] <= red_reference[1] + 20 or
                      red_reference[0] <= color[0] + 20):
                    raise RuntimeError(f"Light color switch did not affect the GPU model: {state}")
                last_frame = int(state["frameSerial"])
            rejected = call("light.set", {"type": "ambient", "rgb": [-1, 0, 0],
                                          "intensityPercent": 100})
            if not rejected.get("isError"):
                raise RuntimeError("Invalid color was accepted")
            report["finalState"] = call("light.state", {})["structuredContent"]
            if report["finalState"]["colorRGB"] != [0.02, 1, 0.02]:
                # Float storage may round channels; compare within its precision.
                if any(abs(a - b) > 1e-6 for a, b in
                       zip(report["finalState"]["colorRGB"], [0.02, 1, 0.02])):
                    raise RuntimeError("Rejected mutation altered committed state")
            green_difference = report["captures"][-1]["averageRGB"][1] - report["captures"][-1]["averageRGB"][0]
            for name, visible, affected in (("hidden", False, True),
                                             ("model_opt_out", True, False),
                                             ("restored", True, True)):
                controls = call("light.controls", {"visible": visible, "affectedByLights": affected})
                committed = controls["structuredContent"]
                if controls.get("isError") or committed["lightVisible"] != visible or committed["modelAffectedByLights"] != affected:
                    raise RuntimeError(f"Light controls failed: {controls}")
                captured = call("light.capture", {"includeWindow": args.window_proof})
                state = captured.get("structuredContent", {})
                if (captured.get("isError") or state["failed"] or state["diligentError"]
                        or state["backendActual"] != args.backend or state["edgeContact"]
                        or state["litPixelCount"] < 100 or int(state["frameSerial"]) <= last_frame):
                    raise RuntimeError(f"Light control capture failed: {state}")
                last_frame = int(state["frameSerial"])
                images = [item for item in captured.get("content", []) if item.get("type") == "image"]
                if not images:
                    raise RuntimeError("Control capture returned no image")
                (args.output / f"ambient_{name}.png").write_bytes(base64.b64decode(images[0]["data"], validate=True))
                if len(images) > 1:
                    (args.output / f"window_ambient_{name}.png").write_bytes(base64.b64decode(images[1]["data"], validate=True))
                report["captures"].append(state)
                difference = state["averageRGB"][1] - state["averageRGB"][0]
                if ((name != "restored" and difference >= green_difference * 0.5)
                        or (name == "restored" and difference < green_difference * 0.8)):
                    raise RuntimeError(f"Light visibility/model opt-out did not affect GPU color: {state}")
            call("light.set", {"type": "point", "rgb": [0.02, 1, 0.02], "intensityPercent": 1000})
            reference_png = None
            reference_window = None
            for name, shown in (("no_gizmos", False), ("with_gizmos", True)):
                controls = call("light.controls", {"visible": True, "affectedByLights": True,
                                                   "showGizmos": shown})
                if controls.get("isError") or controls["structuredContent"]["showGizmos"] != shown:
                    raise RuntimeError("Gizmo visibility did not commit")
                captured = call("light.capture", {"includeWindow": args.window_proof})
                state = captured.get("structuredContent", {})
                if (captured.get("isError") or state["failed"] or state["diligentError"] or state["edgeContact"]
                        or state["backendActual"] != args.backend or state["litPixelCount"] < 100
                        or int(state["frameSerial"]) <= last_frame):
                    raise RuntimeError(f"Gizmo contamination check failed: {state}")
                last_frame = int(state["frameSerial"])
                images = [item for item in captured.get("content", []) if item.get("type") == "image"]
                if not images:
                    raise RuntimeError("Gizmo check returned no GPU image")
                png = base64.b64decode(images[0]["data"], validate=True)
                (args.output / f"point_{name}.png").write_bytes(png)
                report["captures"].append(state)
                if reference_png is None:
                    reference_png = png
                elif png != reference_png:
                    raise RuntimeError("Editor gizmos changed the final GPU composition")
                if args.window_proof:
                    if len(images) < 2:
                        raise RuntimeError(f"Window proof unavailable: {state}")
                    window = base64.b64decode(images[1]["data"], validate=True)
                    (args.output / f"window_point_{name}.png").write_bytes(window)
                    if reference_window is None:
                        reference_window = window
                    elif window == reference_window:
                        raise RuntimeError("Window did not change when editor light controls were enabled")
            report["finalState"] = call("light.state", {})["structuredContent"]
            report["success"] = True
        except Exception as error:
            report["error"] = str(error)
        finally:
            process.stdin.close()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                report["shutdownTimeout"] = True
                process.kill()
                process.wait()
            report["exitCode"] = process.returncode
            report["crashDump"] = {"path": str(dump_path), "written": dump_path.is_file(),
                                   "bytes": dump_path.stat().st_size if dump_path.is_file() else 0}
            report["success"] &= process.returncode == 0
            reader.join(timeout=2)
            (args.output / "report.json").write_text(
                json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"success": report["success"], "exitCode": report["exitCode"],
                      "error": report.get("error"), "output": str(args.output)}))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
