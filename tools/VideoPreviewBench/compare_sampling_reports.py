"""Compare completed diagnostic sampling trials offline; never launch or alter a benchmark."""

import argparse
import json
from pathlib import Path

from export_report import first_late_events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launches", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists; choose a new path")
    launches = json.loads(args.launches.read_text(encoding="utf-8-sig"))
    if not isinstance(launches, list) or not launches:
        parser.error("Launch records must be a nonempty completed array")
    sources = [args.launches.resolve()]
    trials = []
    signatures = []
    hashes = []
    condition_keys = ["compositionWidth", "compositionHeight", "compositionFps",
                      "viewportWidth", "viewportHeight", "viewportDevicePixelRatio",
                      "ramWarmupFrames", "cacheEnabled", "diskCacheEnabled", "waitForDiskCache",
                      "loopMeasurement", "sourceWindowSeconds", "rendererBackendOverride", "measurementLoop", "videoTimingCountPolicy"]
    for launch in launches:
        report_path = Path(launch["report"])
        sources.append(report_path.resolve())
        report = json.loads(report_path.read_text(encoding="utf-8-sig"))
        if report.get("schema") != "artifact-video-preview-bench-v1":
            parser.error("Unsupported report schema")
        policy = report.get("diagnosticSamplingPolicy")
        if policy not in ("final-only", "periodic-250ms-and-final"):
            parser.error("Missing or unsupported diagnostic sampling policy")
        after = launch.get("executableAfterRun", {}).get("sha256")
        if not after or after != launch.get("sha256BeforeLaunch"):
            parser.error("Trial executable changed or lacks post-run verification")
        if any(launch.get("environment", {}).get(key) not in (None, "0", "") for key in ("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP", "ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD")):
            parser.error("Profiled trials cannot enter normal sampling comparison")
        mode = report.get("measurementLoop")
        if mode not in ("polling", "qt") or mode != launch.get("mode"):
            parser.error("Report dispatch mode does not match launch record")
        if launch.get("exitCode") not in (0, 1):
            parser.error("Trial did not complete with a normal acceptance result")
        hashes.append(launch.get("sha256BeforeLaunch"))
        for case in report.get("cases", []):
            signature = {key: report.get(key) for key in condition_keys}
            signature.update({key: case.get(key) for key in
                              ["layerCount", "requestedFps", "requestedStart", "requestedEnd",
                               "preparedRamPreviewFrames", "preparedDiskPreviewFrames",
                               "cachedImageWidth", "cachedImageHeight", "layerSources"]})
            # Requested duration is an explicit launch argument; elapsed time
            # differs slightly across dispatch implementations and trials.
            arguments = launch.get("arguments", [])
            if "--seconds" not in arguments:
                parser.error("Launch record lacks requested measurement duration")
            signature["requestedSeconds"] = arguments[arguments.index("--seconds") + 1]
            signatures.append(signature)
            trials.append({"trial": launch.get("trial"), "mode": mode, "samplingPolicy": policy,
                           "report": str(report_path.resolve()),
                           "firstLateEvents": first_late_events(case),
                           **{key: case.get(key) for key in
                              ["layerCount", "passed", "status", "measuredSeconds",
                               "observedPresentedFrames", "renderCycles", "droppedFrames",
                               "lateFramesObserved", "repeatedFramesObserved",
                               "maximumDiagnosticSampleMs", "maximumDiagnosticSampleAtMs"]}})
    if not trials:
        parser.error("No completed cases")
    if args.output.resolve() in sources:
        parser.error("Output must not replace an input")
    missing_conditions = [[key for key, value in signature.items() if value is None]
                          for signature in signatures]
    same_conditions = not any(missing_conditions) and all(
        signature == signatures[0] for signature in signatures)
    same_executable = bool(hashes[0]) and all(value == hashes[0] for value in hashes)
    summary = {
        "schema": "artifact-video-sampling-comparison-v1",
        "sameReportedConditions": same_conditions,
        "samePreLaunchExecutableHash": same_executable,
        "missingConditionKeysByCase": missing_conditions,
        "conditions": signatures[0] if same_conditions else signatures,
        "groups": [{"samplingPolicy": policy, "caseCount": len(group),
                    "passedCases": sum(case["passed"] is True for case in group),
                    "lateCountsByCase": [case["lateFramesObserved"] for case in group],
                    "dropCountsByCase": [case["droppedFrames"] for case in group],
                    "repeatCountsByCase": [case["repeatedFramesObserved"] for case in group]}
                   for policy in ("periodic-250ms-and-final", "final-only")
                   if (group := [case for case in trials if case["samplingPolicy"] == policy])],
        "trials": trials,
        "caveats": ["Small sequential sample; no causal or statistical improvement claim",
                    "Short cached-window repetition, not long unique-source playback",
                    "Matching before/after file hashes, not loaded-module verification or physical scanout",
                    "Late/repeat values retain the benchmark's maximum-per-layer policy"],
    }
    args.output.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8", newline="\n")
    print(json.dumps({key: summary[key] for key in
                      ["sameReportedConditions", "samePreLaunchExecutableHash", "groups"]}))


if __name__ == "__main__":
    main()
