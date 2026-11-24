"""Offline reference methods and metrics for the VectorTraceLab R&D.

Methods are mirrors of existing product behavior and simple baselines. They do not
call Artifact binaries and do not modify any product source.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import SEED, SS, Region, rasterize_region

BGS = {
    "black": (0.0, 0.0, 0.0),
    "gray": (0.5, 0.5, 0.5),
    "white": (1.0, 1.0, 1.0),
}


def eps_for_width(w: int) -> float:
    return 1.5 * (w / 512.0)


def min_area_for(w: int, h: int) -> float:
    return 16.0 * (w * h) / (512.0 * 320.0)


def _mask_from_polys(outers, holes, w, h) -> np.ndarray:
    m = np.zeros((h, w), np.uint8)
    for poly in outers:
        cv2.fillPoly(m, [np.asarray(poly, np.int32)], 1)
    for poly in holes:
        cv2.fillPoly(m, [np.asarray(poly, np.int32)], 0)
    return m.astype(bool)


def _mean_color(source: np.ndarray, outer, holes) -> np.ndarray:
    w = source.shape[1]
    h = source.shape[0]
    m = _mask_from_polys([outer], holes, w, h) & (source[..., 3] > 0.5)
    if not m.any():
        return np.array([0.0, 0.0, 0.0], np.float32)
    return source[..., :3][m].mean(axis=0).astype(np.float32)


def _build_traced(contours, hierarchy, source, min_area, eps) -> list[dict]:
    hi = hierarchy[0] if hierarchy is not None else []
    traced = []
    for i, cnt in enumerate(contours):
        if len(hi) and hi[i][3] != -1:      # only top-level contours
            continue
        area = abs(float(cv2.contourArea(cnt)))
        if area < min_area:
            continue
        outer = cv2.approxPolyDP(cnt, eps, True).reshape(-1, 2)
        holes = []
        child = hi[i][2] if len(hi) else -1
        while child != -1:
            hc = contours[child]
            if abs(float(cv2.contourArea(hc))) >= max(1.0, min_area * 0.25):
                holes.append(cv2.approxPolyDP(hc, eps, True).reshape(-1, 2))
            child = hi[child][0]
        traced.append({
            "outer": [outer], "holes": holes,
            "color": _mean_color(source, outer, holes),
            "alpha": 1.0, "area": area,
        })
    return traced


def alpha_threshold_trace(fx, source: np.ndarray) -> list[dict]:
    """Control: single alpha threshold -> RETR_CCOMP contours -> polygon.

    Mirrors the geometry behavior of MaskPath::fromAlphaMask (fixed 0.5
    threshold, RETR_CCOMP hierarchy, approxPolyDP simplification).
    """
    mask = (source[..., 3] >= 0.5).astype(np.uint8) * 255
    contours, hierarchy = cv2.findContours(
        mask, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    return _build_traced(contours, hierarchy, source,
                         min_area_for(fx.w, fx.h), eps_for_width(fx.w))


def posterize_components(fx, source: np.ndarray, k: int | None = None) -> list[dict]:
    """Baseline: color k-means + per-cluster connected components + contours."""
    w, h = fx.w, fx.h
    alpha = source[..., 3]
    valid = alpha > 0.05
    k = k or len(fx.regions)
    data = source[..., :3][valid].reshape(-1, 3).astype(np.float32)
    cv2.setRNGSeed(SEED)
    _, labels, _ = cv2.kmeans(
        data, k, None,
        (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.2),
        3, cv2.KMEANS_PP_CENTERS)
    lab = np.full((h, w), -1, np.int32)
    lab[valid] = labels.ravel()
    traced = []
    eps = eps_for_width(w)
    min_area = min_area_for(w, h)
    for i in range(k):
        m = ((lab == i) & valid).astype(np.uint8) * 255
        m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
        contours, hierarchy = cv2.findContours(
            m, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE)
        traced.extend(_build_traced(contours, hierarchy, source, min_area, eps))
    return traced


def traced_to_arrays(traced, w, h, ss=SS):
    """Rasterize traced regions back to straight-alpha RGBA plus coverage maps."""
    covs = []
    for t in traced:
        m = np.zeros((h * ss, w * ss), np.uint8)
        for poly in t["outer"]:
            pts = np.array([[round(p[0] * ss), round(p[1] * ss)]
                            for p in poly], np.int32)
            cv2.fillPoly(m, [pts], 1, lineType=cv2.LINE_8)
        for poly in t["holes"]:
            pts = np.array([[round(p[0] * ss), round(p[1] * ss)]
                            for p in poly], np.int32)
            cv2.fillPoly(m, [pts], 0, lineType=cv2.LINE_8)
        covs.append(cv2.resize(m.astype(np.float32), (w, h),
                               interpolation=cv2.INTER_AREA))
    if not covs:
        empty = np.zeros((h, w, 4), np.float32)
        return empty, np.zeros((0, h, w), np.float32)
    covs = np.stack(covs).astype(np.float32)
    rgb = np.zeros((h, w, 3), np.float32)
    alpha = np.zeros((h, w), np.float32)
    trans = np.ones((h, w), np.float32)
    for t, cov in zip(traced, covs):
        a = cov * t["alpha"]
        rgb += np.asarray(t["color"], np.float32)[None, None, :] * (a * trans)[..., None]
        alpha += a * trans
        trans = trans * (1.0 - a)
    straight = np.zeros_like(rgb)
    valid = alpha > 1e-6
    straight[valid] = rgb[valid] / alpha[valid, None]
    rgba = np.concatenate([straight, alpha[..., None]], axis=2).astype(np.float32)
    return rgba, covs


def composite(rgba: np.ndarray, bg) -> np.ndarray:
    a = rgba[..., 3:4]
    return rgba[..., :3] * a + np.asarray(bg, np.float32)[None, None, :] * (1 - a)


def delta_e76(c1, c2) -> float:
    a = np.array([[int(np.clip(round(v * 255), 0, 255)) for v in c1]],
                 np.uint8).reshape(1, 1, 3)
    b = np.array([[int(np.clip(round(v * 255), 0, 255)) for v in c2]],
                 np.uint8).reshape(1, 1, 3)
    la = cv2.cvtColor(a, cv2.COLOR_RGB2LAB).astype(np.float32)[0]
    lb = cv2.cvtColor(b, cv2.COLOR_RGB2LAB).astype(np.float32)[0]
    return float(np.linalg.norm(la - lb))


def _boundary(mask: np.ndarray) -> np.ndarray:
    m = mask.astype(np.uint8)
    return cv2.morphologyEx(
        m, cv2.MORPH_GRADIENT, np.ones((3, 3), np.uint8)).astype(bool)


def chamfer_p95(mask_a: np.ndarray, mask_b: np.ndarray) -> float:
    a, b = _boundary(mask_a), _boundary(mask_b)
    if not a.any() or not b.any():
        return float("nan")
    # distance to nearest b pixel: b pixels are zero in the source image
    dt_b = cv2.distanceTransform(np.where(b, 0, 1).astype(np.uint8),
                                 cv2.DIST_L2, 3)
    dt_a = cv2.distanceTransform(np.where(a, 0, 1).astype(np.uint8),
                                 cv2.DIST_L2, 3)
    d = np.concatenate([dt_b[a], dt_a[b]])
    return float(np.percentile(d, 95))


def evaluate(fx, source, gt, traced, method: str) -> dict:
    w, h = fx.w, fx.h
    coverage = gt["coverage"]
    min_area = min_area_for(w, h)

    out_rgba, out_covs = traced_to_arrays(traced, w, h)
    # Ground truth must be the *visible* region (frontmost owner), not the
    # full vector coverage of a back layer that is partly hidden.
    labels = gt["labels"]
    gt_masks = [labels == (i + 1) for i in range(len(fx.regions))]
    out_masks = [c >= 0.5 for c in out_covs]

    # match each ground-truth region to the output region with max IoU
    pairs = []
    for gi, gm in enumerate(gt_masks):
        if gm.sum() < min_area:
            continue
        best_iou, best_oi = 0.0, None
        for oi, om in enumerate(out_masks):
            inter = np.count_nonzero(gm & om)
            union = np.count_nonzero(gm | om)
            iou = inter / union if union else 0.0
            if iou > best_iou:
                best_iou, best_oi = iou, oi
        pairs.append((gi, best_oi, best_iou))

    ious = [p[2] for p in pairs]
    # Geometry/color are only meaningful for a confident match. Unmatched
    # ground-truth regions are reported separately instead of injecting
    # huge boundary distances into the aggregate.
    valid_pairs = [p for p in pairs if p[2] >= 0.5]
    unmatched = [fx.regions[p[0]].name for p in pairs if p[2] < 0.5]
    area_errs = []
    chamfers = []
    color_des = []
    per_region = []
    vertices = sum(len(t["outer"][0]) + sum(len(x) for x in t["holes"])
                   for t in traced) if traced else 0
    for gi, oi, iou in valid_pairs:
        gt_area = float(gt_masks[gi].sum())
        out_area = float(out_masks[oi].sum()) if oi is not None else 0.0
        ch = chamfer_p95(gt_masks[gi], out_masks[oi]) if oi is not None else float("nan")
        de = delta_e76(np.asarray(fx.regions[gi].color, np.float32) / 255.0,
                       traced[oi]["color"]) if oi is not None else float("nan")
        area_errs.append(abs(out_area - gt_area) / max(gt_area, 1.0))
        chamfers.append(ch)
        color_des.append(de)
        per_region.append({
            "gt_region": fx.regions[gi].name, "out_index": oi,
            "iou": iou, "chamfer_p95": ch, "delta_e76": de,
        })

    # hole recovery
    gt_holes, out_holes = [], []
    for gi, region in enumerate(fx.regions):
        for hole in region.holes:
            pts = np.array([[round(x), round(y)] for x, y in hole], np.int32)
            m = np.zeros((h, w), np.uint8)
            cv2.fillPoly(m, [pts], 1)
            gt_holes.append(m.astype(bool))
    for t in traced:
        for hole in t["holes"]:
            pts = np.asarray(hole, np.int32)
            m = np.zeros((h, w), np.uint8)
            cv2.fillPoly(m, [pts], 1)
            out_holes.append(m.astype(bool))
    recovered = 0
    for gh in gt_holes:
        best = max((np.count_nonzero(gh & oh) / max(gh.sum(), 1)
                    for oh in out_holes), default=0.0)
        if best >= 0.5:
            recovered += 1

    # thin-feature recall
    thin = np.zeros((h, w), bool)
    for gi, region in enumerate(fx.regions):
        if region.kind == "thin":
            thin |= gt_masks[gi]
    thin_recall = None
    if thin.any():
        out_union = np.zeros((h, w), bool)
        for om in out_masks:
            out_union |= om
        thin_recall = float(np.count_nonzero(thin & out_union) / thin.sum())

    # alpha / edge composite error
    edge_band = np.zeros((h, w), bool)
    for cov in coverage:
        edge_band |= (cov > 0.02) & (cov < 0.98)
    semi_band = (gt["alpha"] > 0.4) & (gt["alpha"] < 0.6)
    edge_error, semi_error = 0.0, 0.0
    edge_p95, edge_mean, edge_shift1_p95 = 0.0, 0.0, 0.0
    for bg in BGS.values():
        a = composite(source, bg)
        b = composite(out_rgba, bg)
        d = np.abs(a - b)
        if edge_band.any():
            vals = d[edge_band]
            edge_error = max(edge_error, float(vals.max()) * 255.0)
            edge_p95 = max(edge_p95, float(np.percentile(vals, 95)) * 255.0)
            edge_mean = max(edge_mean, float(vals.mean()) * 255.0)
            # A traced polygon is re-rasterized, so a one-pixel phase shift is
            # expected. Gate on the shift-tolerant p95 instead of raw max.
            best = None
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    shifted = np.roll(b, (dy, dx), (0, 1))
                    ds = np.abs(a - shifted)
                    best = ds if best is None else np.minimum(best, ds)
            edge_shift1_p95 = max(
                edge_shift1_p95, float(np.percentile(best[edge_band], 95)) * 255.0)
        if semi_band.any():
            semi_error = max(semi_error, float(d[semi_band].max()) * 255.0)

    finite = bool(np.isfinite(out_rgba).all())

    def agg(vals, fn):
        vals = [v for v in vals if np.isfinite(v)]
        return float(fn(vals)) if vals else None

    return {
        "method": method,
        "fixture": fx.name,
        "region_count": len(traced),
        "gt_region_count": len([m for m in gt_masks if m.sum() >= min_area]),
        "matched_pairs": len(pairs),
        "confident_matches": len(valid_pairs),
        "unmatched_gt_regions": unmatched,
        "per_region": per_region,
        "iou_mean": agg(ious, np.mean),
        "iou_min": agg(ious, np.min),
        "area_error_mean": agg(area_errs, np.mean),
        "boundary_chamfer_p95": agg(chamfers, np.mean),
        "holes_gt": len(gt_holes),
        "holes_recovered": recovered,
        "thin_recall": thin_recall,
        "color_de76_mean": agg(color_des, np.mean),
        "alpha_edge_error_codes": edge_error,
        "alpha_edge_error_p95_codes": edge_p95,
        "alpha_edge_error_mean_codes": edge_mean,
        "alpha_edge_error_shift1_p95_codes": edge_shift1_p95,
        "alpha_semi_error_codes": semi_error,
        "vertices": vertices,
        "finite_output": finite,
    }
