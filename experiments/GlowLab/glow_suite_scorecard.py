"""Consolidate registered Glow-family role and implementation evidence."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DETAILS = ROOT / "experiments/GlowLab/captures/glow-suite-role-detail-matrix-rd1/detail-roi-metrics.json"
PROFILES = ROOT / "experiments/GlowLab/glow_role_candidate_profiles.json"
CPU_DIR = ROOT / "experiments/GlowLab/captures/glow-suite-role-candidates-neon-cpu-rd1"
GPU_DIR = ROOT / "experiments/GlowLab/captures/glow-suite-role-candidates-neon-gpu-rd1"
GPU_SMOKE_DIR = ROOT / "experiments/GlowLab/captures/glow-suite-gpu-smoke-neon-rd1"

EFFECTS = {
    "glow": ("foundation halo", "Broad soft multiscale light around emissive sources",
             "Transfer gain and threshold across exposure/material ranges; candidate is not an accepted preset."),
    "edge_bloom": ("source response", "Retain bright contours and thin emissive lines",
                   "Neon tube ROI clips strongly at broad settings; verify restrained contour visibility."),
    "reactive_glow": ("source response", "Adapt halo tint/strength to luminance and saturation",
                      "Check practical-local clip and tint response after restraint."),
    "chromatic_glow": ("spectral / refractive", "Separate color around bright edges",
                       "Current [0,2] dispersion range has a weak measured signature; setter clamps higher requested values."),
    "liquid_glow": ("spectral / refractive", "Add local refractive waviness around emissive contours",
                    "Test high-key headroom and temporal phase stability."),
    "residual_glow": ("film optics / history", "Carry softened light traces through frame history",
                      "Requires a bounded multi-frame motion and cut review; CPU only."),
    "directional_glow": ("direction / point structure", "Stretch bright sources into directional streaks",
                         "Verify post-alpha-fix GPU behavior and protect high-key bright lines."),
    "physical_halation": ("film optics / history", "Spread bright energy with warm red-biased diffusion",
                          "CPU scratch allocations remain in the hot path; no GPU implementation."),
    "luminescence_caustics": ("spectral / refractive", "Add fine refractive ridge detail around glow",
                              "Check scale aliasing and animated evolution."),
    "builtin.optical_glow": ("film optics / history", "Layer halo, afterglow, streak, atmosphere, and warmth",
                             "Balance component spill and local clip; CPU only."),
    "builtin.volumetric_shine": ("direction / point structure", "Project rays from a localized source point",
                                 "Strength and source alignment remain subtle on some materials."),
    "builtin.glint_star_filter": ("direction / point structure", "Shape compact highlights into star glints",
                                  "Review restrained intensity, local clipping, and GPU availability; CPU only."),
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def gpu_paths(effect_id: str) -> tuple[Path, Path]:
    key = effect_id.replace(".", "_")
    if effect_id == "glow":
        return GPU_SMOKE_DIR / "glow-glow-gpu.png", GPU_SMOKE_DIR / "glow-glow-gpu.png.state.txt"
    output = GPU_DIR / f"gpu-role-{key}-{effect_id}-gpu.png"
    return output, Path(str(output) + ".state.txt")


def run(output_dir: Path) -> None:
    detail = json.loads(DETAILS.read_text(encoding="utf-8"))
    profile_doc = json.loads(PROFILES.read_text(encoding="utf-8"))
    rows = detail.get("records", [])
    effect_ids = list(EFFECTS)
    plate_names = sorted({str(row["plate"]) for row in rows})

    # Establish one common source hash per plate before aggregating any metrics.
    source_hashes: dict[str, set[str]] = {plate: set() for plate in plate_names}
    for row in rows:
        source_hashes[str(row["plate"])].add(str(row["input_sha256"]))
    if any(len(hashes) != 1 for hashes in source_hashes.values()):
        raise RuntimeError("Detail matrix contains mismatched inputs within a plate")

    scorecard: list[dict[str, object]] = []
    for effect_id, (group, role, open_gate) in EFFECTS.items():
        effect_rows = [row for row in rows if row["effect_id"] == effect_id]
        seen_plates = {str(row["plate"]) for row in effect_rows}
        if seen_plates != set(plate_names):
            raise RuntimeError(f"Missing detail-matrix plate for {effect_id}: {seen_plates}")
        per_plate: dict[str, object] = {}
        for plate in plate_names:
            plate_rows = [row for row in effect_rows if row["plate"] == plate]
            per_plate[plate] = {
                "roi_count": len(plate_rows),
                "max_roi_new_clipped_channel_fraction": max(
                    float(row["newly_clipped_channel_fraction"]) for row in plate_rows),
                "max_roi_mean_linear_luma_delta": max(
                    abs(float(row["linear_luma_delta_mean"])) for row in plate_rows),
                "all_alpha_mae_zero": all(float(row["alpha_mae"]) == 0.0 for row in plate_rows),
            }

        key = effect_id.replace(".", "_")
        cpu_state_path = CPU_DIR / f"app-role-{key}-{effect_id}-cpu.png.state.txt"
        cpu_state = read_text(cpu_state_path)
        if "compute=0" not in cpu_state or "captureSource=Installed CPU reference" not in cpu_state:
            raise RuntimeError(f"Missing installed CPU evidence for {effect_id}")
        cpu_gpu_declared = "supportsGPU=1" in cpu_state
        gpu_output, gpu_state_path = gpu_paths(effect_id)
        gpu_state = read_text(gpu_state_path)
        gpu_capture = bool(
            gpu_output.exists() and "compute=1" in gpu_state and
            "supportsGPU=1" in gpu_state and
            "captureSource=Accum RT" in gpu_state)
        if cpu_gpu_declared and not gpu_capture:
            raise RuntimeError(f"Declared GPU effect has no validated diagnostic capture: {effect_id}")
        spatial_node_count = None
        if gpu_capture and "gpuNodeCount=" in gpu_state:
            try:
                spatial_node_count = int(gpu_state.split("gpuNodeCount=", 1)[1].split()[0])
            except ValueError as exc:
                raise RuntimeError(f"Malformed spatial GPU node count: {gpu_state_path}") from exc
        profile = profile_doc["profiles"][effect_id]
        scorecard.append({
            "effect_id": effect_id,
            "suite_group": group,
            "intended_visual_role": role,
            "candidate_settings": profile,
            "candidate_acceptance": "pending human aesthetic review; not a product preset",
            "cpu_installed_capture": True,
            "cpu_alpha_preserved_in_all_fixed_rois": all(
                item["all_alpha_mae_zero"] for item in per_plate.values()),
            "gpu_declared_supported": cpu_gpu_declared,
            "gpu_diagnostic_capture_available": gpu_capture,
            "gpu_spatial_node_count": spatial_node_count,
            "gpu_capture_source_class": "rescaled Accum RT diagnostic preview" if gpu_capture else "no GPU implementation",
            "common_input_sha256_by_plate": {
                plate: next(iter(hashes)) for plate, hashes in source_hashes.items()},
            "fixed_roi_evidence_by_plate": per_plate,
            "open_gate": open_gate,
        })

    output_dir.mkdir(parents=True, exist_ok=True)
    json_path = output_dir / "glow-suite-scorecard.json"
    json_path.write_text(json.dumps({
        "intent": "evidence index for the distinct registered Glow-family suite; not a quality ranking",
        "evidence_source": str(DETAILS.relative_to(ROOT)),
        "effect_count": len(scorecard),
        "plate_count": len(plate_names),
        "plates": {
            plate: {"input_sha256": next(iter(hashes))} for plate, hashes in source_hashes.items()},
        "gpu_parity_status": "not proven; GPU role captures use rescaled Accum RT previews",
        "effects": scorecard,
        "suite_level_open_gates": [
            "visual aesthetic acceptance is pending for every role candidate",
            "same-coordinate CPU/GPU effect output comparison is not yet available",
            "float-preserving ordered-stack and final Composition Viewer evidence remain open",
            "HDR, temporal, and steady-state performance/allocation evidence are incomplete by family",
        ],
    }, indent=2) + "\n", encoding="utf-8")

    csv_path = output_dir / "glow-suite-scorecard.csv"
    columns = ("effect_id", "suite_group", "intended_visual_role", "cpu_alpha_preserved_in_all_fixed_rois",
               "gpu_declared_supported", "gpu_diagnostic_capture_available", "gpu_spatial_node_count",
               "gpu_capture_source_class",
               "neon_max_roi_clip", "portrait_face_max_roi_delta_luma", "walkway_highkey_max_roi_clip",
               "open_gate", "candidate_acceptance")
    with csv_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for item in scorecard:
            by_plate = item["fixed_roi_evidence_by_plate"]
            writer.writerow({
                "effect_id": item["effect_id"],
                "suite_group": item["suite_group"],
                "intended_visual_role": item["intended_visual_role"],
                "cpu_alpha_preserved_in_all_fixed_rois": item["cpu_alpha_preserved_in_all_fixed_rois"],
                "gpu_declared_supported": item["gpu_declared_supported"],
                "gpu_diagnostic_capture_available": item["gpu_diagnostic_capture_available"],
                "gpu_spatial_node_count": item["gpu_spatial_node_count"],
                "gpu_capture_source_class": item["gpu_capture_source_class"],
                "neon_max_roi_clip": by_plate["neon"]["max_roi_new_clipped_channel_fraction"],
                "portrait_face_max_roi_delta_luma": by_plate["portrait"]["max_roi_mean_linear_luma_delta"],
                "walkway_highkey_max_roi_clip": by_plate["walkway_highkey"]["max_roi_new_clipped_channel_fraction"],
                "open_gate": item["open_gate"],
                "candidate_acceptance": item["candidate_acceptance"],
            })

    print(f"Saved {json_path}")
    print(f"Saved {csv_path}")
    print(f"Validated {len(scorecard)} effects across {len(plate_names)} matched plate matrices; "
          f"GPU diagnostic captures: {sum(bool(item['gpu_diagnostic_capture_available']) for item in scorecard)}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)


if __name__ == "__main__":
    main()
