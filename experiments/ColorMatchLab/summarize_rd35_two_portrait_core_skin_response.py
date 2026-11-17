"""Summarize matched descriptive Core-preview face rows for RD32/33/34."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURES = ROOT / "captures"
RD32 = CAPTURES / "rd32_core_portrait_skin_color_response"
RD33 = CAPTURES / "rd33_portrait_skin_lightness_response"
RD34 = CAPTURES / "rd34_second_portrait_core_skin_response"
OUT = CAPTURES / "rd35_two_portrait_core_skin_response"
METHODS = ("Reinhard", "MeanStddev", "Histogram")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def load_unique(path: Path, *, route: str, region: str, method: str) -> dict:
    matches = [row for row in rows(path)
               if row["route"] == route and row["region"] == region
               and row["method"] == method and float(row["strength"]) == 1.0]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one matched row in {path}, found {len(matches)}")
    return matches[0]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    manifests = {
        "rd32": json.loads((RD32 / "manifest.json").read_text(encoding="utf-8")),
        "rd33": json.loads((RD33 / "manifest.json").read_text(encoding="utf-8")),
        "rd34": json.loads((RD34 / "manifest.json").read_text(encoding="utf-8")),
    }
    upstream_csvs = {
        "rd32": RD32 / "color_response.csv",
        "rd33": RD33 / "lightness_response.csv",
        "rd34": RD34 / "skin_response.csv",
    }
    for name, path in upstream_csvs.items():
        if sha(path) != manifests[name]["csv_sha256"]:
            raise RuntimeError(f"{name} CSV differs from its capture manifest")
    source_hashes = {
        "portrait_rd9": manifests["rd32"]["source"]["sha256"],
        "portrait_rd23": manifests["rd34"]["source"]["sha256"],
    }
    mask_hashes = {
        "portrait_rd9": manifests["rd32"]["regions"]["face_skin_candidate"]["sha256"],
        "portrait_rd23": manifests["rd34"]["region"]["sha256"],
    }
    if manifests["rd33"]["source"]["sha256"] != source_hashes["portrait_rd9"]:
        raise RuntimeError("RD33 and RD32 do not refer to the same first-portrait baseline")
    if manifests["rd33"]["regions"]["face_skin_candidate"]["sha256"] != mask_hashes["portrait_rd9"]:
        raise RuntimeError("RD33 and RD32 do not refer to the same first-portrait face mask")

    result = []
    for method in METHODS:
        first_color = load_unique(RD32 / "color_response.csv", route="actual_core_preview",
                                  region="face_skin_candidate", method=method)
        first_light = load_unique(RD33 / "lightness_response.csv", route="actual_core_preview",
                                  region="face_skin_candidate", method=method)
        second = [row for row in rows(RD34 / "skin_response.csv")
                  if row["method"] == method and float(row["strength"]) == 1.0]
        if len(second) != 1:
            raise RuntimeError(f"Expected one RD34 strength-1 row for {method}")
        second = second[0]
        if first_color["input_sha256"] != manifests["rd32"]["outputs"].get(
                f"actual_core_preview/{first_color['input_file']}"):
            raise RuntimeError("RD32 input row differs from its capture manifest")
        if first_light["input_sha256"] != manifests["rd33"]["outputs"].get(
                f"actual_core_preview/{first_light['input_file']}"):
            raise RuntimeError("RD33 input row differs from its capture manifest")
        if second["input_sha256"] != manifests["rd34"]["outputs"].get(second["input_file"]):
            raise RuntimeError("RD34 input row differs from its capture manifest")
        for file_path, expected_hash in (
            (CAPTURES / "rd22_core_outputs" / first_color["input_file"],
             first_color["input_sha256"]),
            (CAPTURES / "rd22_core_outputs" / first_light["input_file"],
             first_light["input_sha256"]),
            (CAPTURES / "rd23_shadowed_portrait" / "core_outputs" / second["input_file"],
             second["input_sha256"]),
        ):
            if sha(file_path) != expected_hash:
                raise RuntimeError(f"Selected source image changed: {file_path}")

        for portrait, color_row, light_row, src_hash, mask_hash, count in (
            ("RD9_CC0_1280x851", first_color, first_light,
             source_hashes["portrait_rd9"], mask_hashes["portrait_rd9"],
             int(manifests["rd32"]["regions"]["face_skin_candidate"]["pixels"])),
            ("RD23_CC0_1536x1024_shadowed", second, second, 
             source_hashes["portrait_rd23"], mask_hashes["portrait_rd23"],
             int(manifests["rd34"]["region"]["pixels"])),
        ):
            color_metrics = json.loads(color_row["deltaE76_to_saved_core_source"]
                if "deltaE76_to_saved_core_source" in color_row else color_row["deltaE76_to_source"])
            hue_metrics = json.loads(color_row["absolute_hue_delta_deg"])
            delta_l = json.loads(light_row["signed_delta_Lstar"])
            absolute_l = json.loads(light_row["absolute_delta_Lstar"])
            result.append({
                "portrait": portrait, "method": method, "strength": 1.0,
                "region": "face_skin_candidate", "pixels": count,
                "source_sha256": src_hash, "mask_sha256": mask_hash,
                "deltaE76_p50": color_metrics["p50"],
                "deltaE76_p95": color_metrics["p95"],
                "absolute_hue_delta_deg_p50": hue_metrics["p50"],
                "absolute_hue_delta_deg_p95": hue_metrics["p95"],
                "signed_delta_Lstar_p50": delta_l["p50"],
                "absolute_delta_Lstar_p95": absolute_l["p95"],
                "endpoint_pixel_fraction": color_row.get("output_endpoint_pixel_fraction", ""),
            })

    csv_path = OUT / "two_portrait_summary.csv"
    fields = list(result[0])
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(result)
    manifest = {
        "rd": "rd35-two-portrait-saved-core-face-response-summary",
        "status": "retrospective descriptive cross-fixture summary; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd35-two-portrait-saved-core-face-response-summary-predeclared-2026-11-17",
        "upstream_manifests": {name: sha(folder / "manifest.json")
                               for name, folder in (("rd32", RD32), ("rd33", RD33), ("rd34", RD34))},
        "upstream_csvs": {name: {"file": path.name, "sha256": sha(path)}
                          for name, path in upstream_csvs.items()},
        "analysis_script_sha256": sha(Path(__file__)),
        "source_hashes": source_hashes, "mask_hashes": mask_hashes,
        "conditions": {"route": "actual_core_preview", "region": "face_skin_candidate",
                       "strength": 1.0, "methods": list(METHODS)},
        "limitations": [
            "two unpaired CC0 portraits with different lighting, rendition and ROI",
            "single-review candidate masks; not independently annotated",
            "retrospective 8-bit Core preview measurements, not float output",
            "cross-portrait differences are confounded and not method effects or population generalization",
            "no measured WB target, neutral-white ROI, ranking, threshold, or acceptance gate",
        ],
        "row_count": len(result), "csv_sha256": sha(csv_path),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(result),
                      "csv_sha256": manifest["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
