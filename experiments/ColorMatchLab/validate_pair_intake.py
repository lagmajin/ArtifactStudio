"""Preflight a filled color-match pair intake before any candidate scoring."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_text(errors: list[str], value: object, label: str) -> None:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"missing: {label}")


def require_object(errors: list[str], value: object, label: str) -> dict:
    if not isinstance(value, dict):
        errors.append(f"must be an object: {label}")
        return {}
    return value


def check_image(errors: list[str], repo: Path, value: object, label: str) -> Path | None:
    record = require_object(errors, value, label)
    relative = record.get("file", "")
    digest = record.get("sha256", "")
    if not isinstance(relative, str) or not relative.strip():
        errors.append(f"missing: {label}.file")
        return None
    path = (repo / relative).resolve()
    if not path.is_relative_to(repo.resolve()):
        errors.append(f"outside workspace: {label}.file")
        return None
    if not path.is_file():
        errors.append(f"not found: {label}.file ({relative})")
        return None
    if not digest or sha(path) != digest:
        errors.append(f"sha256 missing or mismatched: {label}")
        return None
    try:
        with Image.open(path) as image:
            actual = [image.width, image.height]
            image.verify()
    except Exception as error:  # Pillow reports format-specific decoding errors.
        errors.append(f"cannot decode {label}: {error}")
        return None
    if record.get("dimensions") != actual:
        errors.append(f"dimension mismatch: {label} records {record.get('dimensions')} but file is {actual}")
    require_text(errors, record.get("color_profile_or_assumption"), f"{label}.color_profile_or_assumption")
    require_text(errors, record.get("alpha_association_or_not_applicable"), f"{label}.alpha_association_or_not_applicable")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("intake", nargs="?", type=Path,
                        default=Path(__file__).with_name("PAIR_INTAKE_TEMPLATE.json"))
    args = parser.parse_args()
    intake_path = args.intake.resolve()
    repo = Path(__file__).resolve().parents[2]
    try:
        data = json.loads(intake_path.read_text(encoding="utf-8"))
    except Exception as error:
        print(f"HOLD: cannot read intake: {error}")
        return 2
    if not isinstance(data, dict):
        print("HOLD: intake root must be a JSON object")
        return 2

    rights_errors: list[str] = []
    rights = require_object(rights_errors, data.get("rights"), "rights")
    for key in ("rights_holder_or_source", "license_or_written_permission",
                "evidence_location_or_url", "reviewed_by", "review_date"):
        require_text(rights_errors, rights.get(key), f"rights.{key}")
    for key in ("internal_product_rd_allowed", "image_processing_and_measurement_allowed",
                "derived_crops_masks_and_metrics_storage_allowed"):
        if rights.get(key) is not True:
            rights_errors.append(f"rights permission not confirmed: rights.{key}")
    require_text(rights_errors, rights.get("people_identifiable_or_consent_basis"),
                 "rights.people_identifiable_or_consent_basis")

    if rights_errors:
        print("HOLD: rights and authorization preflight is incomplete; image files were not opened")
        for error in rights_errors:
            print(f"- {error}")
        return 2

    errors: list[str] = []
    if data.get("template") != "color-match-pair-intake-v2":
        errors.append("template must be color-match-pair-intake-v2")
    if data.get("status") != "ready_for_preflight":
        errors.append("status must be explicitly set to ready_for_preflight")
    require_text(errors, data.get("provided_by"), "provided_by")
    require_text(errors, data.get("pair_id"), "pair_id")
    require_text(errors, rights.get("reviewed_by"), "rights.reviewed_by")
    require_text(errors, rights.get("review_date"), "rights.review_date")

    source_record = require_object(errors, data.get("source"), "source")
    reference = require_object(errors, data.get("reference"), "reference")
    source_path = check_image(errors, repo, source_record, "source")
    reference_path = check_image(errors, repo, reference, "reference")
    dimensions_match = True
    if source_path and reference_path:
        with Image.open(source_path) as source_image, Image.open(reference_path) as reference_image:
            dimensions_match = source_image.size == reference_image.size

    if reference.get("role") not in {"neutral_wb", "measured_target", "camera_render", "user_preference", "other"}:
        errors.append("reference.role is not a recognized role")
    require_text(errors, reference.get("role_evidence"), "reference.role_evidence")

    pair = require_object(errors, data.get("capture_pair"), "capture_pair")
    for key in ("same_scene_and_capture_or_raw_derived_renditions", "camera_and_lens",
                "lighting_and_mixed_light_notes", "exposure_and_tone_curve_match",
                "white_balance_difference", "registration_or_pixel_shift_notes",
                "other_processing_differences"):
        require_text(errors, pair.get(key), f"capture_pair.{key}")
    if not dimensions_match:
        if pair.get("registration_mapping_resolved") is not True:
            errors.append("source/reference dimensions differ and registration mapping is not marked resolved")
        require_text(errors, pair.get("registration_mapping_evidence"),
                     "capture_pair.registration_mapping_evidence")

    for label, record in (("source", source_record), ("reference", reference)):
        require_text(errors, record.get("format"), f"{label}.format")
    if source_record.get("format") != reference.get("format"):
        errors.append("source/reference formats differ; document the decode and normalization path before scoring")

    regions = require_object(errors, data.get("regions"), "regions")
    for key in ("skin_roi_mask_or_coordinates", "neutral_or_white_roi_mask_or_coordinates",
                "neutral_or_white_measurement_or_material_evidence", "roi_reviewer"):
        require_text(errors, regions.get(key), f"regions.{key}")
    if regions.get("skin_roi_reviewed_before_candidate_outputs") is not True:
        errors.append("skin ROI must be reviewed before candidate outputs")

    evaluation = require_object(errors, data.get("evaluation"), "evaluation")
    matcher_route = evaluation.get("matcher_input_route")
    if matcher_route not in ("same_size_image_overload", "explicit_dimensions_algorithm"):
        errors.append("evaluation.matcher_input_route must select a supported, dimension-safe route")
    else:
        require_text(errors, evaluation.get("matcher_input_route_evidence"),
                     "evaluation.matcher_input_route_evidence")
        if not dimensions_match and matcher_route != "explicit_dimensions_algorithm":
            errors.append("differently sized inputs require explicit_dimensions_algorithm; the convenience dispatcher reuses source dimensions for reference")
    for key in ("display_conditions", "reviewers_and_roles", "blind_or_randomized_review_protocol",
                "agreed_color_tolerance_and_measurement_space", "skin_hue_chroma_and_detail_limits",
                "neutral_white_limits", "critical_visual_defects",
                "reviewer_disagreement_resolution"):
        require_text(errors, evaluation.get(key), f"evaluation.{key}")
    if evaluation.get("acceptance_thresholds_agreed_before_candidate_outputs") is not True:
        errors.append("acceptance thresholds must be agreed before candidate outputs")
    if evaluation.get("allowed_to_run_authorized_artifactcore_tests") is not True:
        errors.append("Core test execution is not authorized in this intake")

    if errors:
        print("HOLD: candidate scoring is not ready")
        for error in errors:
            print(f"- {error}")
        return 2
    print("READY FOR HUMAN REVIEW: required fields and file hashes are present; verify rights evidence, ROI correctness, registration, and pre-output gates before scoring")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
