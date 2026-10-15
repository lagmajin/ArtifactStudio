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
    exposed_tool_names = set()
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
            if name not in exposed_tool_names:
                raise AssertionError(f"MCP tools/list did not advertise {name}")
            reply = request("tools/call", {"name": name, "arguments": {}})
            if reply.get("isError"):
                raise RuntimeError(f"{name}: {reply}")
            return json.loads(reply["content"][0]["text"])

        def require(condition, message):
            if not condition:
                raise AssertionError(message)

        def layer_map(state):
            return {layer["name"]: layer for layer in state["layers"]}

        def require_child_settings(result, label):
            require(result["childSettingsCopied"]
                    and result["childResolution"] == [1280, 720]
                    and abs(result["childFrameRate"] - 24.0) < 1e-6
                    and result["childFrameRangeDuration"] == 300
                    and result["parentFrameRangeDuration"] == 300
                    and all(abs(actual - expected) < 1e-6 for actual, expected in
                            zip(result["childBackgroundColor"], (0.125, 0.25, 0.5, 1.0))),
                    f"{label} precompose did not copy custom composition settings")

        try:
            request("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                   "clientInfo": {"name": "precompose-regression", "version": "1"}})
            catalog = request("tools/list", {})
            listed_tools = catalog.get("tools")
            require(isinstance(listed_tools, list), "MCP tools/list did not return a tools array")
            names = [tool.get("name") for tool in listed_tools if isinstance(tool, dict)]
            require(len(names) == len(listed_tools) and len(names) == len(set(names)),
                    "MCP tools/list returned invalid or duplicate tool names")
            exposed_tool_names.update(names)
            initial = call("test.state")
            original = initial["layers"]
            require(set(layer_map(initial)) == {"A", "B", "Untouched"}, "Fixture missing layers")
            expected_ranges = {"A": (40, 200, 7), "B": (70, 160, 9),
                               "Untouched": (25, 220, 3)}
            for name, (in_frame, out_frame, start_frame) in expected_ranges.items():
                layer = layer_map(initial)[name]
                require((layer["inFrame"], layer["outFrame"], layer["startFrame"]) ==
                        (in_frame, out_frame, start_frame), f"Unexpected fixture timing for {name}")
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
            redo_branch = call("test.redoBranchDiscarded")
            require(redo_branch["passed"] and redo_branch["redoBranchCreated"]
                    and redo_branch["abandonedRedoDiscarded"]
                    and redo_branch["replacementUndoRestored"]
                    and redo_branch["replacementRedoRestored"],
                    "A new precompose did not discard and replace the old redo branch")
            call("test.reset")
            opened_child = call("test.openChildComposition")
            require(opened_child["passed"] and opened_child["childOpened"]
                    and opened_child["rootWrapperCreated"] and opened_child["undoReturnedToRoot"]
                    and opened_child["redoKeptRootFocused"] and opened_child["redoRestoredChild"],
                    "Open-child precompose navigation or undo/redo focus changed")
            call("test.reset")
            negative_timing = call("test.negativeTimingRoundTrip")
            require(negative_timing["passed"] and negative_timing["childTimingNormalized"]
                    and negative_timing["childKeysShifted"] and negative_timing["unpackRestored"]
                    and negative_timing["undoRestored"] and negative_timing["redoRestored"],
                    "Negative frame timing or key offsets changed across precompose/unprecompose/undo/redo")
            call("test.reset")
            rational_rate = call("test.rationalFrameRatePreserved")
            require(rational_rate["passed"] and rational_rate["selectedRatePreserved"]
                    and rational_rate["roundTripRatePreserved"]
                    and rational_rate["parentRateAfterSelected"] and rational_rate["allRatePreserved"]
                    and rational_rate["allUnpacked"] and rational_rate["undoRatePreserved"]
                    and rational_rate["redoRestored"],
                    "Exact rational frame rate changed across selected/all precompose or undo/redo")
            call("test.reset")
            workspace_duration = call("test.matchWorkspaceDuration")
            require(workspace_duration["passed"]
                    and workspace_duration["childOpened"]
                    and workspace_duration["childDurationMatched"]
                    and workspace_duration["childWorkAreaNormalized"]
                    and workspace_duration["wrapperTimingPreserved"],
                    "Precompose did not match workspace duration while preserving wrapper timing")
            call("test.reset")
            duration_propagation = call("test.childDurationPropagates")
            require(duration_propagation["passed"] and duration_propagation["shrinkFollowed"]
                    and duration_propagation["expansionFollowed"]
                    and duration_propagation["parentReferencePreserved"],
                    "Child duration shrink/expansion did not propagate to the parent precompose window")
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
                    and nested_override["nestedOverridePreserved"]
                    and nested_override["saveReloadPreserved"]
                    and nested_override["outerUnpacked"]
                    and nested_override["innerOverridePreserved"]
                    and nested_override["overrideMaterialized"]
                    and nested_override["undoPreservedOverride"]
                    and nested_override["redoPreservedOverride"],
                    "Nested Master Property override changed across precompose/unpack/undo/redo")
            call("test.reset")
            missing_child = call("test.missingChildReferenceAtomic")
            require(missing_child["passed"] and missing_child["unwrapRejected"]
                    and missing_child["parentStatePreserved"]
                    and missing_child["compositionPreserved"]
                    and missing_child["wrapperPreserved"]
                    and missing_child["undoHistoryPreserved"]
                    and missing_child["outcomeCleared"]
                    and missing_child["fixtureRestored"],
                    "Unprecompose mutated a wrapper with a missing child reference")
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
                    and delete_redo["restoredByUndo"] and delete_redo["deletedByRedo"]
                    and not delete_redo["state"]["lastChildExists"],
                    "Delete-unpack Undo/Redo did not restore then remove the same child composition")
            call("test.reset")
            single_layer = call("test.singleLayerRoundTrip")
            require(single_layer["passed"] and single_layer["wrappedCorrectly"]
                    and single_layer["undoRestored"] and single_layer["redoRestored"],
                    "Single-layer precompose changed layer identity, order, or registration")
            call("test.reset")
            shared_child = call("test.sharedChildReferences")
            require(shared_child["passed"] and shared_child["twoReferencesCreated"]
                    and shared_child["childDataPreserved"]
                    and shared_child["remainingReferencePreserved"]
                    and shared_child["undoRestoredBoth"]
                    and shared_child["redoChildDataPreserved"]
                    and shared_child["redoPreservedRemaining"],
                    "Unprecomposing one instance changed shared child data or another reference")
            call("test.reset")
            external_parent = call("test.externalParentRestored")
            require(external_parent["passed"] and external_parent["externalParentStoredOnWrapper"]
                    and external_parent["externalParentClearedInChild"]
                    and external_parent["unpackRestoredExternalParent"]
                    and external_parent["undoRestoredMetadata"]
                    and external_parent["redoRestoredExternalParent"],
                    "External layer parent was not preserved across precompose/undo/redo")
            call("test.reset")
            child_edits = call("test.childEditsSurviveUnwrap")
            require(child_edits["passed"] and child_edits["editsAuthored"]
                    and child_edits["unpackedValuesPreserved"]
                    and child_edits["undoValuesPreserved"]
                    and child_edits["redoValuesPreserved"],
                    "Transform or opacity edits inside a child composition changed across unprecompose/undo/redo")
            call("test.reset")
            selection_transitions = call("test.selectionTransitions")
            require(selection_transitions["passed"]
                    and selection_transitions["precomposeSelectedWrapper"]
                    and selection_transitions["unpackSelectedFirstSource"]
                    and selection_transitions["undoSelectedWrapper"]
                    and selection_transitions["redoSelectedFirstSource"],
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
                    and animated["offsetChildKeysRetimed"]
                    and animated["unpackRestoredKeys"] and animated["redoRestoredKeys"]
                    and animated["allModeChildKeysPreserved"]
                    and animated["allModeChildTimingPreserved"]
                    and animated["allModeUnpackKeysPreserved"]
                    and animated["allModeUnpackTimingPreserved"]
                    and animated["allModeRedoKeysPreserved"]
                    and animated["allModeRedoTimingPreserved"],
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
            require(kept["layers"] == original and kept["lastChildExists"], "Unpack keep contract failed")
            require({name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                     for name, layer in layer_map(kept).items()} == expected_ranges,
                    "Unpack keep did not restore original timing")
            require(call("test.undo")["layers"] == nested["layers"], "Unpack undo failed")
            removed = call("test.unpackDelete")
            require(removed["layers"] == original and not removed["lastChildExists"], "Unpack delete failed")
            require({name: (layer["inFrame"], layer["outFrame"], layer["startFrame"])
                     for name, layer in layer_map(removed).items()} == expected_ranges,
                    "Unpack delete did not restore original timing")
            restored = call("test.undo")
            require(restored["lastChildExists"] and restored["layers"] == nested["layers"], "Deleted child undo failed")
            call("test.reset")
            all_nested = call("test.precomposeAll")
            require_child_settings(all_nested, "Move all")
            require(all_nested["emptySelectionSubmitted"] and len(all_nested["layers"]) == 1
                    and len(all_nested["layers"][0]["children"]) == 3,
                    "Move all with an empty selection must include every layer")
            all_layer = all_nested["layers"][0]
            require((all_layer["inFrame"], all_layer["startFrame"]) == (0, 0),
                    "Move all precompose layer must start at composition time zero")
            for name, (in_frame, out_frame, start_frame) in expected_ranges.items():
                child_layer = next(layer for layer in all_layer["children"] if layer["name"] == name)
                require((child_layer["inFrame"], child_layer["outFrame"], child_layer["startFrame"]) ==
                        (in_frame, out_frame, start_frame),
                        f"Move all changed {name} timing")
            round_trip = call("test.saveReload")
            require(round_trip["passed"] and round_trip["parentChildLinkPreserved"]
                    and round_trip["layerStatePreserved"] and round_trip["mappingClearedBeforeReload"]
                    and round_trip["runtimeNestingRestored"]
                    and round_trip["masterPropertyDefinitionPreserved"]
                    and round_trip["masterPropertyOverridePreserved"]
                    and round_trip["overrideStillDeferred"],
                    "Precompose save/reload changed parent-child data, exposed properties, or runtime nesting")
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
                    and reparented["redoRestoredParent"],
                    "Existing-precompose reparenting or undo/redo did not update its runtime hierarchy")
            require(reparented["hierarchy"] == [reparented["state"]["compositionId"],
                    reparented["wrapperChildId"], reparented["existingChildId"]],
                    "Reparented precompose hierarchy IDs do not match the expected wrapper chain")
            call("test.reset")
            unpacked_nested = call("test.unprecomposeNested")
            require(unpacked_nested["passed"] and unpacked_nested["nestingRemoved"]
                    and unpacked_nested["restoredTiming"]
                    and unpacked_nested["undoRestoredNesting"]
                    and unpacked_nested["redoRemovedNesting"],
                    "Nested unpack changed timing or runtime nesting across undo/redo")
            call("test.reset")
            noncontiguous = call("test.nonContiguousSelectionOrder")
            require(noncontiguous["passed"] and noncontiguous["childOrderPreserved"]
                    and noncontiguous["originalOrderRestored"]
                    and noncontiguous["undoRestoredPrecompose"]
                    and noncontiguous["redoRestoredOriginalOrder"],
                    "Noncontiguous precompose/unpack did not restore source layer ordering")
            require("test.capture" in exposed_tool_names,
                    "MCP tools/list did not advertise test.capture")
            image = request("tools/call", {"name": "test.capture", "arguments": {}})
            require(not image.get("isError") and image["content"][0]["type"] == "image", "Capture failed")
            (args.output_dir / "nested.png").write_bytes(base64.b64decode(image["content"][0]["data"]))
            evidence[-1]["reply"] = {"image": "nested.png"}
            require(call("test.runRegression")["regressionFailures"] == 0, "Existing regression suite failed")
            print("PASS: direct/transitive cycle guards, invalid-request atomicity, redo-branch replacement, open-child navigation/focus, negative frame/key timing, workspace-matched duration, child-duration propagation, internal/external layer-parent restoration, nested Master Property save/reload/materialization, animated key timing/metadata in selected/all modes, nested hierarchy/time/save-reload, existing-precompose reparenting, nested unpack registration/timing, noncontiguous unpack ordering, unpack keep/delete, identity, attributes, order, save/reload, undo/redo, regression suite")
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
