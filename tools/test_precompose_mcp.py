"""Exercise the standalone precompose GUI through MCP; keep evidence outside the repo."""
import argparse
import base64
import json
from pathlib import Path
import queue
import subprocess
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--language", choices=("ja", "en", "zh", "zh-TW"), default="en")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    replies = queue.Queue()
    evidence = []
    with (args.output_dir / "stderr.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(args.server.resolve()), "--mcp-stdio", "--lang", args.language],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=log, text=True, encoding="utf-8")

        def read_replies():
            for line in process.stdout:
                try:
                    replies.put(json.loads(line))
                except ValueError:
                    replies.put({"invalid_stdout": line})
            replies.put({"eof": True})

        threading.Thread(target=read_replies, daemon=True).start()

        def request(method, params):
            request_id = len(evidence) + 1
            process.stdin.write(json.dumps({"jsonrpc": "2.0", "id": request_id,
                                           "method": method, "params": params}) + "\n")
            process.stdin.flush()
            reply = replies.get(timeout=120)
            evidence.append({"method": method, "params": params, "reply": reply})
            if reply.get("id") != request_id or "error" in reply:
                raise RuntimeError(f"Invalid MCP reply: {reply}")
            return reply["result"]

        def call(name):
            reply = request("tools/call", {"name": name, "arguments": {}})
            if reply.get("isError"):
                raise RuntimeError(f"{name}: {reply}")
            return json.loads(reply["content"][0]["text"])

        def require(condition, message):
            if not condition:
                raise AssertionError(message)

        def layer_map(state):
            return {layer["name"]: layer for layer in state["layers"]}

        try:
            request("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                   "clientInfo": {"name": "precompose-regression", "version": "1"}})
            request("tools/list", {})
            initial = call("test.state")
            original = initial["layers"]
            require(set(layer_map(initial)) == {"A", "B", "Untouched"}, "Fixture missing layers")
            nested = call("test.precompose")
            require(len(nested["layers"]) == 2, "Selected precompose must leave Untouched outside")
            child = next(layer for layer in nested["layers"] if "children" in layer)
            require({layer["name"] for layer in child["children"]} == {"A", "B"}, "Wrong child content")
            require(layer_map(nested)["Untouched"] == layer_map(initial)["Untouched"], "Unselected layer changed")
            for entry in child["children"]:
                source = layer_map(initial)[entry["name"]]
                require(all(entry[key] == source[key] for key in ("id", "position", "opacity")),
                        "Precompose changed identity or attributes")
            require(call("test.undo")["layers"] == original, "Undo did not restore original layer order/state")
            redone = call("test.redo")
            require(redone["layers"] == nested["layers"], "Redo changed precompose/child identity")
            kept = call("test.unpackKeep")
            require(kept["layers"] == original and kept["lastChildExists"], "Unpack keep contract failed")
            require(call("test.undo")["layers"] == nested["layers"], "Unpack undo failed")
            removed = call("test.unpackDelete")
            require(removed["layers"] == original and not removed["lastChildExists"], "Unpack delete failed")
            restored = call("test.undo")
            require(restored["lastChildExists"] and restored["layers"] == nested["layers"], "Deleted child undo failed")
            call("test.reset")
            all_nested = call("test.precomposeAll")
            require(len(all_nested["layers"]) == 1 and len(all_nested["layers"][0]["children"]) == 3,
                    "Move all must include unselected layer")
            image = request("tools/call", {"name": "test.capture", "arguments": {}})
            require(not image.get("isError") and image["content"][0]["type"] == "image", "Capture failed")
            (args.output_dir / "nested.png").write_bytes(base64.b64decode(image["content"][0]["data"]))
            evidence[-1]["reply"] = {"image": "nested.png"}
            require(call("test.runRegression")["regressionFailures"] == 0, "Existing regression suite failed")
            print("PASS: selected/all, unpack keep/delete, identity, attributes, order, undo/redo, regression suite")
        finally:
            process.stdin.close()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=10)
            (args.output_dir / "result.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2),
                                                         encoding="utf-8")
        if process.returncode:
            raise RuntimeError(f"Server exit code: {process.returncode}")


if __name__ == "__main__":
    main()
