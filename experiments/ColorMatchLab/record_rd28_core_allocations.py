"""Validate and freeze Debug CRT allocation observations from the Core benchmark."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
RD23 = ROOT / "captures" / "rd23_shadowed_portrait"
OUTPUT = RD23 / "core_outputs"
CAPTURE = ROOT / "captures" / "rd28_core_allocations"
TEST_SOURCE = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE_SOURCE = REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUN_LOG = REPO / "temp" / "rd28_core_test.log"
BUILD_LOG = REPO / "temp" / "rd28_core_build.log"
DIRECT_LOG = CAPTURE / "direct_run.log"
DIRECT_CSV = CAPTURE / "direct_run_allocations.csv"
REPEAT_CSV = CAPTURE / "ctest_replicate_allocations.csv"
METHODS = ("Reinhard", "MeanStddev", "Histogram")
IMAGES = ("portrait_1280x851", "portrait_1536x1024")


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    csv_path = OUTPUT / "allocations.csv"
    expected = {(image, method) for image in IMAGES for method in METHODS}
    samples = []
    for sample_id, sample_path in (("direct_run", DIRECT_CSV),
                                   ("ctest_replicate", REPEAT_CSV)):
        with sample_path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        if len(rows) != 6 or {(row["image"], row["method"]) for row in rows} != expected:
            raise RuntimeError(f"RD28 allocation matrix is incomplete: {sample_id}")
        for row in rows:
            if row["hook"] != "MSVC_Debug_CRT":
                raise RuntimeError(f"Unexpected allocation hook: {row}")
            if int(row["width"]) * int(row["height"]) <= 0:
                raise RuntimeError(f"Invalid RD28 dimensions: {row}")
            if not math.isfinite(float(row["requested_bytes"])):
                raise RuntimeError(f"Invalid allocation byte count: {row}")
            pixels = int(row["width"]) * int(row["height"])
            chunks = (pixels + 4095) // 4096
            if row["method"] == "Histogram":
                expected_count = 12
                expected_bytes = 6 * pixels * 4 + 6 * chunks * 256 * 4
            else:
                expected_count = 18
                expected_bytes = 6 * pixels * 4 + 6 * 2 * chunks * 8
            row["sample_id"] = sample_id
            row["explicit_vector_growth_requests"] = expected_count
            row["explicit_vector_requested_bytes"] = expected_bytes
            row["extra_crt_allocation_requests"] = int(row["allocation_count"]) - expected_count
            row["extra_crt_requested_bytes"] = int(row["requested_bytes"]) - expected_bytes
            samples.append(row)

    ctest_log = RUN_LOG.read_text(encoding="utf-8", errors="replace")
    if "100% tests passed, 0 tests failed out of 1" not in ctest_log:
        raise RuntimeError("RD28 CTest replicate log does not show a passing target")
    direct_log = DIRECT_LOG.read_text(encoding="utf-8", errors="replace")
    if "[  PASSED  ] 12 tests." not in direct_log:
        raise RuntimeError("RD28 direct test log does not show 12/12 passing")
    if sha(csv_path) != sha(REPEAT_CSV):
        raise RuntimeError("Live RD23 allocation CSV does not match saved CTest replicate")
    rd23_manifest_path = RD23 / "manifest.json"
    rd23_manifest = json.loads(rd23_manifest_path.read_text(encoding="utf-8"))
    inputs = {
        "1536_source": RD23 / "source.png",
        "1536_face_mask": RD23 / "face_skin_candidate.png",
        "1280_source": ROOT / "assets" / "WikimediaCC0" / "Pexels-photo-707448-1280.jpg",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    manifest = {
        "rd": "rd28-msvc-debug-core-allocation-observation",
        "status": "single-call Debug CRT allocation diagnostic; no acceptance budget",
        "conditions": {
            "images": [{"name": row["image"], "width": int(row["width"]),
                         "height": int(row["height"])} for row in rows[:3]],
            "methods": list(METHODS), "intensity": 1.0,
            "instrumentation": "MSVC Debug CRT _CrtSetAllocHook counts _HOOK_ALLOC and _HOOK_REALLOC requested bytes during one raw-buffer Core match() call after a method/resolution warm-up",
            "scope": "all Debug CRT allocations observed in the instrumented interval, including any ThreadPool work; input restore, logging, CSV writes and allocation aggregation are outside interval",
            "sampling": "two one-invocation runs for each of six conditions: direct GoogleTest executable and a repeated CTest invocation",
            "attribution": "source-derived explicit vector growth request count and cumulative requested bytes are computed from AutoColorMatch.cpp; residual CRT events are recorded without attributing them to a specific runtime component",
        },
        "input_provenance": {
            "rd23_manifest": str(rd23_manifest_path.relative_to(ROOT)),
            "rd23_manifest_sha256": sha(rd23_manifest_path),
            "rd23_inputs": {name: {"path": str(path.relative_to(ROOT)), "sha256": sha(path)}
                            for name, path in inputs.items()},
        },
        "implementation": {
            "test": "AutoColorMatcherContractTest.CoreImageOverloadMeasuresShadowedPortraitSyntheticCast",
            "test_source_sha256": sha(TEST_SOURCE),
            "core_source_sha256": sha(CORE_SOURCE),
            "allocation_csv": str(csv_path.relative_to(ROOT)),
            "allocation_csv_sha256": sha(csv_path),
            "build_log": str(BUILD_LOG.relative_to(REPO)),
            "build_log_sha256": sha(BUILD_LOG),
            "run_logs": {
                "direct_run": {"path": str(DIRECT_LOG.relative_to(REPO)),
                               "sha256": sha(DIRECT_LOG)},
                "ctest_replicate": {"path": str(RUN_LOG.relative_to(REPO)),
                                    "sha256": sha(RUN_LOG)},
            },
            "allocation_samples": {
                "direct_run": {"path": str(DIRECT_CSV.relative_to(ROOT)),
                               "sha256": sha(DIRECT_CSV)},
                "ctest_replicate": {"path": str(REPEAT_CSV.relative_to(ROOT)),
                                    "sha256": sha(REPEAT_CSV)},
            },
            "configuration": "Visual Studio Debug; VSLANG=1033; focused Core test target",
        },
        "rows": samples,
        "limitations": [
            "Debug CRT hook only; does not observe all OS/runtime or non-CRT allocators",
            "counts allocation request traffic, not peak live bytes, retained memory, or working set",
            "one sample per method/resolution and possible ThreadPool allocations are included",
            "hook overhead invalidates use of this run for timing comparisons",
            "synthetic/raw-buffer Core benchmark is not an application caller or GPU route",
        ],
        "source_derived_vector_growth_requests": {
            "Reinhard": 18, "MeanStddev": 18, "Histogram": 12,
            "note": "explicit vector resize growth requests inferred from source at nonzero equal-dimension inputs; Debug CRT count can also include runtime/parallel work",
        },
        "interpretation": [
            "The observed CRT requested-byte total is close to the explicit vector payload traffic predicted from source; the small residual is not assigned to a component.",
            "Histogram's cumulative requested bytes exceed its explicit peak workspace because its two full-frame channel vectors are rebuilt for each of three channels; cumulative allocation traffic is not peak live memory.",
            "Observed allocation event count exceeds explicit vector growth requests, likely including parallel/runtime requests; this diagnostic does not identify their source.",
        ],
    }
    manifest_path = CAPTURE / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"rows": len(rows), "manifest_sha256": sha(manifest_path),
                      "allocations": [{"sample": row["sample_id"], "image": row["image"], "method": row["method"],
                                       "count": int(row["allocation_count"]),
                                       "requested_bytes": int(row["requested_bytes"]),
                                       "explicit_vector_requested_bytes": row["explicit_vector_requested_bytes"],
                                       "extra_bytes": row["extra_crt_requested_bytes"]}
                                      for row in samples]}, indent=2))


if __name__ == "__main__":
    main()
