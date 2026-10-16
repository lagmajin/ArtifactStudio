"""Exercise the standalone precompose GUI through MCP; keep evidence outside the repo."""
import argparse
import base64
import json
from pathlib import Path
import queue
import struct
import subprocess
import sys
import threading
import zlib


def validate_png(png):
    signature = b"\x89PNG\r\n\x1a\n"
    if not png.startswith(signature):
        raise AssertionError("Capture returned an invalid PNG signature")
    offset = len(signature)
    chunk_types = []
    image_header = None
    image_data = []
    seen_image_data = False
    image_data_ended = False
    while offset + 12 <= len(png):
        length = struct.unpack_from(">I", png, offset)[0]
        chunk_end = offset + 12 + length
        if chunk_end > len(png):
            raise AssertionError("Capture PNG contains a truncated chunk")
        chunk_type = png[offset + 4:offset + 8]
        chunk_data = png[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack_from(">I", png, offset + 8 + length)[0]
        actual_crc = zlib.crc32(chunk_data, zlib.crc32(chunk_type)) & 0xffffffff
        if actual_crc != expected_crc:
            raise AssertionError(f"Capture PNG has an invalid {chunk_type!r} CRC")
        if not chunk_types and (chunk_type != b"IHDR" or length != 13):
            raise AssertionError("Capture PNG does not start with a valid IHDR chunk")
        chunk_types.append(chunk_type)
        if chunk_type == b"IHDR":
            image_header = chunk_data
        elif chunk_type == b"IDAT":
            if image_data_ended:
                raise AssertionError("Capture PNG contains nonconsecutive IDAT chunks")
            seen_image_data = True
            image_data.append(chunk_data)
        elif seen_image_data and chunk_type != b"IEND":
            image_data_ended = True
        if chunk_type == b"IEND":
            if length != 0 or chunk_end != len(png):
                raise AssertionError("Capture PNG has an invalid IEND chunk or trailing data")
            break
        offset = chunk_end
    if (not chunk_types or chunk_types[0] != b"IHDR" or chunk_types.count(b"IHDR") != 1
            or b"IDAT" not in chunk_types or chunk_types[-1] != b"IEND"):
        raise AssertionError("Capture PNG is missing required IHDR, IDAT, or IEND chunks")
    if len(png) < len(signature) + 8 + 13:
        raise AssertionError("Capture PNG has a truncated IHDR chunk")
    width, height = struct.unpack_from(">II", image_header)
    if width <= 0 or height <= 0:
        raise AssertionError("Capture PNG has empty dimensions")
    bit_depth, color_type, compression_method, filter_method, interlace_method = image_header[8:13]
    channels_by_color_type = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
    valid_bit_depths = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8),
                        4: (8, 16), 6: (8, 16)}
    if (color_type not in channels_by_color_type or bit_depth not in valid_bit_depths[color_type]
            or compression_method != 0 or filter_method != 0 or interlace_method != 0):
        raise AssertionError("Capture PNG uses an unsupported IHDR encoding")
    row_bytes = (width * channels_by_color_type[color_type] * bit_depth + 7) // 8
    scanline_bytes = row_bytes + 1
    expected_data_length = height * scanline_bytes
    if expected_data_length > 256 * 1024 * 1024:
        raise AssertionError("Capture PNG scanline data is unexpectedly large")
    decoder = zlib.decompressobj()
    try:
        decoded = decoder.decompress(b"".join(image_data), expected_data_length + 1)
    except zlib.error as error:
        raise AssertionError("Capture PNG IDAT stream cannot be decompressed") from error
    if (len(decoded) != expected_data_length or not decoder.eof or decoder.unused_data
            or decoder.unconsumed_tail):
        raise AssertionError("Capture PNG IDAT stream does not match its dimensions")
    if any(decoded[row_start] > 4 for row_start in range(0, len(decoded), scanline_bytes)):
        raise AssertionError("Capture PNG contains an invalid scanline filter")
    return width, height


def parse_tool_result(name, reply):
    content = reply.get("content")
    if (not isinstance(content, list) or len(content) != 1
            or not isinstance(content[0], dict)
            or content[0].get("type") != "text"
            or not isinstance(content[0].get("text"), str)):
        raise RuntimeError(f"{name} returned an invalid MCP text content envelope: {reply}")
    try:
        result = json.loads(content[0]["text"])
    except json.JSONDecodeError as error:
        raise RuntimeError(f"{name} returned invalid JSON text: {content[0]['text']!r}") from error
    if not isinstance(result, dict):
        raise RuntimeError(f"{name} returned a non-object JSON result: {result!r}")
    return result


def parse_tool_error(name, reply):
    if not isinstance(reply, dict) or reply.get("isError") is not True:
        raise RuntimeError(f"{name} did not return an MCP tool error: {reply}")
    result = parse_tool_result(name, reply)
    if not isinstance(result.get("error"), str) or not result["error"].strip():
        raise RuntimeError(f"{name} returned an MCP error without a non-empty error message: {result}")
    return result


def validate_tool_result_contract():
    valid_reply = {"content": [{"type": "text", "text": "{\"passed\":true}"}]}
    if parse_tool_result("fixture", valid_reply) != {"passed": True}:
        raise AssertionError("MCP tool result parser rejected a valid JSON object")
    invalid_replies = {
        "missing content": {},
        "empty content": {"content": []},
        "multiple content items": {"content": [
            {"type": "text", "text": "{}"}, {"type": "text", "text": "{}"}]},
        "image content": {"content": [{"type": "image", "data": "AA=="}]},
        "invalid JSON": {"content": [{"type": "text", "text": "{"}]},
        "non-object JSON": {"content": [{"type": "text", "text": "[]"}]},
    }
    for label, reply in invalid_replies.items():
        try:
            parse_tool_result("fixture", reply)
        except RuntimeError:
            continue
        raise AssertionError(f"MCP tool result parser accepted {label}")

    valid_error = {"isError": True,
                   "content": [{"type": "text", "text": "{\"error\":\"rejected\"}"}]}
    if parse_tool_error("fixture", valid_error) != {"error": "rejected"}:
        raise AssertionError("MCP tool error parser rejected a valid error result")
    invalid_errors = {
        "missing isError flag": {"content": valid_error["content"]},
        "empty error message": {"isError": True,
                                "content": [{"type": "text", "text": "{\"error\":\" \"}"}]},
        "missing error message": {"isError": True,
                                  "content": [{"type": "text", "text": "{}"}]},
    }
    for label, reply in invalid_errors.items():
        try:
            parse_tool_error("fixture", reply)
        except RuntimeError:
            continue
        raise AssertionError(f"MCP tool error parser accepted {label}")


def make_png_chunk(chunk_type, data):
    crc = zlib.crc32(data, zlib.crc32(chunk_type)) & 0xffffffff
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def validate_png_contract():
    signature = b"\x89PNG\r\n\x1a\n"
    header = struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0)

    def image(scanline):
        return (signature + make_png_chunk(b"IHDR", header)
                + make_png_chunk(b"IDAT", zlib.compress(scanline))
                + make_png_chunk(b"IEND", b""))

    valid_png = image(b"\x00\x7f")
    decoded_dimensions = validate_png(valid_png)
    if decoded_dimensions != (1, 1):
        raise AssertionError("PNG validator rejected a valid one-pixel fixture")

    invalid_images = {
        "invalid signature": b"\x00" + valid_png[1:],
        "missing IHDR": (
            signature + make_png_chunk(b"IDAT", zlib.compress(b"\x00\x7f"))
            + make_png_chunk(b"IEND", b"")),
        "missing IDAT": (
            signature + make_png_chunk(b"IHDR", header) + make_png_chunk(b"IEND", b"")),
        "invalid zlib stream": (
            signature + make_png_chunk(b"IHDR", header)
            + make_png_chunk(b"IDAT", b"invalid zlib data")
            + make_png_chunk(b"IEND", b"")),
        "unsupported interlace mode": (
            signature + make_png_chunk(
                b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 1))
            + make_png_chunk(b"IDAT", zlib.compress(b"\x00\x7f"))
            + make_png_chunk(b"IEND", b"")),
        "CRC corruption": valid_png[:29] + bytes([valid_png[29] ^ 1]) + valid_png[30:],
        "invalid filter": image(b"\x05\x7f"),
        "scanline size mismatch": image(b"\x00"),
        "scanline data exceeds dimensions": image(b"\x00\x7f\x80"),
        "decompressed image exceeds size limit": (
            signature + make_png_chunk(
                b"IHDR", struct.pack(">IIBBBBB", 0xffffffff, 0xffffffff, 8, 0, 0, 0, 0))
            + make_png_chunk(b"IDAT", zlib.compress(b""))
            + make_png_chunk(b"IEND", b"")),
        "compressed stream trailing data": (
            signature + make_png_chunk(b"IHDR", header)
            + make_png_chunk(b"IDAT", zlib.compress(b"\x00\x7f") + b"\xff")
            + make_png_chunk(b"IEND", b"")),
        "nonconsecutive IDAT chunks": (
            signature + make_png_chunk(b"IHDR", header)
            + make_png_chunk(b"IDAT", zlib.compress(b"\x00\x7f"))
            + make_png_chunk(b"tEXt", b"test\x00value")
            + make_png_chunk(b"IDAT", b"") + make_png_chunk(b"IEND", b"")),
        "trailing data after IEND": valid_png + b"\x00",
    }
    for label, invalid_png in invalid_images.items():
        try:
            validate_png(invalid_png)
        except AssertionError:
            continue
        raise AssertionError(f"PNG validator accepted {label}")


