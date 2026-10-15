#!/usr/bin/env python3
"""Run JSON tool steps against a GUI test's stdio MCP server without UI input."""

import argparse
import base64
import ctypes
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time


def desktop_state():
    if os.name != "nt":
        return None

    class Point(ctypes.Structure):
        _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

    user32 = ctypes.windll.user32
    user32.GetForegroundWindow.restype = ctypes.c_void_p
    point = Point()
    user32.GetCursorPos(ctypes.byref(point))
    return {"foregroundWindow": user32.GetForegroundWindow(),
            "cursor": [point.x, point.y]}


class StdioClient:
    # Responses may contain a PNG. Limit queued messages and each wire frame.
    max_frame_bytes = 64 * 1024 * 1024

    def __init__(self, command, environment, log_file, timeout):
        self.process = subprocess.Popen(
            command, cwd=Path(command[0]).parent, env=environment,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log_file,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        self.timeout = timeout
        self.responses = queue.Queue(maxsize=8)
        self.request_id = 0
        self.stopping = threading.Event()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        try:
            while not self.stopping.is_set():
                frame = self.process.stdout.readline(self.max_frame_bytes + 1)
                if not frame:
                    raise RuntimeError("MCP server stdout closed")
                if len(frame) > self.max_frame_bytes:
                    raise RuntimeError("MCP response exceeded 64 MiB")
                if frame.strip():
                    self.responses.put(json.loads(frame), timeout=1)
        except Exception as error:
            try:
                self.responses.put(error, timeout=1)
            except queue.Full:
                pass

    def request(self, method, params):
        self.request_id += 1
        params = dict(params)
        params["_meta"] = {
            "io.modelcontextprotocol/protocolVersion": "2026-07-28",
            "io.modelcontextprotocol/clientCapabilities": {},
            "io.modelcontextprotocol/clientInfo": {
                "name": "artifact-test-client", "version": "1.0.0"}}
        wire = {"jsonrpc": "2.0", "id": self.request_id,
                "method": method, "params": params}
        self.process.stdin.write((json.dumps(wire) + "\n").encode("utf-8"))
        self.process.stdin.flush()
        try:
            response = self.responses.get(timeout=self.timeout)
        except queue.Empty as error:
            raise TimeoutError(f"{method}: no response within {self.timeout}s; "
                               f"server PID {self.process.pid}") from error
        if isinstance(response, Exception):
            raise response
        if response.get("id") != self.request_id:
            raise RuntimeError(f"Unexpected response id: {response.get('id')}")
        if "error" in response:
            raise RuntimeError(f"MCP error: {response['error']}")
        return response["result"]

    def close(self, shutdown_timeout=5):
        self.stopping.set()
        try:
            self.process.stdin.close()
            self.process.wait(timeout=shutdown_timeout)
            outcome = "exited-after-stdin-close"
        except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
            self.process.terminate()
            self.process.wait(timeout=5)
            outcome = "terminated-after-timeout"
        self.reader.join(timeout=1)
        return outcome


def save_images(result, output, image_name, step_index):
    images = []
    for content in result.get("content", []):
        if content.get("type") != "image":
            continue
        if content.get("mimeType") != "image/png":
            raise ValueError("Only PNG capture content is supported")
        name = image_name or f"step_{step_index:02d}.png"
        path = (output / name).resolve()
        if not path.is_relative_to(output) or path.suffix.lower() != ".png":
            raise ValueError("Capture image must be a relative PNG path inside output-dir")
        if images:
            path = path.with_stem(f"{path.stem}_{len(images)}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(base64.b64decode(content["data"], validate=True))
        images.append(str(path))
    if image_name and not images:
        raise RuntimeError("Step requested a capture artifact but returned no image")
    return images


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--server-arg", action="append", default=[])
    parser.add_argument("--steps", required=True, type=Path,
                        help="JSON array of {tool, arguments, image?} steps")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--shutdown-timeout", type=float, default=5,
                        help="Grace period for the GUI server to exit after stdin closes")
    parser.add_argument("--attach-wait", action="store_true",
                        help="Wait for Enter after starting the server, so a debugger can attach")
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--fail-log-pattern", help="Fail if stderr contains this regex")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("timeout must be positive")
    if args.shutdown_timeout <= 0:
        parser.error("shutdown-timeout must be positive")
    server = args.server.resolve(strict=True)
    steps = json.loads(args.steps.read_text(encoding="utf-8-sig"))
    if not isinstance(steps, list) or not steps:
        parser.error("steps must contain a nonempty JSON array")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["PATH"] = str(server.parent) + os.pathsep + environment.get("PATH", "")
    for value in args.env:
        key, separator, data = value.partition("=")
        if not separator or not key:
            parser.error("env must use NAME=VALUE")
        environment[key] = data
    pattern = re.compile(args.fail_log_pattern) if args.fail_log_pattern else None
    report = {"server": str(server), "desktopBefore": desktop_state(), "steps": []}
    client = None
    failed = False
    log_path = output / "server-stderr.log"
    with log_path.open("wb") as log:
        try:
            client = StdioClient([str(server), *args.server_arg], environment, log, args.timeout)
            report["serverPid"] = client.process.pid
            if args.attach_wait:
                print(f"Test server PID {client.process.pid}. Attach Visual Studio, then press Enter to continue.",
                      flush=True)
                input()
            report["discovery"] = client.request("server/discover", {})
            for index, step in enumerate(steps):
                if not isinstance(step, dict) or not isinstance(step.get("tool"), str):
                    raise ValueError(f"Step {index}: tool must be a string")
                arguments = step.get("arguments", {})
                if not isinstance(arguments, dict):
                    raise ValueError(f"Step {index}: arguments must be an object")
                started = time.monotonic()
                result = client.request("tools/call", {"name": step["tool"], "arguments": arguments})
                if result.get("isError"):
                    raise RuntimeError(f"{step['tool']}: {result.get('structuredContent', result)}")
                entry = {"tool": step["tool"], "arguments": arguments,
                         "elapsedSeconds": round(time.monotonic() - started, 4),
                         "state": result.get("structuredContent"),
                         "images": save_images(result, output, step.get("image"), index)}
                report["steps"].append(entry)
                print(f"{index + 1}/{len(steps)} {step['tool']}: {entry['elapsedSeconds']:.3f}s", flush=True)
        except Exception as error:
            failed = True
            report["error"] = str(error)
        finally:
            if client:
                report["shutdown"] = client.close(args.shutdown_timeout)
                if report["shutdown"] != "exited-after-stdin-close":
                    failed = True
    report["desktopAfter"] = desktop_state()
    if report["desktopBefore"] and report["desktopAfter"]:
        report["foregroundUnchanged"] = (
            report["desktopBefore"]["foregroundWindow"] == report["desktopAfter"]["foregroundWindow"])
        report["cursorUnchanged"] = (
            report["desktopBefore"]["cursor"] == report["desktopAfter"]["cursor"])
    if pattern:
        with log_path.open(encoding="utf-8", errors="replace") as log:
            report["logPatternMatches"] = sum(bool(pattern.search(line)) for line in log)
        failed |= report["logPatternMatches"] > 0
    report["passed"] = not failed
    report_path = output / "result.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{'PASS' if not failed else 'FAIL'}: {report_path}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
