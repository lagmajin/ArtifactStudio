"""Exercise the independent Artifact Audio Bus Viewer through its stdio MCP API."""
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True,
                        help="Path to ArtifactAudioBusViewer.exe")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--offscreen", action="store_true",
                        help="Run the Qt GUI without showing a window")
    args = parser.parse_args()
    executable = args.exe.resolve()
    if not executable.is_file():
        raise SystemExit(f"Executable not found: {executable}")

    environment = os.environ.copy()
    if args.offscreen:
        environment["QT_QPA_PLATFORM"] = "offscreen"
    elif os.name == "nt":
        environment["QT_QPA_PLATFORM"] = "windows"
    if os.name == "nt":
        environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(executable.parent / "platforms")

    responses: queue.Queue[dict] = queue.Queue()
    process = subprocess.Popen(
        [str(executable), "--mcp-stdio"], stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        encoding="utf-8", env=environment)

    def read_responses() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            try:
                responses.put(json.loads(line))
            except json.JSONDecodeError:
                responses.put({"invalidStdout": line.rstrip()})
        responses.put({"eof": True})

    def read_errors() -> None:
        assert process.stderr is not None
        for line in process.stderr:
            print(line, end="", file=sys.stderr)

    threading.Thread(target=read_responses, daemon=True).start()
    threading.Thread(target=read_errors, daemon=True).start()
    serial = 0

    def request(method: str, params: dict) -> dict:
        nonlocal serial
        serial += 1
        assert process.stdin is not None
        process.stdin.write(json.dumps({"jsonrpc": "2.0", "id": serial,
                                        "method": method, "params": params}) + "\n")
        process.stdin.flush()
        response = responses.get(timeout=args.timeout)
        if response.get("id") != serial or "error" in response:
            raise RuntimeError(f"Invalid MCP response: {response}")
        return response["result"]

    def notify(method: str, params: dict) -> None:
        assert process.stdin is not None
        process.stdin.write(json.dumps({"jsonrpc": "2.0", "method": method,
                                        "params": params}) + "\n")
        process.stdin.flush()

    def call(name: str, arguments: dict | None = None) -> dict:
        result = request("tools/call", {"name": name,
                                         "arguments": arguments or {}})
        if result.get("isError"):
            raise RuntimeError(f"MCP tool {name} failed: {result}")
        state = result.get("structuredContent")
        if not isinstance(state, dict):
            raise RuntimeError(f"MCP tool {name} returned no structured state: {result}")
        if state.get("error"):
            raise RuntimeError(f"MCP tool {name} failed: {state['error']}")
        return state

    try:
        request("initialize", {
            "protocolVersion": "2025-11-25", "capabilities": {},
            "clientInfo": {"name": "audio-bus-contract", "version": "1"}})
        notify("notifications/initialized", {})
        tool_list = request("tools/list", {}).get("tools", [])
        names = {tool.get("name") for tool in tool_list}
        required = {"audio.state", "audio.resetFixture", "audio.processProbe",
                    "audio.setBus", "audio.setRoute", "audio.addSidechainSend",
                    "audio.removeSidechainSend", "audio.setVcaMember",
                    "audio.runContract"}
        if not required.issubset(names):
            raise RuntimeError(f"Missing MCP tools: {sorted(required - names)}")

        report = call("audio.runContract")
        if not report.get("passed"):
            raise RuntimeError(f"Offline AudioMixer contract failed: {report}")

        state = call("audio.state")
        graph_summary = state.get("graphSummary", "")
        if not all(token in graph_summary for token in (
                "Primary:", "Sidechain:", "VCA:", "Dialogue Group", "Stem Bus",
                "FX Return", "25%, Pre", "50%, Post", "Dialogue VCA")):
            raise RuntimeError(f"Visible routing graph summary was incomplete: {graph_summary}")
        buses = {bus["name"]: bus for bus in state.get("buses", [])}
        expected = {"Master", "Dialogue Group", "Music Group", "Stem Bus",
                    "FX Return", "Dialogue VCA", "Dialogue Trim VCA",
                    "Dialogue", "Music", "Ambience", "FX Bed"}
        if set(buses) != expected:
            raise RuntimeError(f"Unexpected fixture buses: {sorted(buses)}")
        if buses["Dialogue"]["routeTarget"] != "Dialogue Group":
            raise RuntimeError("Dialogue route was not present in viewer state")
        if buses["Dialogue Group"]["routeTarget"] != "Stem Bus" or \
                buses["Music Group"]["routeTarget"] != "Stem Bus" or \
                buses["Stem Bus"]["routeTarget"] != "Master":
            raise RuntimeError("Nested group-to-stem routing was not present in viewer state")
        if buses["FX Bed"]["routeTarget"] != "FX Return" or \
                buses["FX Return"]["routeTarget"] != "Master":
            raise RuntimeError("FX Bed was not routed through the compressor return to Master")
        return_effects = buses["FX Return"].get("effects", [])
        if len(return_effects) != 1 or return_effects[0].get("type") != "compressor" or \
                not return_effects[0].get("sidechain"):
            raise RuntimeError(f"FX Return sidechain compressor was not present: {return_effects}")
        dialogue_sends = buses["Dialogue"]["sends"]
        music_sends = buses["Music"]["sends"]
        if len(dialogue_sends) != 1 or dialogue_sends[0]["amount"] != 0.25 or \
                not dialogue_sends[0]["preFader"]:
            raise RuntimeError(f"Dialogue Pre-Fader send was not present: {dialogue_sends}")
        if len(music_sends) != 1 or music_sends[0]["amount"] != 0.5 or \
                music_sends[0]["preFader"]:
            raise RuntimeError(f"Music Post-Fader send was not present: {music_sends}")
        for vca_name in ("Dialogue VCA", "Dialogue Trim VCA"):
            if buses[vca_name].get("members") != ["Dialogue"]:
                raise RuntimeError(f"Unexpected VCA membership for {vca_name}: {buses[vca_name]}")

        send_edit = call("audio.addSidechainSend", {
            "source": "Dialogue", "target": "FX Return",
            "amount": 0.5, "preFader": False})
        if not send_edit.get("sendAccepted"):
            raise RuntimeError(f"Live sidechain send edit was rejected: {send_edit}")
        if "Dialogue → FX Return (50%, Post)" not in send_edit.get("graphSummary", ""):
            raise RuntimeError(f"Visible graph summary did not reflect the edited send: {send_edit}")
        edited_send_state = next(bus for bus in send_edit["buses"]
                                 if bus["name"] == "Dialogue")
        if edited_send_state["sends"] != [{
                "target": "FX Return", "amount": 0.5, "preFader": False}]:
            raise RuntimeError(f"Live sidechain send edit was not reflected in state: {edited_send_state}")
        edited_send_probe = call("audio.processProbe", {"frames": 256})
        if abs(edited_send_probe["fxReturnSidechainRms"] - 0.15) > 0.003:
            raise RuntimeError(f"Live sidechain send edit did not change detector input: {edited_send_probe}")

        removed_send = call("audio.removeSidechainSend", {
            "source": "Dialogue", "target": "FX Return"})
        if not removed_send.get("sendRemoved"):
            raise RuntimeError(f"Live sidechain send removal was rejected: {removed_send}")
        if "Dialogue → FX Return" in removed_send.get("graphSummary", ""):
            raise RuntimeError(f"Visible graph summary retained the removed send: {removed_send}")
        removed_dialogue_state = next(bus for bus in removed_send["buses"]
                                      if bus["name"] == "Dialogue")
        if removed_dialogue_state["sends"]:
            raise RuntimeError(f"Removed sidechain send remained in state: {removed_dialogue_state}")
        removed_send_probe = call("audio.processProbe", {"frames": 256})
        if abs(removed_send_probe["fxReturnSidechainRms"] - 0.05) > 0.003:
            raise RuntimeError(f"Removing the send did not remove its detector contribution: {removed_send_probe}")

        restored_send = call("audio.addSidechainSend", {
            "source": "Dialogue", "target": "FX Return",
            "amount": 0.25, "preFader": True})
        if not restored_send.get("sendAccepted"):
            raise RuntimeError(f"Fixture sidechain send could not be restored: {restored_send}")
        if "Dialogue → FX Return (25%, Pre)" not in restored_send.get("graphSummary", ""):
            raise RuntimeError(f"Visible graph summary did not reflect the restored send: {restored_send}")

        probe = call("audio.processProbe", {"frames": 256})
        if abs(probe["leftRms"] - 0.35) > 0.003:
            raise RuntimeError(f"Unexpected baseline output: {probe}")
        if abs(probe["fxReturnSidechainRms"] - 0.10) > 0.003:
            raise RuntimeError(f"Unexpected combined sidechain signal: {probe}")

        fx_probe = call("audio.processProbe", {"frames": 256, "fxBed": 0.20})
        if not 0.0 < fx_probe["fxReturnOutputRms"] < 0.20 or \
                fx_probe["fxReturnGainReduction"] >= 1.0:
            raise RuntimeError(f"Sidechain did not compress the routed FX Bed: {fx_probe}")
        if abs(fx_probe["leftRms"] - (0.35 + fx_probe["fxReturnOutputRms"])) > 0.005:
            raise RuntimeError(f"Compressed FX Return did not reach the Master output: {fx_probe}")

        vca_assignment = call("audio.setVcaMember", {
            "vca": "Dialogue VCA", "member": "Music", "assigned": True})
        if not vca_assignment.get("membershipChanged") or \
                "Dialogue VCA → Dialogue, Music" not in vca_assignment.get("graphSummary", ""):
            raise RuntimeError(f"VCA assignment was not reflected in the graph: {vca_assignment}")
        call("audio.setBus", {"name": "Dialogue VCA", "volumeDb": -6.0206})
        shared_vca_mix = call("audio.processProbe", {"frames": 256})
        if abs(shared_vca_mix["leftRms"] - 0.20) > 0.004 or \
                abs(shared_vca_mix["fxReturnSidechainRms"] - 0.075) > 0.003:
            raise RuntimeError(f"VCA gain did not affect its main and post-fader send paths: {shared_vca_mix}")
        vca_removal = call("audio.setVcaMember", {
            "vca": "Dialogue VCA", "member": "Music", "assigned": False})
        if not vca_removal.get("membershipChanged") or \
                "Dialogue VCA → Dialogue, Music" in vca_removal.get("graphSummary", ""):
            raise RuntimeError(f"VCA member removal was not reflected in the graph: {vca_removal}")
        call("audio.setBus", {"name": "Dialogue VCA", "volumeDb": 0.0})
        restored_vca_mix = call("audio.processProbe", {"frames": 256})
        if abs(restored_vca_mix["leftRms"] - 0.35) > 0.003:
            raise RuntimeError(f"Restoring the VCA membership changed the baseline mix: {restored_vca_mix}")

        rerouted = call("audio.setRoute", {
            "source": "Music Group", "target": "Master"})
        if "Music Group [group] → Master" not in rerouted.get("graphSummary", ""):
            raise RuntimeError(f"Visible graph summary did not reflect the route edit: {rerouted}")
        rerouted_state = call("audio.state")
        music_group = next(bus for bus in rerouted_state["buses"]
                           if bus["name"] == "Music Group")
        if music_group["routeTarget"] != "Master":
            raise RuntimeError("Live route edit did not update the music group destination")
        restored_route = call("audio.setRoute", {
            "source": "Music Group", "target": "Stem Bus"})
        if "Music Group [group] → Stem Bus" not in restored_route.get("graphSummary", ""):
            raise RuntimeError(f"Visible graph summary did not reflect the restored route: {restored_route}")
        nested_probe = call("audio.processProbe", {"frames": 256})
        if abs(nested_probe["leftRms"] - 0.35) > 0.003:
            raise RuntimeError(f"Restored nested route changed the expected mix: {nested_probe}")

        call("audio.setBus", {"name": "Dialogue Group", "pan": 1.0})
        panned = call("audio.processProbe", {"frames": 256})
        if abs(panned["leftRms"] - 0.15) > 0.003 or abs(panned["rightRms"] - 0.35) > 0.003:
            raise RuntimeError(f"Group pan did not change stereo balance: {panned}")
        call("audio.setBus", {"name": "Dialogue Group", "pan": 0.0})

        muted = call("audio.setBus", {"name": "Dialogue", "mute": True})
        if not next(bus for bus in muted["buses"] if bus["name"] == "Dialogue")["mute"]:
            raise RuntimeError("Mute state did not reach the visible mixer model")
        muted_probe = call("audio.processProbe", {"frames": 256})
        if abs(muted_probe["leftRms"] - 0.15) > 0.003:
            raise RuntimeError(f"Mute did not remove the dialogue branch: {muted_probe}")
        call("audio.setBus", {"name": "Dialogue", "mute": False})

        call("audio.setBus", {"name": "Master", "volumeDb": -6.0206})
        master_gain = call("audio.processProbe", {"frames": 256})
        if abs(master_gain["leftRms"] - 0.175) > 0.003:
            raise RuntimeError(f"Master gain did not affect final output: {master_gain}")
        call("audio.setBus", {"name": "Master", "volumeDb": 0.0})
        call("audio.setBus", {"name": "Master", "mute": True})
        master_mute = call("audio.processProbe", {"frames": 256})
        if master_mute["leftRms"] > 0.001 or master_mute["rightRms"] > 0.001:
            raise RuntimeError(f"Master mute did not silence output: {master_mute}")
        call("audio.setBus", {"name": "Master", "mute": False})

        cycle = call("audio.setRoute", {
            "source": "Dialogue Group", "target": "Dialogue"})
        if cycle.get("routeAccepted") or cycle.get("operationResult") != "This route would create an audio routing cycle.":
            raise RuntimeError(f"Cycle was not rejected: {cycle}")
        state_after_rejection = call("audio.state")
        dialogue = next(bus for bus in state_after_rejection["buses"]
                        if bus["name"] == "Dialogue")
        if dialogue["routeTarget"] != "Dialogue Group":
            raise RuntimeError("Rejected route altered the committed graph")

        reset_state = call("audio.resetFixture")
        reset_buses = {bus["name"]: bus for bus in reset_state.get("buses", [])}
        if set(reset_buses) != expected or reset_state.get("graphSummary") != graph_summary:
            raise RuntimeError(f"Fixture reset did not restore the original routing graph: {reset_state}")
        if reset_buses["Dialogue"]["sends"] != dialogue_sends or \
                reset_buses["Music"]["sends"] != music_sends or \
                reset_buses["Dialogue VCA"].get("members") != ["Dialogue"]:
            raise RuntimeError(f"Fixture reset did not restore sends and VCA membership: {reset_state}")
        print(json.dumps({"passed": True, "checks": report["checks"],
                          "baselineRms": probe["leftRms"],
                          "editedSendSidechainRms": edited_send_probe["fxReturnSidechainRms"],
                          "removedSendSidechainRms": removed_send_probe["fxReturnSidechainRms"],
                          "sharedVcaRms": shared_vca_mix["leftRms"],
                          "sharedVcaSidechainRms": shared_vca_mix["fxReturnSidechainRms"],
                          "fxReturnSidechainRms": probe["fxReturnSidechainRms"],
                          "fxReturnOutputRms": fx_probe["fxReturnOutputRms"],
                          "mutedRms": muted_probe["leftRms"],
                          "pannedRms": [panned["leftRms"], panned["rightRms"]],
                          "masterGainRms": master_gain["leftRms"],
                          "masterMuteRms": master_mute["leftRms"],
                          "cycleRejected": True,
                          "fixtureRestored": True}, indent=2))
    finally:
        if process.stdin:
            process.stdin.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