def main():
    validate_png_contract()
    validate_tool_result_contract()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--language", choices=("ja", "en", "zh", "zh-TW", "all"), default="en")
    args = parser.parse_args()
    if args.language == "all":
        args.output_dir.mkdir(parents=True, exist_ok=True)
        languages = ("ja", "en", "zh", "zh-TW")
        language_results = []
        for language in languages:
            completed = subprocess.run([
                sys.executable, str(Path(__file__).resolve()),
                "--server", str(args.server.resolve()),
                "--output-dir", str((args.output_dir / language).resolve()),
                "--language", language,
            ], check=False)
            language_results.append({"language": language, "exitCode": completed.returncode})
        all_passed = all(result["exitCode"] == 0 for result in language_results)
        (args.output_dir / "language-results.json").write_text(
            json.dumps({"expectedLanguages": list(languages), "languageRuns": language_results,
                        "passed": all_passed}, ensure_ascii=False, indent=2),
            encoding="utf-8")
        failed_languages = [result["language"] for result in language_results
                            if result["exitCode"] != 0]
        if failed_languages:
            raise SystemExit(
                "Precompose MCP test failed for languages: " + ", ".join(failed_languages))
        return
    args.output_dir.mkdir(parents=True, exist_ok=True)
    replies = queue.Queue()
    evidence = []
    capture_files_written = []
    exposed_tool_names = set()
    invoked_tool_names = set()
    with (args.output_dir / "stderr.log").open("w", encoding="utf-8") as log:
        try:
            process = subprocess.Popen(
                [str(args.server.resolve()), "--mcp-stdio", "--lang", args.language],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=log, text=True, encoding="utf-8")
        except OSError as error:
            evidence.append({"harnessError": f"{type(error).__name__}: {error}"})
            evidence.append({"serverExitCode": None})
            evidence.append({"serverForcedTermination": False, "serverForcedKill": False})
            evidence.append({"harnessPassed": False})
            evidence.append({"captureFiles": []})
            (args.output_dir / "result.json").write_text(
                json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8")
            raise

        def read_replies():
            try:
                for line in process.stdout:
                    try:
                        replies.put(json.loads(line))
                    except ValueError:
                        replies.put({"invalid_stdout": line})
            except (OSError, UnicodeError) as error:
                replies.put({"stdoutReadError": f"{type(error).__name__}: {error}"})
            finally:
                replies.put({"eof": True})

        reader_thread = threading.Thread(target=read_replies, daemon=True)
        reader_thread.start()

        def request(method, params, allow_protocol_error=False):
            request_id = len(evidence) + 1
            record = {"method": method, "params": params, "requestId": request_id}
            evidence.append(record)
            try:
                process.stdin.write(json.dumps({"jsonrpc": "2.0", "id": request_id,
                                               "method": method, "params": params}) + "\n")
                process.stdin.flush()
                reply = replies.get(timeout=120)
            except Exception as error:
                record["transportError"] = f"{type(error).__name__}: {error}"
                raise
            record["reply"] = reply
            if (not isinstance(reply, dict) or reply.get("jsonrpc") != "2.0"
                    or reply.get("id") != request_id
                    or ("result" in reply) == ("error" in reply)
                    or ("error" in reply and not allow_protocol_error)):
                raise RuntimeError(f"Invalid MCP reply: {reply}")
            if allow_protocol_error:
                return reply
            return reply["result"]

        def notify(method, params):
            record = {"method": method, "params": params, "notification": True}
            evidence.append(record)
            try:
                process.stdin.write(json.dumps({"jsonrpc": "2.0", "method": method,
                                               "params": params}) + "\n")
                process.stdin.flush()
            except Exception as error:
                record["transportError"] = f"{type(error).__name__}: {error}"
                raise

        def call(name):
            if name not in exposed_tool_names:
                raise AssertionError(f"MCP tools/list did not advertise {name}")
            invoked_tool_names.add(name)
            reply = request("tools/call", {"name": name, "arguments": {}})
            if reply.get("isError"):
                raise RuntimeError(f"{name}: {parse_tool_error(name, reply)}")
            return parse_tool_result(name, reply)

        def require(condition, message):
            if not condition:
                raise AssertionError(message)

        def layer_map(state):
            return {layer["name"]: layer for layer in state["layers"]}

        def without_history(state):
            return {key: value for key, value in state.items()
                    if key not in ("canUndo", "canRedo")}

        def require_child_settings(result, label):
            require(result["childSettingsCopied"]
                    and result["childResolution"] == [1280, 720]
                    and abs(result["childFrameRate"] - 24.0) < 1e-6
                    and result["childFrameRangeDuration"] == 300
                    and result["parentFrameRangeDuration"] == 300
                    and all(abs(actual - expected) < 1e-6 for actual, expected in
                            zip(result["childBackgroundColor"], (0.125, 0.25, 0.5, 1.0))),
                    f"{label} precompose did not copy custom composition settings")

        tests_passed = False
        try:
            initialized = request("initialize", {
                "protocolVersion": "2024-11-05", "capabilities": {},
                "clientInfo": {"name": "precompose-regression", "version": "1"}})
            require(initialized.get("protocolVersion") == "2024-11-05"
                    and initialized.get("capabilities") == {"tools": {}}
                    and initialized.get("serverInfo") == {
                        "name": "artifact-precompose-test", "version": "1.0.0"},
                    "MCP initialize response changed its protocol or server capabilities")
            require(request("ping", {}) == {}, "MCP ping did not return an empty result")
            invoked_tool_names.add("test.state")
            state_without_arguments = request("tools/call", {"name": "test.state"})
            state_without_arguments_result = json.loads(state_without_arguments["content"][0]["text"])
            require(not state_without_arguments.get("isError") and
                    isinstance(state_without_arguments_result, dict),
                    "MCP tool call without optional arguments was rejected")
            catalog = request("tools/list", {})
            listed_tools = catalog.get("tools")
            require(isinstance(listed_tools, list), "MCP tools/list did not return a tools array")
            names = [tool.get("name") for tool in listed_tools if isinstance(tool, dict)]
            require(len(names) == len(listed_tools)
                    and all(isinstance(name, str) and name.strip() for name in names),
                    "MCP tools/list returned a missing or invalid tool name")
            require(len(names) == len(set(names)),
                    "MCP tools/list returned duplicate tool names")
            required_tools = {
                "test.state", "test.reset", "test.resetFromChild", "test.runRegression",
                "test.prepareInvalidCapturePanel", "test.prepareMismatchedCapturePanel",
                "test.refreshStatePanel", "test.hideCaptureWindow", "test.showCaptureWindow",
                "test.minimizeCaptureWindow", "test.restoreCaptureWindow", "test.capture",
            }
            require(required_tools.issubset(set(names)),
                    f"MCP tools/list omitted required tools: {sorted(required_tools - set(names))}")
            require(all(isinstance(tool.get("description"), str)
                        and tool["description"].strip()
                        and tool.get("inputSchema") == {
                            "type": "object", "additionalProperties": False}
                        for tool in listed_tools),
                    "MCP tools/list returned an invalid description or input schema")
            exposed_tool_names.update(names)
            protocol_state_before_invalid_calls = call("test.state")
            require(protocol_state_before_invalid_calls["locale"] == args.language,
                    f"Requested locale {args.language} was not applied to the test window")
            require(state_without_arguments_result == protocol_state_before_invalid_calls,
                    "MCP tool call without arguments changed the precompose fixture")
            notify("notifications/initialized", {})
            require(call("test.state") == protocol_state_before_invalid_calls,
                    "MCP initialized notification changed state or produced an unexpected response")
            unknown_tool = request("tools/call", {"name": "test.notARealTool", "arguments": {}})
            require("Unknown tool" in parse_tool_error("unknown tool", unknown_tool)["error"],
                    "Unknown MCP tool did not return a tool error")
            unexpected_argument = request("tools/call", {
                "name": "test.state", "arguments": {"unexpected": True}})
            require("empty object arguments" in parse_tool_error(
                    "unexpected argument", unexpected_argument)["error"],
                    "MCP tool accepted unsupported arguments")
            missing_tool_name = request("tools/call", {"arguments": {}})
            require("tool name" in parse_tool_error(
                    "missing tool name", missing_tool_name)["error"],
                    "MCP tools/call accepted a request with no tool name")
            non_object_arguments = request("tools/call", {
                "name": "test.state", "arguments": []})
            require("empty object arguments" in parse_tool_error(
                    "non-object arguments", non_object_arguments)["error"],
                    "MCP tools/call accepted non-object arguments")
            unknown_method = request("method/not-found", {}, allow_protocol_error=True)
            require(unknown_method.get("error", {}).get("code") == -32601,
                    "Unknown MCP method did not return JSON-RPC method-not-found")
            malformed_json = '{"jsonrpc":"2.0","id":999,"method":"ping"'
            malformed_record = {"rawRequest": malformed_json}
            evidence.append(malformed_record)
            process.stdin.write(malformed_json + "\n")
            process.stdin.flush()
            malformed_reply = replies.get(timeout=120)
            malformed_record["reply"] = malformed_reply
            require(isinstance(malformed_reply, dict)
                    and malformed_reply.get("jsonrpc") == "2.0"
                    and malformed_reply.get("id") is None
                    and malformed_reply.get("error", {}).get("code") == -32700,
                    f"Malformed JSON-RPC input did not return parse-error: {malformed_reply}")
            protocol_state_after_invalid_calls = call("test.state")
            require(protocol_state_after_invalid_calls == protocol_state_before_invalid_calls,
                    "Invalid MCP requests mutated the precompose fixture")
            reset_from_child = call("test.resetFromChild")
            require(reset_from_child["passed"] and reset_from_child["childWasActiveBeforeReset"]
                    and reset_from_child["createdFreshRoot"]
                    and reset_from_child["fixtureUsesFreshLayerIds"]
                    and reset_from_child["rootIsActive"]
                    and reset_from_child["fixtureRestored"] and reset_from_child["selectionCleared"]
                    and reset_from_child["historyCleared"] and reset_from_child["localePreserved"],
                    "MCP reset failed to leave an active child and restore the root fixture")
            reset_state = reset_from_child["state"]
            require(reset_state["compositionId"]
                    and reset_state["locale"] == args.language
                    and [layer["name"] for layer in reset_state["layers"]] ==
                    ["A", "B", "Untouched"]
                    and "lastChildExists" not in reset_state
                    and not reset_state["canUndo"] and not reset_state["canRedo"]
                    and reset_state["selection"]["activeCompositionId"] ==
                    reset_state["compositionId"]
                    and reset_state["selection"]["selectedLayerIds"] == []
                    and "currentLayerId" not in reset_state["selection"]
                    and reset_state["resolution"] == [1280, 720]
                    and abs(reset_state["frameRate"] - 24.0) < 1e-6
                    and reset_state["frameRangeDuration"] == 300
                    and reset_state["backgroundColor"] == [0.125, 0.25, 0.5, 1.0]
                    and {name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                         for name, layer in layer_map(reset_state).items()} ==
                    {"A": (40, 200, 7), "B": (70, 160, 9), "Untouched": (25, 220, 3)}
                    and layer_map(reset_state)["A"]["position"] == [12.0, -8.0, 3.0]
                    and abs(layer_map(reset_state)["A"]["opacity"] - 0.42) < 1e-6,
                    "MCP reset did not create a clean locale-preserving fixture")
            require("test.capture" in exposed_tool_names,
                    "MCP tools/list did not advertise test.capture")
            invoked_tool_names.add("test.capture")
            recovered_capture_dimensions = None
            for prepare_tool, expected_panel in (
                    ("test.prepareInvalidCapturePanel", "{"),
                    ("test.prepareMismatchedCapturePanel", "{}")):
                prepared_panel = call(prepare_tool)
                require(prepared_panel["panelPrepared"]
                        and prepared_panel["panelContent"] == expected_panel
                        and prepared_panel["state"] == reset_state
                        and call("test.state") == reset_state,
                        f"{prepare_tool} changed the fixture or failed to prepare the visible panel")
                stale_capture = request("tools/call", {
                    "name": "test.capture", "arguments": {}})
                capture_error = parse_tool_error("test.capture", stale_capture)
                require(capture_error.get("error") == "Visible MCP state panel is stale"
                        and call("test.state") == reset_state,
                        f"Capture did not reject {prepare_tool} without changing fixture state")
                refreshed_panel = call("test.refreshStatePanel")
                require(refreshed_panel["panelRefreshed"]
                        and refreshed_panel["state"] == reset_state
                        and call("test.state") == reset_state,
                        f"State panel did not recover after {prepare_tool}")
                recovered_capture = request("tools/call", {
                    "name": "test.capture", "arguments": {}})
                require(not recovered_capture.get("isError")
                        and len(recovered_capture.get("content", [])) == 1
                        and recovered_capture["content"][0].get("type") == "image"
                        and recovered_capture["content"][0].get("mimeType") == "image/png",
                        f"Capture did not recover after {prepare_tool}")
                recovered_png = base64.b64decode(
                    recovered_capture["content"][0]["data"], validate=True)
                current_dimensions = validate_png(recovered_png)
                if recovered_capture_dimensions is None:
                    recovered_capture_dimensions = current_dimensions
                require(current_dimensions == recovered_capture_dimensions
                        and call("test.state") == reset_state,
                        f"Recovered capture after {prepare_tool} changed size or project state")
                evidence[-1]["reply"] = {
                    "captureValidated": True, "dimensions": current_dimensions}
            hidden_window = call("test.hideCaptureWindow")
            require(hidden_window["windowHidden"] and hidden_window["state"] == reset_state
                    and call("test.state") == reset_state,
                    "Hiding the capture window changed the restored fixture")
            hidden_capture = request("tools/call", {
                "name": "test.capture", "arguments": {}})
            require(parse_tool_error("test.capture", hidden_capture).get("error") ==
                    "Capture window is not visible"
                    and call("test.state") == reset_state,
                    "Capture succeeded or changed fixture while the test window was hidden")
            shown_window = call("test.showCaptureWindow")
            require(shown_window["windowVisible"] and shown_window["state"] == reset_state
                    and call("test.state") == reset_state,
                    "Capture window did not recover visibility without changing fixture")
            visible_capture = request("tools/call", {
                "name": "test.capture", "arguments": {}})
            require(not visible_capture.get("isError")
                    and len(visible_capture.get("content", [])) == 1
                    and visible_capture["content"][0].get("type") == "image"
                    and visible_capture["content"][0].get("mimeType") == "image/png",
                    "Capture did not recover after the test window was shown")
            visible_png = base64.b64decode(visible_capture["content"][0]["data"], validate=True)
            visible_dimensions = validate_png(visible_png)
            require(visible_dimensions == recovered_capture_dimensions
                    and call("test.state") == reset_state,
                    "Visible capture after window recovery changed size or fixture state")
            evidence[-1]["reply"] = {
                "captureValidated": True, "dimensions": visible_dimensions}
            minimized_window = call("test.minimizeCaptureWindow")
            require(minimized_window["windowMinimized"]
                    and minimized_window["state"] == reset_state
                    and call("test.state") == reset_state,
                    "Minimizing the capture window changed fixture state or was not applied")
            minimized_capture = request("tools/call", {
                "name": "test.capture", "arguments": {}})
            require(parse_tool_error("test.capture", minimized_capture).get("error") ==
                    "Capture window is not visible"
                    and call("test.state") == reset_state,
                    "Capture succeeded or changed fixture while the test window was minimized")
            restored_window = call("test.restoreCaptureWindow")
            require(restored_window["windowVisible"] and restored_window["state"] == reset_state
                    and call("test.state") == reset_state,
                    "Minimized capture window did not restore without changing fixture state")
            restored_capture = request("tools/call", {
                "name": "test.capture", "arguments": {}})
            require(not restored_capture.get("isError")
                    and len(restored_capture.get("content", [])) == 1
                    and restored_capture["content"][0].get("type") == "image"
                    and restored_capture["content"][0].get("mimeType") == "image/png",
                    "Capture did not recover after restoring the minimized window")
            restored_png = base64.b64decode(
                restored_capture["content"][0]["data"], validate=True)
            restored_dimensions = validate_png(restored_png)
            require(restored_dimensions == recovered_capture_dimensions
                    and call("test.state") == reset_state,
                    "Restored capture changed size or fixture state")
            evidence[-1]["reply"] = {
                "captureValidated": True, "dimensions": restored_dimensions}
            reset_capture_state = call("test.state")
            reset_capture = request("tools/call", {"name": "test.capture", "arguments": {}})
            require(not reset_capture.get("isError") and reset_capture["content"][0]["type"] == "image"
                    and reset_capture["content"][0].get("mimeType") == "image/png",
                    "Capture immediately after reset did not return a PNG image")
            reset_png = base64.b64decode(reset_capture["content"][0]["data"], validate=True)
            reset_dimensions = validate_png(reset_png)
            (args.output_dir / "reset.png").write_bytes(reset_png)
            capture_files_written.append("reset.png")
            evidence[-1]["reply"] = {"image": "reset.png", "dimensions": reset_dimensions}
            require(call("test.state") == reset_capture_state,
                    "Capture immediately after reset changed the restored fixture")
            keyboard_round_trip = call("test.guiKeyboardRoundTrip")
            require(keyboard_round_trip["passed"] and keyboard_round_trip["precomposedByKey"]
                    and keyboard_round_trip["keepMappingRegisteredByKey"]
                    and keyboard_round_trip["selectedLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["untouchedObjectPreservedByKey"]
                    and keyboard_round_trip["undoneByKey"]
                    and keyboard_round_trip["precomposeUndoSelectionByKey"]
                    and keyboard_round_trip["precomposeMappingRemovedByUndoKey"]
                    and keyboard_round_trip["precomposeUndoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["redoneByKey"]
                    and keyboard_round_trip["precomposeMappingRestoredByRedoKey"]
                    and keyboard_round_trip["precomposeRedoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["selectedSettingsCopiedByKey"]
                    and keyboard_round_trip["selectedLayersMovedByKey"]
                    and keyboard_round_trip["precomposeSelectionByKey"]
                    and keyboard_round_trip["unpackedKeepByKey"]
                    and keyboard_round_trip["keepMappingRemovedByKey"]
                    and keyboard_round_trip["keepUnpackLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["keepUndoByKey"] and keyboard_round_trip["keepRedoByKey"]
                    and keyboard_round_trip["keepMappingRestoredByUndoKey"]
                    and keyboard_round_trip["keepMappingRemovedByRedoKey"]
                    and keyboard_round_trip["keepUndoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["keepRedoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["keepChildObjectPreservedByUnpackKey"]
                    and keyboard_round_trip["keepChildEmptyByUnpackKey"]
                    and keyboard_round_trip["keepChildObjectPreservedByUndoKey"]
                    and keyboard_round_trip["keepChildLayersRestoredByUndoKey"]
                    and keyboard_round_trip["keepChildObjectPreservedByRedoKey"]
                    and keyboard_round_trip["keepChildEmptyByRedoKey"]
                    and keyboard_round_trip["precomposedForDeleteByKey"]
                    and keyboard_round_trip["deleteMappingRegisteredByKey"]
                    and keyboard_round_trip["deletePrecomposeLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["unpackedDeleteByKey"]
                    and keyboard_round_trip["deleteMappingRemovedByKey"]
                    and keyboard_round_trip["deleteChildRemovedByKey"]
                    and keyboard_round_trip["deleteUnpackLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["deleteUndoByKey"]
                    and keyboard_round_trip["deleteMappingRestoredByUndoKey"]
                    and keyboard_round_trip["deleteUndoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["deleteChildObjectRestoredByUndoKey"]
                    and keyboard_round_trip["deleteUndoSourceLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["deleteRedoByKey"]
                    and keyboard_round_trip["deleteMappingRemovedByRedoKey"]
                    and keyboard_round_trip["deleteChildRemovedByRedoKey"]
                    and keyboard_round_trip["deleteRedoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["allMovedByKey"]
                    and keyboard_round_trip["allMappingRegisteredByKey"]
                    and keyboard_round_trip["allLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["allSettingsCopiedByKey"]
                    and keyboard_round_trip["allUndoByKey"]
                    and keyboard_round_trip["allUndoSelectionByKey"]
                    and keyboard_round_trip["allMappingRemovedByUndoKey"]
                    and keyboard_round_trip["allUndoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["allRedoByKey"]
                    and keyboard_round_trip["allMappingRestoredByRedoKey"]
                    and keyboard_round_trip["allRedoLayerObjectsPreservedByKey"]
                    and keyboard_round_trip["resetByKey"]
                    and keyboard_round_trip["resetFixtureMatchesByKey"]
                    and keyboard_round_trip["resetLocalePreservedByKey"]
                    and keyboard_round_trip["fixtureRecreatedByKey"]
                    and keyboard_round_trip["resetSelectionByKey"],
                    "GUI A/P/K/D/R/Z/Y keyboard routing did not round-trip precompose, unprecompose, and reset")
            animated_keyboard_round_trip = call("test.guiAnimatedKeyRoundTrip")
            require(animated_keyboard_round_trip["passed"]
                    and animated_keyboard_round_trip["animationMovedByKey"]
                    and animated_keyboard_round_trip["movedLayerIdentityPreserved"]
                    and animated_keyboard_round_trip["wrapperNegativeTimingPreservedByKey"]
                    and animated_keyboard_round_trip["managerRegisteredByKey"]
                    and animated_keyboard_round_trip["wrapperSelectedByKey"]
                    and animated_keyboard_round_trip["animationEnabledByKey"]
                    and animated_keyboard_round_trip["negativeTimingNormalizedByKey"]
                    and animated_keyboard_round_trip["animationSamplePreservedByKey"]
                    and animated_keyboard_round_trip["animationRestoredByUndo"]
                    and animated_keyboard_round_trip["undoLayerIdentityPreserved"]
                    and animated_keyboard_round_trip["wrapperRemovedByUndo"]
                    and animated_keyboard_round_trip["managerUnregisteredByUndo"]
                    and animated_keyboard_round_trip["sourceSelectedByUndoKey"]
                    and animated_keyboard_round_trip["negativeTimingRestoredByUndo"]
                    and animated_keyboard_round_trip["animationEnabledAfterUndo"]
                    and animated_keyboard_round_trip["undoKeysPreserved"]
                    and animated_keyboard_round_trip["undoSamplePreserved"]
                    and animated_keyboard_round_trip["animationRestoredByRedo"]
                    and animated_keyboard_round_trip["childCompositionIdentityPreservedByRedo"]
                    and animated_keyboard_round_trip["redoLayerIdentityPreserved"]
                    and animated_keyboard_round_trip["wrapperNegativeTimingRestoredByRedo"]
                    and animated_keyboard_round_trip["wrapperIdentityRestoredByRedo"]
                    and animated_keyboard_round_trip["managerRegisteredByRedo"]
                    and animated_keyboard_round_trip["wrapperSelectedByRedoKey"]
                    and animated_keyboard_round_trip["negativeTimingRestoredByRedo"]
                    and animated_keyboard_round_trip["animationEnabledAfterRedo"]
                    and animated_keyboard_round_trip["redoSamplePreserved"]
                    and animated_keyboard_round_trip["animationRestoredByKeyboardUnpack"]
                    and animated_keyboard_round_trip["unpackLayerIdentityPreserved"]
                    and animated_keyboard_round_trip["childObjectPreservedByUnpackKey"]
                    and animated_keyboard_round_trip["childEmptyByUnpackKey"]
                    and animated_keyboard_round_trip["wrapperRemovedByUnpack"]
                    and animated_keyboard_round_trip["managerUnregisteredByUnpack"]
                    and animated_keyboard_round_trip["sourceSelectedByUnpackKey"]
                    and animated_keyboard_round_trip["negativeTimingRestoredByUnpack"]
                    and animated_keyboard_round_trip["animationEnabledAfterUnpack"]
                    and animated_keyboard_round_trip["unpackSamplePreserved"]
                    and animated_keyboard_round_trip["animationRestoredByUnpackUndo"]
                    and animated_keyboard_round_trip["unpackUndoIdentityPreserved"]
                    and animated_keyboard_round_trip["childObjectPreservedByUnpackUndoKey"]
                    and animated_keyboard_round_trip["childLayerRestoredByUnpackUndoKey"]
                    and animated_keyboard_round_trip["wrapperNegativeTimingRestoredByUnpackUndo"]
                    and animated_keyboard_round_trip["wrapperIdentityRestoredByUnpackUndo"]
                    and animated_keyboard_round_trip["managerRegisteredByUnpackUndo"]
                    and animated_keyboard_round_trip["wrapperSelectedByUnpackUndoKey"]
                    and animated_keyboard_round_trip["negativeTimingRestoredByUnpackUndo"]
                    and animated_keyboard_round_trip["animationEnabledAfterUnpackUndo"]
                    and animated_keyboard_round_trip["unpackUndoSamplePreserved"]
                    and animated_keyboard_round_trip["animationRestoredByUnpackRedo"]
                    and animated_keyboard_round_trip["unpackRedoIdentityPreserved"]
                    and animated_keyboard_round_trip["childObjectPreservedByUnpackRedoKey"]
                    and animated_keyboard_round_trip["childEmptyByUnpackRedoKey"]
                    and animated_keyboard_round_trip["animatedDeletePrecomposeByKey"]
                    and animated_keyboard_round_trip["animatedDeleteUnpackByKey"]
                    and animated_keyboard_round_trip["animatedDeleteChildRemovedByKey"]
                    and animated_keyboard_round_trip["animatedDeleteUndoRestoredByKey"]
                    and animated_keyboard_round_trip["animatedDeleteRedoRestoredByKey"]
                    and animated_keyboard_round_trip["wrapperRemovedByUnpackRedo"]
                    and animated_keyboard_round_trip["managerUnregisteredByUnpackRedo"]
                    and animated_keyboard_round_trip["sourceSelectedByUnpackRedoKey"]
                    and animated_keyboard_round_trip["negativeTimingRestoredByUnpackRedo"]
                    and animated_keyboard_round_trip["animationEnabledAfterUnpackRedo"]
                    and animated_keyboard_round_trip["unpackRedoSamplePreserved"],
                    "GUI P/Z/Y/K/D keyboard routing did not preserve animated data, child identity, and layer identity across unpack undo/redo")
            call("test.reset")
            initial = call("test.state")
            original = initial["layers"]
            require(set(layer_map(initial)) == {"A", "B", "Untouched"}, "Fixture missing layers")
            expected_ranges = {"A": (40, 200, 7), "B": (70, 160, 9),
                               "Untouched": (25, 220, 3)}
            for name, (in_frame, out_frame, start_frame) in expected_ranges.items():
                layer = layer_map(initial)[name]
                require((layer["inFrame"], layer["outFrame"], layer["startFrame"]) ==
                        (in_frame, out_frame, start_frame), f"Unexpected fixture timing for {name}")
            stale_selection = call("test.precomposeIgnoresStaleSelectionId")
            require(stale_selection["passed"] and stale_selection["staleIdWasNonNil"]
                    and stale_selection["movedOnlyExistingSelection"]
                    and stale_selection["duplicateSelectionIdsIgnored"]
                    and stale_selection["managerRegistrationAfterPrecompose"]
                    and stale_selection["undoRestoredOriginal"]
                    and stale_selection["undoClearedManagerRegistration"]
                    and stale_selection["redoRestoredOnlyExistingSelection"]
                    and stale_selection["redoManagerRegistrationRestored"],
                    "Precompose did not ignore a stale non-nil selection ID consistently")
            call("test.reset")
            cycle = call("test.cycleRejected")
            require(cycle["passed"] and cycle["cycleRejected"] and cycle["stateUnchanged"]
                    and cycle["selectionPreserved"] and cycle["fixtureCleaned"],
                    "Cycle precompose was not rejected without changing project or selection state")
            call("test.reset")
            invalid_requests = call("test.invalidRequestsAtomic")
            require(invalid_requests["passed"] and invalid_requests["blankNameRejected"]
                    and invalid_requests["blankNameAtomic"]
                    and invalid_requests["invalidSelectionRejected"]
                    and invalid_requests["invalidSelectionAtomic"]
                    and invalid_requests["staleSelectionRejected"]
                    and invalid_requests["staleSelectionAtomic"]
                    and invalid_requests["emptySelectionRejected"]
                    and invalid_requests["emptySelectionAtomic"]
                    and invalid_requests["emptyPrecompIdRejected"]
                    and invalid_requests["emptyPrecompIdAtomic"]
                    and invalid_requests["ordinaryLayerRejected"]
                    and invalid_requests["ordinaryLayerAtomic"]
                    and invalid_requests["selectionPreserved"]
                    and invalid_requests["redoHistoryPreserved"]
                    and invalid_requests["preservedRedoExecuted"],
                    "Invalid precompose/unprecompose requests changed project or selection state")
            call("test.reset")
            undo_budget = call("test.undoBudgetRejectsStructureChanges")
            require(undo_budget["passed"] and undo_budget["precomposeRejected"]
                    and undo_budget["precomposeBudgetAtomic"] and undo_budget["unprecomposeRejected"]
                    and undo_budget["unprecomposeBudgetAtomic"],
                    "Undo-budget rejection changed precompose/unprecompose structure or mappings")
            call("test.reset")
            redo_branch = call("test.redoBranchDiscarded")
            require(redo_branch["passed"] and redo_branch["redoBranchCreated"]
                    and redo_branch["abandonedChildRemoved"]
                    and redo_branch["abandonedRedoDiscarded"]
                    and redo_branch["replacementChildCountCorrect"]
                    and redo_branch["replacementMappingRestored"]
                    and redo_branch["replacementUndoRestored"]
                    and redo_branch["replacementMappingAfterRedo"]
                    and redo_branch["replacementRedoRestored"],
                    "A new precompose did not discard and replace the old redo branch")
            call("test.reset")
            parent_focused = call("test.keepParentFocused")
            require(parent_focused["passed"] and parent_focused["parentStayedFocused"]
                    and parent_focused["childWasCreated"] and parent_focused["undoStayedFocused"]
                    and parent_focused["redoStayedFocused"],
                    "Precompose with openNewComposition=false changed focus or selection")
            call("test.reset")
            component_round_trip = call("test.componentDescriptorsSurviveUnprecompose")
            require(component_round_trip["passed"] and component_round_trip["authored"]
                    and component_round_trip["childDescriptorPreserved"]
                    and component_round_trip["unpackDescriptorPreserved"]
                    and component_round_trip["undoDescriptorPreserved"]
                    and component_round_trip["redoDescriptorPreserved"],
                    "Layer component descriptor graph changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            matte_round_trip = call("test.matteReferencesSurviveUnprecompose")
            require(matte_round_trip["passed"] and matte_round_trip["authored"]
                    and matte_round_trip["childMattesPreserved"]
                    and matte_round_trip["unpackMattesPreserved"]
                    and matte_round_trip["undoMattesPreserved"]
                    and matte_round_trip["redoMattesPreserved"],
                    "Matte reference identity, order, or settings changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            mask_round_trip = call("test.maskSurvivesUnprecompose")
            require(mask_round_trip["passed"] and mask_round_trip["authored"]
                    and mask_round_trip["childMaskPreserved"]
                    and mask_round_trip["unpackMaskPreserved"]
                    and mask_round_trip["undoMaskPreserved"]
                    and mask_round_trip["redoMaskPreserved"],
                    "Layer mask settings or path geometry changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            effect_round_trip = call("test.effectSurvivesUnprecompose")
            require(effect_round_trip["passed"] and effect_round_trip["authored"]
                    and effect_round_trip["childEffectPreserved"]
                    and effect_round_trip["unpackEffectPreserved"]
                    and effect_round_trip["undoEffectPreserved"]
                    and effect_round_trip["redoEffectPreserved"]
                    and abs(effect_round_trip["brightness"] - 0.375) < 1e-6,
                    "Layer effect identity or parameters changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            composition_effect = call("test.compositionEffectStaysOnParent")
            require(composition_effect["passed"] and composition_effect["authored"]
                    and composition_effect["scopedToParent"]
                    and composition_effect["unpackPreservedScope"]
                    and composition_effect["undoPreservedScope"]
                    and composition_effect["redoPreservedScope"],
                    "Composition-scoped effect moved to the child or changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            child_composition_effect = call("test.childCompositionEffectStaysWithChild")
            require(child_composition_effect["passed"] and child_composition_effect["saveSucceeded"]
                    and child_composition_effect["mappingClearedBeforeReload"]
                    and child_composition_effect["loaded"]
                    and child_composition_effect["effectRestoredOnChild"]
                    and child_composition_effect["unpackKeptChildEffect"]
                    and child_composition_effect["undoKeptChildEffect"]
                    and child_composition_effect["redoKeptChildEffect"],
                    "Child composition effect changed scope across save/reload or unprecompose history")
            call("test.reset")
            opened_child = call("test.openChildComposition")
            require(opened_child["passed"] and opened_child["childOpened"]
                    and opened_child["childSelectionCorrect"]
                    and opened_child["rootWrapperCreated"]
                    and opened_child["wrongContextUnpackRejected"]
                    and opened_child["wrongContextSelectionPreserved"]
                    and opened_child["wrongContextManagerMappingPreserved"]
                    and opened_child["wrongContextReverseMappingPreserved"]
                    and opened_child["wrongContextLayerSnapshotsPreserved"]
                    and opened_child["wrongContextPublishedStatePreserved"]
                    and opened_child["wrongContextStatePreserved"]
                    and opened_child["undoReturnedToRoot"]
                    and opened_child["redoKeptRootFocused"] and opened_child["redoRestoredChild"],
                    "Open-child precompose navigation or undo/redo focus changed")
            call("test.reset")
            gui_opened_child = call("test.guiOpenChildCompositionRoundTrip")
            require(gui_opened_child["passed"] and gui_opened_child["guiKeyRouted"]
                    and gui_opened_child["returnedToRoot"]
                    and gui_opened_child["rootWrapperSelectedAfterReturn"]
                    and gui_opened_child["childOpened"]
                    and gui_opened_child["childLayerOrderPreservedOnOpen"]
                    and gui_opened_child["childLayerObjectsPreservedOnOpen"]
                    and gui_opened_child["childTimingNormalizedOnOpen"]
                    and gui_opened_child["wrapperTimingPreservedOnOpen"]
                    and gui_opened_child["childSettingsCopiedOnOpen"]
                    and gui_opened_child["childRangesNormalizedOnOpen"]
                    and gui_opened_child["childSelectionCorrect"]
                    and gui_opened_child["wrongContextUnpackRejected"]
                    and gui_opened_child["wrongContextSelectionPreserved"]
                    and gui_opened_child["wrongContextManagerMappingPreserved"]
                    and gui_opened_child["wrongContextReverseMappingPreserved"]
                    and gui_opened_child["wrongContextLayerSnapshotsPreserved"]
                    and gui_opened_child["wrongContextSettingsPreserved"]
                    and gui_opened_child["wrongContextCompositionCountPreserved"]
                    and gui_opened_child["wrongContextPublishedStatePreserved"]
                    and gui_opened_child["wrongContextStatePreserved"]
                    and gui_opened_child["undoReturnedToRoot"]
                    and gui_opened_child["undoLayerObjectsReturnedToRoot"]
                    and gui_opened_child["undoSourceTimingRestored"]
                    and gui_opened_child["undoLayerOrderRestoredToRoot"]
                    and gui_opened_child["undoRootLayerSnapshotRestored"]
                    and gui_opened_child["undoManagerMappingsCleared"]
                    and gui_opened_child["undoCompositionCountRestored"]
                    and gui_opened_child["redoKeptRootFocused"]
                    and gui_opened_child["redoRestoredChild"]
                    and gui_opened_child["redoChildCompositionIdentityPreserved"]
                    and gui_opened_child["redoChildLayerObjectsPreserved"]
                    and gui_opened_child["redoChildLayerOrderPreserved"]
                    and gui_opened_child["redoChildTimingNormalized"]
                    and gui_opened_child["redoChildSettingsPreserved"]
                    and gui_opened_child["redoChildRangesNormalized"]
                    and gui_opened_child["redoReverseMappingRestored"]
                    and gui_opened_child["redoForwardMappingRestored"]
                    and gui_opened_child["redoCompositionCountRestored"]
                    and gui_opened_child["redoWrapperIdentityPreserved"]
                    and gui_opened_child["redoWrapperTimingPreserved"]
                    and gui_opened_child["redoRootContainsOnlyWrapperAndUntouched"],
                    "GUI O key did not preserve open-child selection and undo/redo focus behavior")
            call("test.reset")
            negative_timing = call("test.negativeTimingRoundTrip")
            require(negative_timing["passed"] and negative_timing["childTimingNormalized"]
                    and negative_timing["childKeysShifted"]
                    and negative_timing["childSamplesPreserved"]
                    and negative_timing["unpackRestored"]
                    and negative_timing["unpackSamplesPreserved"]
                    and negative_timing["undoRestored"]
                    and negative_timing["undoSamplesPreserved"]
                    and negative_timing["redoRestored"]
                    and negative_timing["redoSamplesPreserved"],
                    "Negative frame timing or evaluated animation changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            rational_rate = call("test.rationalFrameRatePreserved")
            require(rational_rate["passed"] and rational_rate["selectedRatePreserved"]
                    and rational_rate["selectedAnimationPreserved"]
                    and rational_rate["roundTripRatePreserved"]
                    and rational_rate["roundTripAnimationPreserved"]
                    and rational_rate["selectedUnpackedAnimationPreserved"]
                    and rational_rate["parentRateAfterSelected"] and rational_rate["allRatePreserved"]
                    and rational_rate["allAnimationPreserved"]
                    and rational_rate["allUnpacked"] and rational_rate["allUnpackedAnimationPreserved"]
                    and rational_rate["undoRatePreserved"] and rational_rate["undoAnimationPreserved"]
                    and rational_rate["redoRestored"] and rational_rate["redoAnimationPreserved"],
                    "Exact rational frame rate changed across selected/all precompose or undo/redo")
            call("test.reset")
            workspace_duration = call("test.matchWorkspaceDuration")
            require(workspace_duration["passed"]
                    and workspace_duration["childOpened"]
                    and workspace_duration["childDurationMatched"]
                    and workspace_duration["childWorkAreaNormalized"]
                    and workspace_duration["childSettingsCopied"]
                    and workspace_duration["parentWorkAreaPreserved"]
                    and workspace_duration["parentFrameRangePreserved"]
                    and workspace_duration["wrapperTimingPreserved"]
                    and workspace_duration["undoRestoredParent"]
                    and workspace_duration["redoRestoredWorkspaceChild"],
                    "Precompose did not match workspace duration while preserving parent work area and wrapper timing")
            call("test.reset")
            empty_workspace = call("test.emptyWorkspaceDurationFallsBack")
            require(empty_workspace["passed"] and empty_workspace["emptyWorkspaceInputDuration"] == 0
                    and empty_workspace["emptyWorkspaceNormalized"]
                    and empty_workspace["effectiveParentWorkAreaStart"] == 0
                    and empty_workspace["parentWorkAreaPreserved"]
                    and empty_workspace["fallbackApplied"]
                    and empty_workspace["wrapperTimingPreserved"]
                    and empty_workspace["undoRestoredParent"]
                    and empty_workspace["redoRestoredFallbackChild"],
                    "Empty workspace range did not fall back to the parent duration consistently")
            call("test.reset")
            duration_propagation = call("test.childDurationPropagates")
            require(duration_propagation["passed"] and duration_propagation["shrinkFollowed"]
                    and duration_propagation["expansionFollowed"]
                    and duration_propagation["workAreaClippedOnShrink"]
                    and duration_propagation["workAreaRetainedOnExpansion"]
                    and duration_propagation["activeChildStayed"]
                    and duration_propagation["parentReferencePreserved"],
                    "Active child duration shrink/expansion did not propagate while keeping child focus")
            call("test.reset")
            parent_duration = call("test.parentDurationPropagatesToChild")
            require(parent_duration["passed"] and parent_duration["firstShrinkFollowed"]
                    and parent_duration["firstWorkAreaFollowed"]
                    and parent_duration["secondShrinkFollowed"]
                    and parent_duration["secondWorkAreaFollowed"]
                    and parent_duration["expansionFollowed"]
                    and parent_duration["expansionWorkAreaRetained"],
                    "Parent duration shrink/expansion did not propagate to its child composition")
            call("test.reset")
            shared_duration = call("test.sharedChildDurationPropagates")
            require(shared_duration["passed"] and shared_duration["bothParentsRegistered"]
                    and shared_duration["shrinkFollowed"] and shared_duration["expansionFollowed"]
                    and shared_duration["workAreaClippedOnShrink"]
                    and shared_duration["workAreaRetainedOnExpansion"]
                    and shared_duration["saveReloadRestored"]
                    and shared_duration["saveReloadWorkAreaRestored"]
                    and shared_duration["firstParentDeleted"]
                    and shared_duration["sharedChildSurvivedDelete"]
                    and shared_duration["deleteUndoRestored"]
                    and shared_duration["deleteRedoRestored"]
                    and shared_duration["firstParentUnpacked"]
                    and shared_duration["otherParentReferencePreserved"]
                    and shared_duration["firstUnpackUndoRestored"]
                    and shared_duration["firstUnpackRedoRestored"],
                    "Shared child duration propagation or its two-parent save/reload restoration failed")
            call("test.reset")
            same_parent_duration = call("test.sharedParentDurationPropagates")
            require(same_parent_duration["passed"] and same_parent_duration["sameParentRegistered"]
                    and same_parent_duration["shrinkFollowed"]
                    and same_parent_duration["workAreaClippedOnShrink"]
                    and same_parent_duration["shrinkWorkAreaPreserved"]
                    and same_parent_duration["shrinkPreservedReferences"]
                    and same_parent_duration["expansionFollowed"]
                    and same_parent_duration["workAreaRetainedOnExpansion"]
                    and same_parent_duration["expansionWorkAreaPreserved"]
                    and same_parent_duration["expansionPreservedReferences"],
                    "Shared child duration shrink/expansion did not update both wrappers in one parent")
            call("test.reset")
            transitive_cycle = call("test.transitiveCycleRejected")
            require(transitive_cycle["passed"] and transitive_cycle["transitiveCycleRejected"]
                    and transitive_cycle["threeLevelCycleRejected"]
                    and transitive_cycle["stateUnchanged"] and transitive_cycle["fixtureCleaned"],
                    "Transitive cycle precompose was not rejected atomically")
            call("test.reset")
            exposed_override = call("test.exposedOverrideMaterialized")
            require(exposed_override["passed"] and exposed_override["bindingAdded"]
                    and exposed_override["overrideAccepted"]
                    and exposed_override["overrideWasDeferred"]
                    and exposed_override["unpackMaterialized"]
                    and exposed_override["undoPreservedMaterializedValue"]
                    and exposed_override["redoPreservedMaterializedValue"],
                    "Master Property override was lost while unprecomposing or traversing undo/redo")
            call("test.reset")
            nested_override = call("test.nestedExposedOverrideMaterialized")
            require(nested_override["passed"] and nested_override["bindingAdded"]
                    and nested_override["overrideDeferred"]
                    and nested_override["outerOverrideDeferred"]
                    and nested_override["nestedOverridePreserved"]
                    and nested_override["saveReloadPreserved"]
                    and nested_override["outerUnpacked"]
                    and nested_override["innerOverridePreserved"]
                    and nested_override["overrideMaterialized"]
                    and nested_override["undoPreservedOverride"]
                    and nested_override["redoPreservedOverride"],
                    "Nested Master Property override changed across precompose/unpack/undo/redo")
            call("test.reset")
            layer_id_collision = call("test.unpackLayerIdCollisionAtomic")
            require(layer_id_collision["passed"] and layer_id_collision["collisionRejected"]
                    and layer_id_collision["bindingAdded"]
                    and layer_id_collision["overrideDeferredBeforeFailure"]
                    and layer_id_collision["overrideRemainsDeferred"]
                    and layer_id_collision["opacityAnimationPreserved"]
                    and layer_id_collision["parentStatePreserved"]
                    and layer_id_collision["compositionStatePreserved"]
                    and layer_id_collision["wrapperPreserved"]
                    and layer_id_collision["historyPreserved"],
                    "Unprecompose partially mutated state after a restored child layer ID collided")
            call("test.reset")
            duplicate_child_ids = call("test.unpackDuplicateChildLayerIdsAtomic")
            require(duplicate_child_ids["passed"] and duplicate_child_ids["duplicateIdsRejected"]
                    and duplicate_child_ids["parentStatePreserved"]
                    and duplicate_child_ids["childStatePreserved"]
                    and duplicate_child_ids["wrapperPreserved"]
                    and duplicate_child_ids["historyPreserved"],
                    "Unprecompose partially mutated state after child layer IDs duplicated")
            call("test.reset")
            nil_child_id = call("test.unpackNilChildLayerIdAtomic")
            require(nil_child_id["passed"] and nil_child_id["nilIdRejected"]
                    and nil_child_id["parentStatePreserved"]
                    and nil_child_id["childStatePreserved"]
                    and nil_child_id["wrapperPreserved"]
                    and nil_child_id["historyPreserved"],
                    "Unprecompose partially mutated state after a child layer ID became nil")
            call("test.reset")
            missing_child = call("test.missingChildReferenceAtomic")
            require(missing_child["passed"] and missing_child["unwrapRejected"]
                    and missing_child["parentStatePreserved"]
                    and missing_child["compositionPreserved"]
                    and missing_child["wrapperPreserved"]
                    and missing_child["undoHistoryPreserved"]
                    and missing_child["outcomeCleared"]
                    and missing_child["fixtureRestored"]
                    and missing_child["danglingReferenceRejected"]
                    and missing_child["danglingManagerMappingPreserved"]
                    and missing_child["danglingOutcomeCleared"]
                    and missing_child["danglingReferenceStatePreserved"]
                    and missing_child["danglingFixtureRestored"]
                    and missing_child["selfReferenceRejected"]
                    and missing_child["selfReferenceManagerMappingPreserved"]
                    and missing_child["selfReferenceOutcomeCleared"]
                    and missing_child["selfReferenceStatePreserved"]
                    and missing_child["selfReferenceFixtureRestored"],
                    "Unprecompose mutated a wrapper with a missing child reference")
            call("test.reset")
            missing_manager = call("test.unprecomposeWithMissingManagerMapping")
            require(missing_manager["passed"] and missing_manager["fixtureReady"]
                    and missing_manager["mappingCleared"]
                    and missing_manager["unpackedFromLayerReference"]
                    and missing_manager["undoRestoredManagerMapping"]
                    and missing_manager["redoRemovedMappingAgain"],
                    "Unprecompose did not restore layers and Manager mapping across missing-map undo/redo")
            call("test.reset")
            empty_child = call("test.emptyChildUnwrapRejected")
            require(empty_child["passed"] and empty_child["childEmptied"]
                    and empty_child["unwrapRejected"] and empty_child["wrapperPreserved"]
                    and empty_child["childPreserved"] and empty_child["parentStatePreserved"]
                    and empty_child["undoHistoryPreserved"] and empty_child["outcomeCleared"],
                    "Unprecompose did not preserve an empty child and wrapper atomically")
            call("test.reset")
            empty_move_all = call("test.emptyMoveAllRejectedAtomic")
            require(empty_move_all["passed"] and empty_move_all["emptyFixtureReady"]
                    and empty_move_all["rejected"] and empty_move_all["rejectionAtomic"]
                    and empty_move_all["fixtureRestored"],
                    "MoveAllAttributes accepted an empty composition or changed state on rejection")
            call("test.reset")
            delete_redo = call("test.unpackDeleteRedo")
            require(delete_redo["passed"] and delete_redo["deletedAfterUnpack"]
                    and delete_redo["childDataCaptured"] and delete_redo["restoredByUndo"]
                    and delete_redo["sameChildObjectsRestored"] and delete_redo["deletedByRedo"]
                    and not delete_redo["state"]["lastChildExists"],
                    "Delete-unpack Undo/Redo did not restore then remove the same child composition")
            call("test.reset")
            single_layer = call("test.singleLayerRoundTrip")
            require(single_layer["passed"] and single_layer["wrappedCorrectly"]
                    and single_layer["undoRestored"] and single_layer["redoRestored"],
                    "Single-layer precompose changed layer identity, order, or registration")
            call("test.reset")
            layer_notes = call("test.layerNotesRoundTrip")
            require(layer_notes["passed"] and layer_notes["notesMoved"]
                    and layer_notes["notesUnpacked"] and layer_notes["undoRestored"]
                    and layer_notes["redoRestored"],
                    "Layer notes changed across precompose/unprecompose/undo/redo")
            final_notes = layer_map(layer_notes["state"])
            require(final_notes["A"]["layerNote"] == "A memo / 子"
                    and final_notes["B"]["layerNote"] == "B memo\nline 2"
                    and final_notes["Untouched"]["layerNote"] == "Keep this note",
                    "MCP state did not expose the final per-layer note values")
            call("test.reset")
            layer_flags = call("test.layerFlagsAndBlendRoundTrip")
            require(layer_flags["passed"] and layer_flags["flagsMoved"]
                    and layer_flags["flagsUnpacked"] and layer_flags["undoRestored"]
                    and layer_flags["redoRestored"],
                    "Layer flags or blend modes changed across precompose/unprecompose/undo/redo")
            final_flags = layer_map(layer_flags["state"])
            require(not final_flags["A"]["visible"] and final_flags["A"]["locked"]
                    and final_flags["A"]["solo"] and final_flags["A"]["blendMode"] == 4
                    and final_flags["B"]["visible"] and not final_flags["B"]["locked"]
                    and not final_flags["B"]["solo"] and final_flags["B"]["blendMode"] == 3
                    and final_flags["Untouched"]["visible"]
                    and final_flags["Untouched"]["blendMode"] == 0,
                    "MCP state did not expose the final layer flags and blend modes")
            call("test.reset")
            shared_child = call("test.sharedChildReferences")
            require(shared_child["passed"] and shared_child["twoReferencesCreated"]
                    and shared_child["saveSucceeded"] and shared_child["mappingsClearedBeforeReload"]
                    and shared_child["loaded"] and shared_child["sharedChildSaveReload"]
                    and shared_child["childDataPreserved"]
                    and shared_child["remainingReferencePreserved"]
                    and shared_child["undoRestoredBoth"]
                    and shared_child["redoChildDataPreserved"]
                    and shared_child["redoPreservedRemaining"] and shared_child["finalChildRemoved"]
                    and shared_child["finalDeleteUndoRestored"] and shared_child["finalDeleteRedoRemoved"],
                    "Shared child reference removal or final child deletion undo/redo changed state")
            call("test.reset")
            shared_overrides = call("test.sharedInstanceOverridesRoundTrip")
            require(shared_overrides["passed"] and shared_overrides["bindingAdded"]
                    and shared_overrides["overridesIsolated"] and shared_overrides["saveSucceeded"]
                    and shared_overrides["mappingsClearedBeforeReload"] and shared_overrides["loaded"]
                    and shared_overrides["saveReloadPreserved"],
                    "Shared wrapper Master Property overrides were not preserved independently")
            call("test.reset")
            external_parent = call("test.externalParentRestored")
            require(external_parent["passed"] and external_parent["externalParentStoredOnWrapper"]
                    and external_parent["externalParentClearedInChild"]
                    and external_parent["movedObjectsPreserved"]
                    and external_parent["bothExternalParentLinksRestored"]
                    and external_parent["unpackPreservedObjects"]
                    and external_parent["unpackRestoredExternalParent"]
                    and external_parent["undoRestoredMetadata"]
                    and external_parent["undoRestoredBothMetadata"]
                    and external_parent["undoPreservedObjects"]
                    and external_parent["redoRestoredBothExternalParents"]
                    and external_parent["redoPreservedObjects"]
                    and external_parent["redoRestoredExternalParent"],
                    "Multiple external parent links were not preserved across precompose/unpack/undo/redo")
            call("test.reset")
            missing_external_parent = call("test.missingExternalParentUnwrapAtomic")
            require(missing_external_parent["passed"] and missing_external_parent["parentAuthored"]
                    and missing_external_parent["metadataStored"] and missing_external_parent["parentDeleted"]
                    and missing_external_parent["saveSucceeded"]
                    and missing_external_parent["mappingClearedBeforeReload"]
                    and missing_external_parent["loaded"]
                    and missing_external_parent["metadataRestoredAfterReload"]
                    and missing_external_parent["unpackKeptParentCleared"]
                    and missing_external_parent["unpackSelectionCorrect"]
                    and missing_external_parent["unpackHistoryUpdated"]
                    and missing_external_parent["undoRestoredMetadata"]
                    and missing_external_parent["undoSelectionCorrect"]
                    and missing_external_parent["undoHistoryUpdated"]
                    and missing_external_parent["redoKeptParentCleared"]
                    and missing_external_parent["redoSelectionCorrect"]
                    and missing_external_parent["redoHistoryUpdated"],
                    "Unprecompose restored a deleted external parent or lost undo metadata")
            call("test.reset")
            child_edits = call("test.childEditsSurviveUnwrap")
            require(child_edits["passed"] and child_edits["editsAuthored"]
                    and child_edits["unpackedValuesPreserved"]
                    and child_edits["undoValuesPreserved"]
                    and child_edits["redoValuesPreserved"],
                    "Transform or opacity edits inside a child composition changed across unprecompose/undo/redo")
            edited_layers = layer_map(child_edits["state"])
            require(edited_layers["A"]["rotation"] == [17, -23, 38]
                    and edited_layers["A"]["scale"][:2] == [1.5, 0.75]
                    and edited_layers["A"]["anchor"] == [11, -6, 2]
                    and edited_layers["B"]["rotation"] == [-31, 12, 91]
                    and edited_layers["B"]["scale"][:2] == [0.625, 1.875]
                    and edited_layers["B"]["anchor"] == [-4, 9, -3],
                    "MCP state did not expose child-edited rotation, scale, and anchor values")
            call("test.reset")
            selection_transitions = call("test.selectionTransitions")
            require(selection_transitions["passed"]
                    and selection_transitions["precomposeSelectedWrapper"]
                    and selection_transitions["unpackSelectedFirstSource"]
                    and selection_transitions["undoSelectedWrapper"]
                    and selection_transitions["redoSelectedFirstSource"]
                    and selection_transitions["selectionObjectPreservedAcrossTransitions"],
                    "Layer selection or active composition changed unexpectedly across precompose/unprecompose/undo/redo")
            call("test.reset")
            external_parent_round_trip = call("test.externalParentSaveReload")
            require(external_parent_round_trip["passed"]
                    and external_parent_round_trip["mappingClearedBeforeReload"]
                    and external_parent_round_trip["metadataPersisted"]
                    and external_parent_round_trip["sourceParentRemainsCleared"]
                    and external_parent_round_trip["runtimeMappingRestored"]
                    and external_parent_round_trip["externalParentRestoredAfterUnpack"],
                    "External-parent metadata did not survive project save/reload and unprecompose")
            call("test.reset")
            internal_parent = call("test.internalParentRestored")
            require(internal_parent["passed"] and internal_parent["parentAuthored"]
                    and internal_parent["childParentPreserved"]
                    and internal_parent["unpackParentPreserved"]
                    and internal_parent["undoParentPreserved"]
                    and internal_parent["redoParentPreserved"],
                    "Internal layer parenting changed across precompose/unpack/undo/redo")
            call("test.reset")
            animated = call("test.animatedTiming")
            require(animated["passed"] and animated["childKeysRetimed"]
                    and animated["rationalTimebaseAuthored"]
                    and animated["rationalChildTimebasePreserved"]
                    and animated["selectedChildSamplesPreserved"]
                    and animated["offsetChildKeysRetimed"]
                    and animated["animationChannelsRetimed"]
                    and animated["animationChannelsUnpacked"]
                    and animated["selectedUnpackSamplesPreserved"]
                    and animated["selectedUndoSamplesPreserved"]
                    and animated["animationChannelsRedoPreserved"]
                    and animated["selectedRedoSamplesPreserved"]
                    and animated["unpackRestoredKeys"] and animated["redoRestoredKeys"]
                    and animated["allModeChildKeysPreserved"]
                    and animated["allModeChildAnimationKeysPreserved"]
                    and animated["allModeChildTimingPreserved"]
                    and animated["allModeRationalTimebasePreserved"]
                    and animated["allModeChildSamplesPreserved"]
                    and animated["allModeUnpackKeysPreserved"]
                    and animated["allModeUnpackAnimationKeysPreserved"]
                    and animated["allModeUnpackTimingPreserved"]
                    and animated["allModeUnpackSamplesPreserved"]
                    and animated["allModeRedoKeysPreserved"]
                    and animated["allModeRedoAnimationKeysPreserved"]
                    and animated["allModeRedoTimingPreserved"]
                    and animated["allModeRedoSamplesPreserved"],
                    "Animated keyframes or metadata changed across selected/all precompose, unpack, or redo")
            call("test.reset")
            initial = call("test.state")
            original = initial["layers"]
            nested = call("test.precompose")
            require_child_settings(nested, "Selected")
            require(len(nested["layers"]) == 2, "Selected precompose must leave Untouched outside")
            child = next(layer for layer in nested["layers"] if "children" in layer)
            require({layer["name"] for layer in child["children"]} == {"A", "B"}, "Wrong child content")
            require((child["inFrame"], child["outFrame"], child["startFrame"]) == (40, 200, 40),
                    "Selected precompose layer does not retain the selected time span")
            nested_children = {layer["name"]: layer for layer in child["children"]}
            require([layer["name"] for layer in child["children"]] == ["A", "B"],
                    "Precompose child order must follow the source composition, not selection order or duplicates")
            require((nested_children["A"]["inFrame"], nested_children["A"]["outFrame"],
                     nested_children["A"]["startFrame"]) == (0, 160, 7),
                    "First child layer was not normalized to the selected range")
            require((nested_children["B"]["inFrame"], nested_children["B"]["outFrame"],
                     nested_children["B"]["startFrame"]) == (30, 120, 9),
                    "Second child layer was not offset relative to the selected range")
            require(layer_map(nested)["Untouched"] == layer_map(initial)["Untouched"], "Unselected layer changed")
            for entry in child["children"]:
                source = layer_map(initial)[entry["name"]]
                require(all(entry[key] == source[key] for key in
                            ("id", "position", "opacity", "startFrame")),
                        "Precompose changed identity or attributes")
            require(call("test.undo")["layers"] == original, "Undo did not restore original layer order/state")
            redone = call("test.redo")
            require(redone["layers"] == nested["layers"], "Redo changed precompose/child identity")
            kept = call("test.unpackKeep")
            require(kept["layers"] == original and kept["lastChildExists"]
                    and kept["canUndo"] and not kept["canRedo"],
                    "Unpack keep contract failed")
            require({name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                     for name, layer in layer_map(kept).items()} == expected_ranges,
                    "Unpack keep did not restore original timing")
            keep_undo = call("test.undo")
            require(without_history(keep_undo) == without_history(redone)
                    and keep_undo["canUndo"] and keep_undo["canRedo"],
                    "Unpack keep undo changed hierarchy or expected history availability")
            keep_redo = call("test.redo")
            require(without_history(keep_redo) == without_history(kept)
                    and keep_redo["canUndo"] and not keep_redo["canRedo"],
                    "Unpack keep redo did not restore the kept-child state")
            keep_second_undo = call("test.undo")
            require(without_history(keep_second_undo) == without_history(redone)
                    and keep_second_undo["canUndo"] and keep_second_undo["canRedo"],
                    "Unpack keep second undo did not restore the precomposed state")
            removed = call("test.unpackDelete")
            require(removed["layers"] == original and not removed["lastChildExists"]
                    and removed["canUndo"] and not removed["canRedo"],
                    "Unpack delete failed")
            require({name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                     for name, layer in layer_map(removed).items()} == expected_ranges,
                    "Unpack delete did not restore original timing")
            restored = call("test.undo")
            require(restored["lastChildExists"] and restored["layers"] == nested["layers"]
                    and restored["canUndo"] and restored["canRedo"], "Deleted child undo failed")
            delete_redo = call("test.redo")
            require(without_history(delete_redo) == without_history(removed)
                    and delete_redo["canUndo"] and not delete_redo["canRedo"],
                    "Unpack delete redo did not restore the removed-child state")
            delete_second_undo = call("test.undo")
            require(without_history(delete_second_undo) == without_history(restored)
                    and delete_second_undo["canUndo"] and delete_second_undo["canRedo"],
                    "Unpack delete second undo did not restore the same child state")
            call("test.reset")
            all_initial = call("test.state")
            all_nested = call("test.precomposeAll")
            require_child_settings(all_nested, "Move all")
            require(all_nested["emptySelectionSubmitted"] and len(all_nested["layers"]) == 1
                    and len(all_nested["layers"][0]["children"]) == 3,
                    "Move all with an empty selection must include every layer")
            all_layer = all_nested["layers"][0]
            require([layer["name"] for layer in all_layer["children"]] ==
                    [layer["name"] for layer in all_initial["layers"]],
                    "Move all did not preserve the original source-layer order")
            require(all_layer["children"] == all_initial["layers"],
                    "Move all changed observable source-layer attributes or timing")
            wrapper_id = all_layer["id"]
            all_selection = all_nested["selection"]
            require(all_selection["activeCompositionId"] == all_nested["compositionId"]
                    and all_selection["currentLayerId"] == wrapper_id
                    and all_selection["selectedLayerIds"] == [wrapper_id],
                    "Move all did not keep the parent active with its wrapper selected")
            require((all_layer["inFrame"], all_layer["startFrame"]) == (0, 0),
                    "Move all precompose layer must start at composition time zero")
            for name, (in_frame, out_frame, start_frame) in expected_ranges.items():
                child_layer = next(layer for layer in all_layer["children"] if layer["name"] == name)
                require((child_layer["inFrame"], child_layer["outFrame"], child_layer["startFrame"]) ==
                        (in_frame, out_frame, start_frame),
                        f"Move all changed {name} timing")
            all_undo = call("test.undo")
            require(all_undo["layers"] == all_initial["layers"]
                    and all_undo["selection"] == all_initial["selection"]
                    and not all_undo["lastChildExists"]
                    and not all_undo["canUndo"] and all_undo["canRedo"],
                    "Move all undo did not restore the original layers and remove the child")
            all_redo = call("test.redo")
            all_result_fields = {"emptySelectionSubmitted", "childSettingsCopied", "childResolution",
                                 "childFrameRate", "childFrameRangeDuration", "parentFrameRangeDuration",
                                 "childBackgroundColor"}
            expected_redo = {key: value for key, value in all_nested.items()
                             if key not in all_result_fields}
            require(without_history(all_redo) == without_history(expected_redo)
                    and all_redo["canUndo"] and not all_redo["canRedo"],
                    "Move all redo did not restore the complete precomposed state")
            round_trip = call("test.saveReload")
            require(round_trip["passed"] and round_trip["parentChildLinkPreserved"]
                    and round_trip["layerStatePreserved"] and round_trip["mappingClearedBeforeReload"]
                    and round_trip["runtimeNestingRestored"]
                    and round_trip["masterPropertyDefinitionPreserved"]
                    and round_trip["masterPropertyOverridePreserved"]
                    and round_trip["overrideStillDeferred"]
                    and round_trip["layerMetadataPreserved"]
                    and round_trip["transformKeyframesPreserved"]
                    and round_trip["animationSamplesPreserved"]
                    and round_trip["layerEffectPersisted"]
                    and round_trip["layerMaskPersisted"]
                    and round_trip["layerMattePersisted"]
                    and round_trip["componentDescriptorsPersisted"]
                    and round_trip["compositionEffectPersistedOnParent"]
                    and round_trip["compositionEffectScopeAfterUnpack"]
                    and round_trip["compositionEffectScopeAfterUndoRedo"]
                    and round_trip["auxiliaryStateUnpackedAfterReload"]
                    and round_trip["opacityOverrideMaterializedAsKeyframe"]
                    and round_trip["unpackUndoRestoredChild"]
                    and round_trip["unpackRedoRestoredParent"],
                    "Precompose save/reload changed layer metadata, effects, masks, mattes, component descriptors, exposed properties, or runtime nesting")
            call("test.reset")
            hierarchy = call("test.nestedHierarchy")
            require(hierarchy["passed"] and hierarchy["hierarchyPreserved"]
                    and hierarchy["timeRoundTripPreserved"],
                    "Nested hierarchy or coordinate conversion did not round-trip")
            require(hierarchy["hierarchy"] == [hierarchy["state"]["compositionId"],
                    hierarchy["state"]["layers"][0]["childId"],
                    hierarchy["state"]["layers"][0]["children"][0]["childId"]],
                    "Nested hierarchy IDs do not match the visible parent-child tree")
            grandchild = hierarchy["state"]["layers"][0]["children"][0]
            require({layer["name"] for layer in grandchild["children"]} == {"A", "B"},
                    "Grandchild composition content changed")
            call("test.reset")
            nested_round_trip = call("test.nestedSaveReload")
            require(nested_round_trip["passed"] and nested_round_trip["allMappingsClearedBeforeReload"]
                    and nested_round_trip["hierarchyDataPreserved"]
                    and nested_round_trip["runtimeHierarchyRestored"]
                    and nested_round_trip["nestedUnpackedAfterReload"]
                    and nested_round_trip["nestedUnpackUndoRestored"]
                    and nested_round_trip["nestedUnpackRedoRestored"]
                    and nested_round_trip["nestedDurationShrinkPropagated"]
                    and nested_round_trip["nestedWorkAreaClippedOnShrink"]
                    and nested_round_trip["nestedDurationExpansionPropagated"]
                    and nested_round_trip["nestedWorkAreaRetainedOnExpansion"]
                    and nested_round_trip["timeConversionRestored"]
                    and nested_round_trip["timeInRoot"] == 45.0,
                    "Nested project save/reload did not restore both runtime nesting levels")
            call("test.reset")
            nested_external_parent = call("test.nestedExternalParentRestored")
            require(nested_external_parent["passed"]
                    and nested_external_parent["innerMetadataAuthored"]
                    and nested_external_parent["nestedMetadataPreserved"]
                    and nested_external_parent["outerUnpacked"]
                    and nested_external_parent["innerMappingRestored"]
                    and nested_external_parent["distinctExternalParents"]
                    and nested_external_parent["externalParentARestored"]
                    and nested_external_parent["externalParentBRestored"]
                    and nested_external_parent["externalParentRestored"]
                    and nested_external_parent["undoRestoredMetadata"]
                    and nested_external_parent["redoRestoredParent"],
                    "External-parent restore metadata changed across nested precompose/unpack/undo/redo")
            call("test.reset")
            reparented = call("test.reparentExistingPrecompose")
            require(reparented["passed"] and reparented["parentReferenceUpdated"]
                    and reparented["hierarchyUpdated"] and reparented["undoRestoredParent"]
                    and reparented["redoRestoredParent"] and reparented["saveSucceeded"]
                    and reparented["mappingsClearedBeforeReload"] and reparented["loaded"]
                    and reparented["reparentSaveReloadRestored"]
                    and reparented["wrapperTransformAuthored"]
                    and reparented["wrapperTransformPreservedInNestedChild"]
                    and reparented["unpackRestoredOriginalWrapper"]
                    and reparented["unpackUndoRestoredNested"]
                    and reparented["unpackRedoRestoredOriginal"],
                    "Existing-precompose reparenting changed its wrapper transform, keys, hierarchy, or unpack undo/redo")
            require(reparented["hierarchy"] == [reparented["state"]["compositionId"],
                    reparented["wrapperChildId"], reparented["existingChildId"]],
                    "Reparented precompose hierarchy IDs do not match the expected wrapper chain")
            call("test.reset")
            unpacked_nested = call("test.unprecomposeNested")
            require(unpacked_nested["passed"] and unpacked_nested["nestingRemoved"]
                    and unpacked_nested["nestedSelectionCorrect"]
                    and unpacked_nested["restoredTiming"]
                    and unpacked_nested["nestedObjectsPreserved"]
                    and unpacked_nested["unpackPreservedObjects"]
                    and unpacked_nested["unpackSelectionCorrect"]
                    and unpacked_nested["undoRestoredNesting"]
                    and unpacked_nested["undoPreservedObjects"]
                    and unpacked_nested["undoSelectionCorrect"]
                    and unpacked_nested["redoRemovedNesting"]
                    and unpacked_nested["redoPreservedObjects"]
                    and unpacked_nested["redoSelectionCorrect"],
                    "Nested unpack changed timing or runtime nesting across undo/redo")
            call("test.reset")
            noncontiguous = call("test.nonContiguousSelectionOrder")
            require(noncontiguous["passed"] and noncontiguous["childOrderPreserved"]
                    and noncontiguous["childKeysRetimed"]
                    and noncontiguous["childSamplesPreserved"]
                    and noncontiguous["originalOrderRestored"]
                    and noncontiguous["unpackedKeysRestored"]
                    and noncontiguous["unpackedSamplesPreserved"]
                    and noncontiguous["undoRestoredPrecompose"]
                    and noncontiguous["undoAnimationPreserved"]
                    and noncontiguous["redoRestoredOriginalOrder"]
                    and noncontiguous["redoAnimationRestored"],
                    "Noncontiguous precompose/unpack changed animation timing or layer ordering")
            capture_state_before = call("test.state")
            image = request("tools/call", {"name": "test.capture", "arguments": {}})
            require(not image.get("isError") and image["content"][0]["type"] == "image"
                    and image["content"][0].get("mimeType") == "image/png",
                    "Capture did not return a PNG image")
            png = base64.b64decode(image["content"][0]["data"], validate=True)
            width, height = validate_png(png)
            require((width, height) == reset_dimensions == recovered_capture_dimensions,
                    "GUI capture dimensions changed during the precompose test run")
            require(call("test.state") == capture_state_before,
                    "Capture changed the composition, selection, or undo history")
            (args.output_dir / "nested.png").write_bytes(png)
            capture_files_written.append("nested.png")
            evidence[-1]["reply"] = {"image": "nested.png", "dimensions": (width, height)}
            regression = call("test.runRegression")
            regression_state = regression.get("state", {})
            require(regression["regressionFailures"] == 0
                    and regression_state.get("regressionFailures") == 0
                    and regression_state.get("locale") == args.language
                    and [layer["name"] for layer in regression_state.get("layers", [])] ==
                    ["A", "B", "Untouched"]
                    and regression_state.get("compositionId")
                    and "lastChildExists" not in regression_state
                    and not regression_state.get("canUndo")
                    and not regression_state.get("canRedo")
                    and regression_state.get("resolution") == [1280, 720]
                    and abs(regression_state.get("frameRate", 0.0) - 24.0) < 1e-6
                    and regression_state.get("frameRangeDuration") == 300
                    and regression_state.get("backgroundColor") == [0.125, 0.25, 0.5, 1.0]
                    and regression_state.get("selection", {}).get("activeCompositionId") ==
                    regression_state.get("compositionId")
                    and regression_state.get("selection", {}).get("selectedLayerIds") == []
                    and "currentLayerId" not in regression_state.get("selection", {})
                    and {name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                         for name, layer in layer_map(regression_state).items()} ==
                    {"A": (40, 200, 7), "B": (70, 160, 9), "Untouched": (25, 220, 3)}
                    and call("test.state") == regression_state,
                    "Existing precompose regression suite failed or did not restore the default fixture")
            require(invoked_tool_names == exposed_tool_names,
                    f"MCP tool catalog has unexercised or unavailable tools: "
                    f"unexercised={sorted(exposed_tool_names - invoked_tool_names)}, "
                    f"unadvertised={sorted(invoked_tool_names - exposed_tool_names)}")
            print("PASS: MCP tool catalog and malformed-call rejection, direct/transitive cycle guards, invalid-request and empty MoveAll atomicity, redo-branch replacement, closed/open-child focus behavior, component descriptor graph, matte reference order/settings, layer mask geometry/state, layer/composition effect scope and parameter round-trip, single-layer round-trip, shared-child references/data, negative frame/key timing, workspace-matched duration, child-duration propagation, internal/external layer-parent restoration, nested Master Property save/reload/materialization, animated key timing/metadata in selected/all modes, nested hierarchy/time/save-reload, existing-precompose reparenting, nested unpack registration/timing, noncontiguous unpack ordering, unpack keep/delete, identity, attributes, order, save/reload, undo/redo, regression suite and fixture reset")
            tests_passed = True
        except Exception as error:
            evidence.append({"harnessError": f"{type(error).__name__}: {error}"})
            raise
        finally:
            forced_termination = False
            forced_kill = False
            try:
                process.stdin.close()
            except OSError:
                pass
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                forced_termination = True
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    forced_kill = True
                    process.kill()
                    process.wait()
            reader_thread.join(timeout=2)
            trailing_stdout = []
            while True:
                try:
                    message = replies.get_nowait()
                except queue.Empty:
                    break
                if message != {"eof": True}:
                    trailing_stdout.append(message)
            reader_stopped = not reader_thread.is_alive()
            harness_passed = (tests_passed and process.returncode == 0
                              and reader_stopped and not trailing_stdout)
            evidence.append({"serverExitCode": process.returncode})
            evidence.append({"serverForcedTermination": forced_termination,
                             "serverForcedKill": forced_kill})
            evidence.append({"stdoutReaderStopped": reader_stopped,
                             "trailingStdout": trailing_stdout})
            evidence.append({"harnessPassed": harness_passed})
            evidence.append({"captureFiles": capture_files_written})
            (args.output_dir / "result.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2),
                                                         encoding="utf-8")
        if reader_thread.is_alive():
            raise RuntimeError("MCP stdout reader did not stop after server exit")
        if trailing_stdout:
            raise RuntimeError(f"MCP server wrote unsolicited stdout after its final reply: {trailing_stdout}")
        if process.returncode:
            raise RuntimeError(f"Server exit code: {process.returncode}")


if __name__ == "__main__":
    main()
