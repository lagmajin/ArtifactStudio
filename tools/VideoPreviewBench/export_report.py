"""Export completed preview reports without changing or rerunning the benchmark."""

import argparse
import csv
import datetime
import hashlib
import json
import re
from pathlib import Path


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    stat = path.stat()
    return {"path": str(path.resolve()), "bytes": stat.st_size,
            "sha256": digest.hexdigest(), "mtimeNs": stat.st_mtime_ns}


def first_late_events(case):
    """Parse explicit cold snapshots; absent snapshots remain unavailable."""
    if "videoTimingByLayer" not in case:
        return None
    events = []
    for layer in case["videoTimingByLayer"]:
        _, separator, tail = layer.get("diagnostics", "").rpartition(" firstLateFrame=")
        if not separator:
            continue
        text = "firstLateFrame=" + tail
        values = {key: value for key, value in re.findall(
            r"\b(firstLate\w+)=(-?\d+(?:\.\d+)?)", text)}
        if int(values.get("firstLateFrame", "-1")) < 0:
            continue
        event = {"layer": layer.get("layer"), "layerId": layer.get("layerId"),
                 "sourcePath": layer.get("sourcePath")}
        for key, value in values.items():
            event[key] = int(value) if key.endswith("Frame") else float(value)
        events.append(event)
    return events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path)
    parser.add_argument("--executable", type=Path,
                        help="Optional post-run executable snapshot; does not prove launch identity.")
    parser.add_argument("--environment", type=Path,
                        help="Optional independently captured environment JSON.")
    parser.add_argument("--process-memory", type=Path,
                        help="Optional external whole-process memory observation JSON; not per-case memory.")
    parser.add_argument("--launch-record", type=Path,
                        help="Optional pre-launch executable/arguments record for this exact report.")
    args = parser.parse_args()
    report = json.loads(args.report.read_text(encoding="utf-8-sig"))
    if report.get("schema") != "artifact-video-preview-bench-v1":
        parser.error("Unsupported benchmark report schema")
    cases = report.get("cases")
    if not isinstance(cases, list) or not cases:
        parser.error("Completed report must contain at least one case")
    launch_record = None
    if args.launch_record:
        launch_record = json.loads(args.launch_record.read_text(encoding="utf-8-sig"))
        if (not isinstance(launch_record, dict) or
                Path(launch_record.get("report", "")).resolve() != args.report.resolve()):
            parser.error("Launch record must identify this exact report")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", launch_record.get("sha256BeforeLaunch", "")):
            parser.error("Launch record lacks a pre-launch SHA256")
    prefix = args.output_prefix
    prefix.parent.mkdir(parents=True, exist_ok=True)
    csv_path = Path(str(prefix) + ".csv")
    manifest_path = Path(str(prefix) + ".manifest.json")
    inputs = [path.resolve() for path in
              (args.report, args.executable, args.environment, args.process_memory,
               args.launch_record) if path]
    if any(path in inputs for path in (csv_path.resolve(), manifest_path.resolve())):
        parser.error("Export destination must not replace any input")
    fields = ["layerCount", "status", "passed", "measuredSeconds",
              "observedPresentedFrames", "renderCycles", "finalFrame", "finalPresentedFrame",
              "maximumDiagnosticSampleMs", "maximumDiagnosticSampleAtMs",
              "maximumVideoDiagnosticSampleMs",
              "droppedFrames", "lateFramesObserved", "repeatedFramesObserved",
              "previewReadyFrames", "previewRequestedFrames", "previewFramesInRam",
              "previewFramesOnDisk", "cachedImageWidth", "cachedImageHeight", "error"]
    fields += ["preparedRamPreviewFrames", "preparedDiskPreviewFrames"]
    conditions = ["compositionWidth", "compositionHeight", "compositionFps",
                  "viewportWidth", "viewportHeight", "viewportDevicePixelRatio", "diagnosticSamplingPolicy",
                  "ramWarmupFrames", "cacheEnabled", "diskCacheEnabled", "waitForDiskCache", "measurementLoop", "loopMeasurement",
                  "sourceWindowSeconds", "rendererBackendOverride", "videoTimingCountPolicy"]
    late_fields = ["firstLateFrame", "firstLateElapsedMs", "firstLateDecodeMs",
                   "firstLateQueuedMs", "firstLatePresentedMs", "firstLateRenderToPresentMs"]
    with csv_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields + conditions +
                                ["sourceCodecs", "layersWithFirstLateEvidence"] + late_fields,
                                lineterminator="\n")
        writer.writeheader()
        for case in cases:
            row = {key: case.get(key) for key in fields}
            row.update({key: report.get(key) for key in conditions})
            row["sourceCodecs"] = ";".join(str(source.get("codec", ""))
                                           for source in case.get("layerSources", []))
            events = first_late_events(case)
            row["layersWithFirstLateEvidence"] = len(events) if events is not None else None
            if events:
                first = min(events, key=lambda event: event.get("firstLateElapsedMs", float("inf")))
                row.update({key: first.get(key) for key in late_fields})
            writer.writerow(row)
    manifest = {
        "schema": "artifact-video-preview-export-v1",
        "exportedAt": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "report": fingerprint(args.report),
        "reportCreatedAt": report.get("createdAt"),
        "conditions": {key: report.get(key) for key in conditions},
        "reportedEnvironment": {key: report.get(key) for key in
                                ["host", "cpuArchitecture", "qtVersion", "cachePolicy"]},
        "layerSourcesByCase": [{"layerCount": case.get("layerCount"),
                                "sources": case.get("layerSources", [])} for case in cases],
        "videoTimingByCase": [{"layerCount": case.get("layerCount"),
                               "layers": case.get("videoTimingByLayer", [])} for case in cases],
        "firstLateEventsByCase": [{"layerCount": case.get("layerCount"),
                                   "events": first_late_events(case)} for case in cases],
        "firstLateCsvPolicy": "Earliest elapsed first-late snapshot among layers; missing snapshots are blank, not zero",
        "missingReportedMetrics": ["physicalScanout", "blackFrames", "staleFrames",
                                   "frameIntervalPercentiles", "cpuGpuUtilization",
                                   "peakProcessMemory", "sourcePeakBitrate", "sourceGop",
                                   "decoderBackendPerLayer", "proxyMode", "memoryBudget",
                                   "processLaunchBuildIdentity"],
        "csv": str(csv_path.resolve()),
        "caseCount": len(cases),
    }
    if args.executable:
        manifest["postRunExecutableSnapshot"] = fingerprint(args.executable)
        manifest["executableIdentityCaveat"] = "Captured at export, not automatically at process launch"
    if args.environment:
        manifest["independentEnvironment"] = json.loads(
            args.environment.read_text(encoding="utf-8-sig"))
        manifest["environmentSource"] = fingerprint(args.environment)
    if args.process_memory:
        manifest["independentProcessMemory"] = json.loads(
            args.process_memory.read_text(encoding="utf-8-sig"))
        manifest["processMemorySource"] = fingerprint(args.process_memory)
        manifest["processMemoryCaveat"] = (
            "External observation scope and completeness are stated in the source; "
            "whole-process peaks include preparation and are not per-layer-case peaks")
    if launch_record is not None:
        manifest["independentLaunchRecord"] = launch_record
        manifest["launchRecordSource"] = fingerprint(args.launch_record)
        manifest["launchIdentityCaveat"] = (
            "Recorded executable file hash before launch; not loaded-module verification")
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"csv": str(csv_path.resolve()),
                      "manifest": str(manifest_path.resolve()), "cases": len(cases)}))


if __name__ == "__main__":
    main()
