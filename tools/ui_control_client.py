#!/usr/bin/env python3
"""Drive the running ArtifactStudio UI through its localhost control endpoint.

Start the application with ``--ui-mcp-port <port>`` and call this client
instead of OS level automation: no window activation, no cursor movement and
no focus stealing. Input is synthesized inside Qt as QMouseEvent/QKeyEvent/
QWheelEvent, so every widget's existing mouse and key handling runs unchanged.

The endpoint speaks newline-free JSON-RPC over a single TCP connection using
LSP style framing (``Content-Length: <n>\\r\\n\\r\\n<body>``).

Examples
--------
    python tools/ui_control_client.py --port 7801 list
    python tools/ui_control_client.py --port 7801 click --target layerListWidget
    python tools/ui_control_client.py --port 7801 drag --from-x 120 --from-y 300 --to-x 420 --to-y 300
    python tools/ui_control_client.py --port 7801 key --sequence Ctrl+Z
    python tools/ui_control_client.py --port 7801 call ui.mouseWheel --arguments '{"deltaY": -120}'
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
from typing import Any, Optional

DEFAULT_HOST = "127.0.0.1"
PROTOCOL_VERSION = "2024-11-05"


class UiControlClient:
    def __init__(self, port: int, host: str = DEFAULT_HOST, timeout: float = 15.0) -> None:
        self._socket = socket.create_connection((host, port), timeout=timeout)
        self._socket.settimeout(timeout)
        self._buffer = b""
        self._next_id = 0

    def close(self) -> None:
        try:
            self._socket.close()
        except OSError:
            pass

    def __enter__(self) -> "UiControlClient":
        self.handshake()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def handshake(self) -> dict[str, Any]:
        result = self.request(
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "clientInfo": {"name": "artifact-ui-control-client", "version": "1.0"},
                "capabilities": {},
            },
        )
        self._send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        return result

    def request(self, method: str, params: Optional[dict[str, Any]] = None) -> dict[str, Any]:
        self._next_id += 1
        message: dict[str, Any] = {
            "jsonrpc": "2.0",
            "id": self._next_id,
            "method": method,
        }
        if params is not None:
            message["params"] = params
        self._send(message)
        while True:
            response = self._read_frame()
            if response.get("id") != self._next_id:
                continue
            if "error" in response:
                raise RuntimeError(response["error"].get("message", "control endpoint error"))
            return response.get("result", {})

    def call(self, tool: str, arguments: Optional[dict[str, Any]] = None) -> dict[str, Any]:
        return self.request(
            "tools/call", {"name": tool, "arguments": arguments or {}}
        )

    # -- framing -----------------------------------------------------------

    def _send(self, message: dict[str, Any]) -> None:
        body = json.dumps(message, separators=(",", ":")).encode("utf-8")
        self._socket.sendall(b"Content-Length: %d\r\n\r\n" % len(body) + body)

    def _read_frame(self) -> dict[str, Any]:
        while True:
            header_end = self._buffer.find(b"\r\n\r\n")
            if header_end >= 0:
                length = self._content_length(self._buffer[:header_end])
                start = header_end + 4
                if length is not None and len(self._buffer) >= start + length:
                    body = self._buffer[start : start + length]
                    self._buffer = self._buffer[start + length :]
                    return json.loads(body.decode("utf-8"))
            chunk = self._socket.recv(65536)
            if not chunk:
                raise RuntimeError("control endpoint closed the connection")
            self._buffer += chunk

    @staticmethod
    def _content_length(header: bytes) -> Optional[int]:
        for line in header.decode("ascii", "replace").split("\r\n"):
            if line.lower().startswith("content-length:"):
                try:
                    return int(line.split(":", 1)[1].strip())
                except ValueError:
                    return None
        return None


def _print_result(result: dict[str, Any]) -> int:
    structured = result.get("structuredContent")
    if structured is None:
        content = result.get("content") or []
        text = content[0].get("text", "") if content else json.dumps(result)
        sys.stdout.write(text + "\n")
    else:
        sys.stdout.write(json.dumps(structured, indent=2, ensure_ascii=False) + "\n")
    return 1 if result.get("isError") else 0


def _drag_arguments(args: argparse.Namespace) -> dict[str, Any]:
    arguments: dict[str, Any] = {
        "fromX": args.from_x,
        "fromY": args.from_y,
        "toX": args.to_x,
        "toY": args.to_y,
        "steps": args.steps,
        "button": args.button,
    }
    if args.target:
        arguments["target"] = args.target
    return arguments


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, required=True, help="port passed to --ui-mcp-port")
    parser.add_argument("--host", default=DEFAULT_HOST)

    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("list", help="list visible named widgets")

    click = commands.add_parser("click", help="press and release a mouse button")
    click.add_argument("--target")
    click.add_argument("--x", type=int)
    click.add_argument("--y", type=int)
    click.add_argument("--button", default="left", choices=["left", "right", "middle"])

    move = commands.add_parser("move", help="move the virtual pointer")
    move.add_argument("--target")
    move.add_argument("--x", type=int)
    move.add_argument("--y", type=int)

    drag = commands.add_parser("drag", help="drag with a held button")
    drag.add_argument("--target", help="coordinates become widget local when set")
    drag.add_argument("--from-x", type=int, required=True)
    drag.add_argument("--from-y", type=int, required=True)
    drag.add_argument("--to-x", type=int, required=True)
    drag.add_argument("--to-y", type=int, required=True)
    drag.add_argument("--steps", type=int, default=8)
    drag.add_argument("--button", default="left", choices=["left", "right", "middle"])

    button = commands.add_parser("button", help="press or release and hold")
    button.add_argument("action", choices=["press", "release"])
    button.add_argument("--target")
    button.add_argument("--x", type=int)
    button.add_argument("--y", type=int)
    button.add_argument("--button", default="left", choices=["left", "right", "middle"])

    key = commands.add_parser("key", help="send a key press and release")
    key.add_argument("--sequence", required=True, help='portable sequence, e.g. "S" or "Ctrl+Shift+Z"')
    key.add_argument("--target")
    key.add_argument("--text", default="")

    wheel = commands.add_parser("wheel", help="send a wheel event")
    wheel.add_argument("--delta-y", type=int, default=120)
    wheel.add_argument("--delta-x", type=int, default=0)
    wheel.add_argument("--target")
    wheel.add_argument("--x", type=int)
    wheel.add_argument("--y", type=int)

    call = commands.add_parser("call", help="call a tool directly")
    call.add_argument("tool")
    call.add_argument("--arguments", default="{}")

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    with UiControlClient(args.port, args.host) as client:
        if args.command == "list":
            return _print_result(client.call("ui.listWidgets"))
        if args.command == "move":
            return _print_result(
                client.call("ui.mouseMove", _point_arguments(args))
            )
        if args.command == "click":
            arguments = _point_arguments(args)
            arguments["button"] = args.button
            return _print_result(client.call("ui.mouseClick", arguments))
        if args.command == "drag":
            return _print_result(client.call("ui.mouseDrag", _drag_arguments(args)))
        if args.command == "button":
            arguments = _point_arguments(args)
            arguments["button"] = args.button
            arguments["action"] = args.action
            return _print_result(client.call("ui.mouseButton", arguments))
        if args.command == "key":
            arguments = {"sequence": args.sequence, "text": args.text}
            if args.target:
                arguments["target"] = args.target
            return _print_result(client.call("ui.keyPress", arguments))
        if args.command == "wheel":
            arguments = _point_arguments(args)
            arguments["deltaY"] = args.delta_y
            arguments["deltaX"] = args.delta_x
            return _print_result(client.call("ui.mouseWheel", arguments))
        if args.command == "call":
            return _print_result(
                client.call(args.tool, json.loads(args.arguments))
            )
    parser.error("unknown command")
    return 2


def _point_arguments(args: argparse.Namespace) -> dict[str, Any]:
    arguments: dict[str, Any] = {}
    if args.target:
        arguments["target"] = args.target
    if args.x is not None:
        arguments["x"] = args.x
    if args.y is not None:
        arguments["y"] = args.y
    return arguments


if __name__ == "__main__":
    sys.exit(main())
