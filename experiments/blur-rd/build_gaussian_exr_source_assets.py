"""Encode canonical Gaussian fixtures as float32 OpenEXR source assets.

The script verifies an exact OpenCV write/read round trip for the payload. It
does not validate Artifact's EXR importer or independently inspect EXR headers.
"""
from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "experiments/blur-rd/fixtures/gaussian-r1"
FIXTURE_MANIFEST = BUNDLE / "manifest.json"
OUT_DIR = BUNDLE / "exr-source"
OUT_MANIFEST = OUT_DIR / "manifest.json"
IMAGE_LAYER_SOURCE = ROOT / "Artifact/src/Layer/ArtifactImageLayer.cpp"
IMAGE_IMPORTER_SOURCE = ROOT / "ArtifactCore/src/IO/Image/ImageImporter.cpp"
ASSET_NAMES = (
    "opaque_asymmetric_impulses",
    "premultiplied_alpha_edge_hdr",
    "transparent_hidden_rgb_checker",
    "opaque_hdr_color_edge",
    "opaque_three_pixel_detail",
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_exr_header(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 8 or data[:4] != bytes((0x76, 0x2F, 0x31, 0x01)):
        raise SystemExit(f"not an OpenEXR file: {path}")
    offset = 8
    attributes: dict[str, tuple[str, bytes]] = {}
    while offset < len(data) and data[offset] != 0:
        name_end = data.find(b"\0", offset)
        if name_end < 0:
            raise SystemExit(f"malformed OpenEXR attribute name: {path}")
        name = data[offset:name_end].decode("ascii")
        type_start = name_end + 1
        type_end = data.find(b"\0", type_start)
        if type_end < 0 or type_end + 5 > len(data):
            raise SystemExit(f"malformed OpenEXR attribute header: {path}")
        attr_type = data[type_start:type_end].decode("ascii")
        size = struct.unpack_from("<I", data, type_end + 1)[0]
        value_start = type_end + 5
        value_end = value_start + size
        if value_end > len(data):
            raise SystemExit(f"truncated OpenEXR attribute: {name}")
        attributes[name] = (attr_type, data[value_start:value_end])
        offset = value_end
    if offset >= len(data) or data[offset] != 0:
        raise SystemExit(f"OpenEXR header terminator missing: {path}")

    channel_attr = attributes.get("channels")
    if not channel_attr or channel_attr[0] != "chlist":
        raise SystemExit(f"OpenEXR channel list missing: {path}")
    channel_data = channel_attr[1]
    channel_names = []
    channel_offset = 0
    while channel_offset < len(channel_data) and channel_data[channel_offset] != 0:
        name_end = channel_data.find(b"\0", channel_offset)
        if name_end < 0 or name_end + 17 > len(channel_data):
            raise SystemExit(f"malformed OpenEXR channel list: {path}")
        channel_names.append(channel_data[channel_offset:name_end].decode("ascii"))
        channel_offset = name_end + 17

    unassociated = attributes.get("oiio:UnassociatedAlpha")
    unassociated_value = None
    if unassociated and unassociated[0] == "int" and len(unassociated[1]) == 4:
        unassociated_value = struct.unpack("<i", unassociated[1])[0]
    return {
        "attribute_names": sorted(attributes),
        "channel_names": channel_names,
        "oiio_unassociated_alpha": unassociated_value,
    }


def main() -> None:
    source_manifest = json.loads(FIXTURE_MANIFEST.read_text(encoding="utf-8"))
    image_layer_text = IMAGE_LAYER_SOURCE.read_text(encoding="utf-8")
    image_importer_text = IMAGE_IMPORTER_SOURCE.read_text(encoding="utf-8")
    import_source_checks = {
        "image_layer_uses_openexr_unassociated_alpha_default": (
            'spec.get_int_attribute("oiio:UnassociatedAlpha", 1) == 0' in image_layer_text
        ),
        "image_layer_import_uses_float_and_named_channel_order": (
            "source.read(readSubimage, 0, true, OIIO::TypeDesc::FLOAT)" in image_layer_text and
            "rgbaChannelOrder(spec)" in image_layer_text and
            "setFromRGBA32F(pixels.data(), spec.width, spec.height)" in image_layer_text
        ),
        "generic_image_importer_reads_exr_float": (
            'suffix == QStringLiteral("exr")' in image_importer_text and
            "OIIO::TypeDesc::FLOAT" in image_importer_text
        ),
        "generic_image_importer_requests_unassociated_alpha": (
            'config.attribute("oiio:UnassociatedAlpha", 1)' in image_importer_text
        ),
    }
    if not all(import_source_checks.values()):
        raise SystemExit(f"image import source check failed: {import_source_checks}")
    entries = {Path(row["path"]).stem: row for row in source_manifest["fixtures"]}
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    assets = []
    for name in ASSET_NAMES:
        source_entry = entries[name]
        source_path = ROOT / source_entry["path"]
        source = np.load(source_path, allow_pickle=False)
        if source.dtype != np.float32 or source.ndim != 3 or source.shape[-1] != 4:
            raise SystemExit(f"invalid canonical fixture: {source_path}")
        if not np.isfinite(source).all():
            raise SystemExit(f"non-finite fixture values: {source_path}")
        source_bytes = source_path.read_bytes()
        if sha(source_bytes) != source_entry["sha256"]:
            raise SystemExit(f"fixture hash differs from source manifest: {source_path}")

        asset_name = name
        asset_pixels = source
        alpha_conversion = "none"
        if name == "premultiplied_alpha_edge_hdr":
            asset_name = "premultiplied_alpha_edge_hdr_straight_alpha"
            asset_pixels = source.copy()
            alpha = source[..., 3:4]
            rgb = asset_pixels[..., :3]
            np.divide(rgb, alpha, out=rgb, where=alpha > np.float32(1e-5))
            rgb[alpha[..., 0] <= np.float32(1e-5)] = 0.0
            if not np.allclose(rgb * alpha, source[..., :3], rtol=1e-6, atol=1e-7):
                raise SystemExit("premultiplied-to-straight conversion changed visible color")
            alpha_conversion = "unpremultiply RGB where alpha>1e-5; set RGB=0 otherwise; preserve alpha"
        asset_npy_path = OUT_DIR / f"{asset_name}.npy"
        np.save(asset_npy_path, asset_pixels, allow_pickle=False)
        exr_path = OUT_DIR / f"{asset_name}.exr"
        params = [
            cv2.IMWRITE_EXR_TYPE, cv2.IMWRITE_EXR_TYPE_FLOAT,
            cv2.IMWRITE_EXR_COMPRESSION, cv2.IMWRITE_EXR_COMPRESSION_NO,
        ]
        if not cv2.imwrite(str(exr_path), asset_pixels, params):
            raise SystemExit(f"OpenCV failed to write EXR: {exr_path}")
        decoded = cv2.imread(str(exr_path), cv2.IMREAD_UNCHANGED)
        if decoded is None or decoded.dtype != np.float32 or decoded.shape != asset_pixels.shape:
            raise SystemExit(f"OpenCV EXR round trip has unexpected shape/type: {exr_path}")
        if not np.isfinite(decoded).all() or not np.array_equal(decoded, asset_pixels):
            max_error = float(np.max(np.abs(decoded.astype(np.float64) - asset_pixels.astype(np.float64))))
            raise SystemExit(f"OpenCV EXR round trip changed pixels: {exr_path}; max abs={max_error}")

        header = read_exr_header(exr_path)
        if sorted(header["channel_names"]) != ["A", "B", "G", "R"]:
            raise SystemExit(f"unexpected OpenEXR channel names: {header['channel_names']}")
        alpha_association = "straight" if name == "premultiplied_alpha_edge_hdr" else source_entry["alpha_association"]
        if alpha_association == "premultiplied":
            raise SystemExit("premultiplied source assets require an explicit EXR alpha association attribute")
        sidecar = {
            "schema": "artifact.blur-rd.exr-source-asset.v1",
            "evidence_class": "synthetic source asset; OpenCV encode/decode verified, Artifact import unverified",
            "fixture_name": name,
            "source_npy": source_entry["path"],
            "source_npy_sha256": source_entry["sha256"],
            "asset_pixel_npy": asset_npy_path.relative_to(ROOT).as_posix(),
            "asset_pixel_npy_sha256": sha(asset_npy_path.read_bytes()),
            "alpha_conversion_from_fixture": alpha_conversion,
            "exr_path": exr_path.relative_to(ROOT).as_posix(),
            "exr_sha256": sha(exr_path.read_bytes()),
            "shape": list(source.shape),
            "dtype": "float32",
            "numeric_roundtrip": {
                "reader": "OpenCV IMREAD_UNCHANGED",
                "exact_pixel_match": True,
                "maximum_absolute_error": 0.0,
                "reader_array_order": "BGRA",
            },
            "parsed_exr_header": header,
            "pixel_contract": {
                "logical_order": "RGBA",
                "canonical_array_order": "BGRA",
                "transfer": "linear synthetic numeric values",
                "primaries": "unspecified synthetic numeric primaries",
                "scene_range": "unclamped; preserve negative and >1 values",
                "authored_alpha_association": alpha_association,
                "alpha_descriptor_validation": (
                    "EXR has no oiio:UnassociatedAlpha marker; ArtifactImageLayer source metadata defaults to unassociated alpha"
                    if header["oiio_unassociated_alpha"] is None else
                    "explicit EXR alpha association marker parsed"
                ),
                "exr_channel_name_validation": "parsed from OpenEXR chlist header; numeric mapping also round-trip checked with OpenCV",
            },
            "artifact_import_status": "pending; compare an asymmetric sentinel and descriptor after import before using as evaluated capture input",
        }
        sidecar_path = exr_path.with_suffix(".json")
        sidecar_path.write_text(json.dumps(sidecar, indent=2, ensure_ascii=False) + "\n",
                                encoding="utf-8", newline="\n")
        assets.append({
            "name": name,
            "exr": exr_path.relative_to(ROOT).as_posix(),
            "asset_pixel_npy": asset_npy_path.relative_to(ROOT).as_posix(),
            "asset_pixel_npy_sha256": sidecar["asset_pixel_npy_sha256"],
            "sidecar": sidecar_path.relative_to(ROOT).as_posix(),
            "sidecar_sha256": sha(sidecar_path.read_bytes()),
            "source_npy_sha256": source_entry["sha256"],
            "exr_sha256": sidecar["exr_sha256"],
            "roundtrip_exact": True,
            "alpha_association": alpha_association,
            "alpha_conversion_from_fixture": alpha_conversion,
            "exr_channel_names": header["channel_names"],
        })

    result = {
        "schema": "artifact.blur-rd.exr-source-asset-manifest.v1",
        "created_date": "2026-11-12",
        "source_fixture_manifest": FIXTURE_MANIFEST.relative_to(ROOT).as_posix(),
        "source_fixture_manifest_sha256": sha(FIXTURE_MANIFEST.read_bytes()),
        "bundle_identity_sha256": source_manifest["bundle_identity_sha256"],
        "writer": {
            "library": "OpenCV",
            "version": cv2.__version__,
            "format": "OpenEXR float32, uncompressed",
            "input_array_order": "BGRA",
        },
        "header_validation": "OpenEXR header parsed locally for channel names and oiio:UnassociatedAlpha; Artifact import behavior remains unverified",
        "artifact_image_import_source": {
            "checks": import_source_checks,
            "files": {
                "image_layer": {"path": IMAGE_LAYER_SOURCE.relative_to(ROOT).as_posix(), "sha256": sha(IMAGE_LAYER_SOURCE.read_bytes())},
                "generic_importer": {"path": IMAGE_IMPORTER_SOURCE.relative_to(ROOT).as_posix(), "sha256": sha(IMAGE_IMPORTER_SOURCE.read_bytes())},
            },
        },
        "assets": assets,
        "limitations": [
            "OpenEXR chlist channel names and the alpha association attribute were parsed; OCIO interpretation and Artifact import/render values remain unverified.",
            "Artifact image import may change channel order, transfer interpretation, or numeric values; compare the imported result with the source sentinel before evaluation.",
            "The premultiplied alpha-edge fixture is converted to straight alpha for the EXR source because the EXR writer cannot author the required association attribute.",
            "transparent_hidden_rgb_checker may be canonicalized during import/render and should be treated as a hidden-RGB stress source, not an expected preserved descriptor.",
        ],
    }
    OUT_MANIFEST.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8", newline="\n")
    print(OUT_MANIFEST)


if __name__ == "__main__":
    main()
