"""Independent geometric ray trace of LensFlareLab's sequential prescription.

The fixture uses the bundled prescription, its Cauchy dispersion approximation,
physical six-blade stop, and sensor plane. It evaluates geometrical spot maps;
it does not claim measured glass, coating phase, or exit-pupil OPD accuracy.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import gaussian_filter


SURFACE_DATA = (
    (164.13, 10.99, 1.6751, 32.2, 27.0), (559.20, 0.23, 1.0, 0.0, 27.0),
    (100.12, 11.45, 1.6689, 47.1, 25.5), (213.54, 0.23, 1.0, 0.0, 25.5),
    (58.04, 22.95, 1.6913, 54.7, 20.5), (2551.10, 2.58, 1.6751, 32.2, 20.5),
    (32.39, 15.66, 1.0, 0.0, 13.5), (-40.42, 2.74, 1.6992, 30.1, 12.5),
    (192.98, 27.92, 1.6204, 60.3, 18.0), (-55.53, 0.23, 1.0, 0.0, 18.0),
    (192.98, 7.98, 1.6913, 54.7, 17.5), (-225.30, 0.23, 1.0, 0.0, 17.5),
    (175.08, 8.48, 1.6913, 54.7, 17.5), (-203.55, 55.742, 1.0, 0.0, 17.5),
)
EFL = 100.0
SENSOR_WIDTH = 53.96
STOP_RADIUS = EFL / 22.0
SENSOR_PIXEL_PITCH_UM = 5.0


def make_surfaces():
    result = []
    z = 0.0
    for i, (radius, thickness, nd, abbe, semi_aperture) in enumerate(SURFACE_DATA):
        if i:
            z += SURFACE_DATA[i - 1][1]
            if i == 7:
                z += 15.0
        result.append((radius, z, nd, abbe, semi_aperture))
    return result


SURFACES = make_surfaces()
IRIS_Z = SURFACES[6][1] + SURFACE_DATA[6][1]
SENSOR_Z = SURFACES[-1][1] + SURFACE_DATA[-1][1]
FIELD_CASES = (("on_axis", 0.0), ("field_5deg", 5.0),
               ("field_10deg", 10.0), ("field_12deg", 12.0))


def refractive_index(surface, wavelength_nm):
    _, _, nd, abbe, _ = surface
    if abbe <= 0.0:
        return 1.0
    slope = ((nd - 1.0) / abbe) * 0.52345
    offset = nd - slope * 2.897
    wavelength_um = wavelength_nm * 1e-3
    return offset + slope / wavelength_um**2


def intersect(origin, direction, surface):
    radius, z, _, _, aperture = surface
    if abs(radius) > 1e6:
        if abs(direction[2]) < 1e-12:
            return None
        t = (z - origin[2]) / direction[2]
    else:
        center = np.array((0.0, 0.0, z + radius))
        oc = origin - center
        b = float(np.dot(oc, direction))
        c = float(np.dot(oc, oc) - radius * radius)
        disc = b * b - c
        if disc < 0.0:
            return None
        root = math.sqrt(disc)
        positive = [v for v in (-b - root, -b + root) if v > 1e-8]
        if not positive:
            return None
        t = min(positive)
    if t <= 1e-8 or not math.isfinite(t):
        return None
    point = origin + t * direction
    if math.hypot(point[0], point[1]) > aperture:
        return None
    return point


def refract(direction, point, surface, n_before, n_after):
    radius, z = surface[:2]
    if abs(radius) > 1e6:
        normal = np.array((0.0, 0.0, -1.0))
    else:
        normal = point - np.array((0.0, 0.0, z + radius))
        normal /= np.linalg.norm(normal)
    if np.dot(direction, normal) > 0.0:
        normal = -normal
    cos1 = float(np.clip(-np.dot(normal, direction), 0.0, 1.0))
    eta = n_before / n_after
    k = 1.0 - eta * eta * (1.0 - cos1 * cos1)
    if k < 0.0:
        return None
    result = eta * direction + (eta * cos1 - math.sqrt(k)) * normal
    return result / np.linalg.norm(result)


def in_hexagon(x, y, radius):
    sector = 2.0 * math.pi / 6.0
    angle = math.atan2(y, x)
    local = math.remainder(angle, sector)
    return math.hypot(x, y) * math.cos(local) <= radius * math.cos(math.pi / 6.0)


def trace_front_to_stop(origin, direction, wavelength_nm):
    """Trace through the front group and return the physical stop intercept."""
    origin = np.asarray(origin, dtype=np.float64).copy()
    direction = direction.copy()
    for i, surface in enumerate(SURFACES[:7]):
        point = intersect(origin, direction, surface)
        if point is None:
            return None
        n1 = 1.0 if i == 0 else refractive_index(SURFACES[i - 1], wavelength_nm)
        n2 = refractive_index(surface, wavelength_nm)
        direction = refract(direction, point, surface, n1, n2)
        if direction is None:
            return None
        origin = point
    t = (IRIS_Z - origin[2]) / direction[2]
    if t <= 0.0:
        return None
    return origin + t * direction


def chief_ray(field_angle, wavelength_nm):
    """Solve for the ray through the stop center and return its image direction."""
    direction = np.array((math.sin(field_angle), 0.0, math.cos(field_angle)))
    launch_xy = np.array((-(IRIS_Z + 100.0) * math.tan(field_angle), 0.0))

    def hit(candidate):
        return trace_front_to_stop((candidate[0], candidate[1], -100.0),
                                   direction, wavelength_nm)

    for _ in range(12):
        iris = hit(launch_xy)
        if iris is None:
            raise RuntimeError(f"chief ray blocked before stop at {math.degrees(field_angle):.2f} deg")
        error = iris[:2]
        if np.linalg.norm(error) < 1e-7:
            break
        step = 1e-3
        plus_x = hit(launch_xy + (step, 0.0))
        plus_y = hit(launch_xy + (0.0, step))
        if plus_x is None or plus_y is None:
            raise RuntimeError("chief-ray finite-difference trace was vignetted")
        jx = (plus_x[:2] - iris[:2]) / step
        jy = (plus_y[:2] - iris[:2]) / step
        jacobian = np.column_stack((jx, jy))
        launch_xy -= np.linalg.solve(jacobian, error)
    if np.linalg.norm(hit(launch_xy)[:2]) > 1e-5:
        raise RuntimeError(f"chief ray failed to converge at {math.degrees(field_angle):.2f} deg")
    return np.array((launch_xy[0], launch_xy[1], -100.0)), direction


def trace_transmission(launch_origin, direction, wavelength_nm):
    """Trace one parallel ray through the full prescription and stop."""
    origin = np.asarray(launch_origin, dtype=np.float64).copy()
    direction = direction.copy()
    for i, surface in enumerate(SURFACES):
        if i == 7:
            t = (IRIS_Z - origin[2]) / direction[2]
            iris = origin + t * direction
            if t <= 0.0 or not in_hexagon(iris[0], iris[1], STOP_RADIUS):
                return None
            origin = iris
        point = intersect(origin, direction, surface)
        if point is None:
            return None
        n1 = 1.0 if i == 0 else refractive_index(SURFACES[i - 1], wavelength_nm)
        n2 = refractive_index(surface, wavelength_nm)
        direction = refract(direction, point, surface, n1, n2)
        if direction is None:
            return None
        origin = point
    if direction[2] <= 1e-10:
        return None
    sensor = origin + ((SENSOR_Z - origin[2]) / direction[2]) * direction
    return sensor[:2], direction


def trace_field(field_angle, wavelength_nm, samples):
    direction = np.array((math.sin(field_angle), 0.0, math.cos(field_angle)))
    chief_origin, _ = chief_ray(field_angle, wavelength_nm)
    chief_trace = trace_transmission(chief_origin, direction, wavelength_nm)
    if chief_trace is None:
        raise RuntimeError("chief ray is blocked by the physical stop")
    chief_angle = math.acos(float(np.clip(chief_trace[1][2], -1.0, 1.0)))
    # Sample equal areas on a plane perpendicular to the collimated bundle so
    # pupil clipping is measured separately from cos^4 radiometric roll-off.
    transverse_x = np.array((math.cos(field_angle), 0.0, -math.sin(field_angle)))
    transverse_y = np.array((0.0, 1.0, 0.0))
    extent = 35.0
    grid = np.linspace(-extent, extent, samples, endpoint=False) + extent / samples
    spots = []
    for y in grid:
        for x in grid:
            origin = chief_origin + x * transverse_x + y * transverse_y
            traced = trace_transmission(origin, direction, wavelength_nm)
            if traced is None:
                continue
            sensor, _ = traced
            spots.append(sensor)
    spot_array = np.asarray(spots, dtype=np.float64).reshape(-1, 2)
    return spot_array, chief_angle


def metrics(points):
    if len(points) == 0:
        return {k: math.nan for k in ("rms_radius_um", "r50_um", "r90_um", "r99_um",
                                       "centroid_x_um", "centroid_y_um")}
    center = np.median(points, axis=0)
    radii = np.linalg.norm(points - center, axis=1) * 1000.0
    return {
        "rms_radius_um": float(np.sqrt(np.mean(radii * radii))),
        "r50_um": float(np.quantile(radii, 0.50)), "r90_um": float(np.quantile(radii, 0.90)),
        "r99_um": float(np.quantile(radii, 0.99)),
        "centroid_x_um": float(np.mean(points[:, 0] - center[0]) * 1000.0),
        "centroid_y_um": float(np.mean(points[:, 1] - center[1]) * 1000.0),
    }


def pixel_psf_metrics(points, pitch_um=SENSOR_PIXEL_PITCH_UM):
    """Return energy radii of equal-weight rays deposited into square pixels."""
    if len(points) == 0:
        return {"pixel_r50_um": math.nan, "pixel_r80_um": math.nan,
                "pixel_r90_um": math.nan, "pixel_pitch_um": pitch_um}
    center = np.median(points, axis=0)
    pixel_indices = np.floor((points - center) * (1000.0 / pitch_um) + 0.5).astype(np.int64)
    unique_pixels, counts = np.unique(pixel_indices, axis=0, return_counts=True)
    pixel_centers = unique_pixels * (pitch_um / 1000.0)
    pixel_radii_um = np.linalg.norm(pixel_centers, axis=1) * 1000.0
    energy_order = np.argsort(pixel_radii_um, kind="stable")
    cumulative = np.cumsum(counts[energy_order]) / len(points)
    result = {}
    for fraction in (0.50, 0.80, 0.90):
        result[f"pixel_r{int(fraction * 100)}_um"] = float(
            pixel_radii_um[energy_order[np.searchsorted(cumulative, fraction)]])
    result["pixel_pitch_um"] = pitch_um
    return result


def make_pixel_psf_chart(clouds, output, pitch_um=SENSOR_PIXEL_PITCH_UM):
    """Visualize ray-count PSFs on one shared, explicitly non-diffractive grid."""
    fields = tuple(name for name, _ in FIELD_CASES)
    wavelengths = (610.0, 550.0, 460.0)
    colors = {610.0: (255, 112, 89), 550.0: (132, 222, 255),
              460.0: (175, 255, 137)}
    centered_clouds = {}
    radii_um = []
    for name in fields:
        points = [clouds[(name, wavelength)] for wavelength in wavelengths
                  if len(clouds[(name, wavelength)])]
        center = np.median(np.concatenate(points), axis=0)
        centered_clouds[name] = {
            wavelength: clouds[(name, wavelength)] - center for wavelength in wavelengths
        }
        radii_um.extend(np.linalg.norm(points, axis=1) * 1000.0
                        for points in centered_clouds[name].values() if len(points))
    radius_um = max(float(np.quantile(np.concatenate(radii_um), .999)), pitch_um)
    half_span_um = math.ceil(radius_um * 1.08 / pitch_um) * pitch_um
    half_span_mm = half_span_um / 1000.0
    bins = max(1, int(round(2.0 * half_span_um / pitch_um)))
    panel_size = 384
    panel_width, panel_height, gap = 430, 492, 12
    width = len(fields) * panel_width + (len(fields) - 1) * gap
    image = Image.new("RGB", (width, panel_height + 52), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    channel_index = {610.0: 0, 550.0: 1, 460.0: 2}
    for col, (name, angle_deg) in enumerate(FIELD_CASES):
        left = col * (panel_width + gap)
        draw.text((left + 6, 5), f"{name} ({angle_deg:.1f} deg)", fill=(235, 238, 244))
        rgb = np.zeros((bins, bins, 3), dtype=np.float64)
        for wavelength in wavelengths:
            points = centered_clouds[name][wavelength]
            if not len(points):
                continue
            hist, _, _ = np.histogram2d(
                points[:, 1], points[:, 0], bins=bins,
                range=((-half_span_mm, half_span_mm), (-half_span_mm, half_span_mm)))
            hist = np.flipud(hist)
            # No smoothing: each ray contributes its energy to one physical pixel.
            channel = hist / max(float(hist.max()), 1.0)
            rgb[:, :, channel_index[wavelength]] = np.sqrt(channel)
        panel = Image.fromarray(np.clip(rgb * 235, 0, 255).astype(np.uint8)).resize(
            (panel_width - 24, panel_width - 24), Image.Resampling.NEAREST)
        image.paste(panel, (left + 12, 31))
        draw.rectangle((left + 11, 30, left + panel_width - 13, panel_width - 5),
                       outline=(62, 68, 78))
        draw.text((left + 7, panel_width + 4),
                  f"shared +/-{half_span_um:.0f} um; {pitch_um:.1f} um pixels",
                  fill=(190, 198, 210))
        draw.text((left + 7, panel_width + 25), "sqrt density; each band peak normalized",
                  fill=(160, 168, 180))
    for i, wavelength in enumerate(wavelengths):
        x = 8 + i * 135
        draw.text((x, panel_height + 38), f"{wavelength:.0f} nm", fill=colors[wavelength])
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def make_illumination_chart(records, output):
    width, height = 1000, 560
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    left, top, plot_w, plot_h = 92, 48, 850, 360
    y_min, y_max = 0.45, 1.02
    draw.text((left, 12), "Relative field illumination estimate (normalized on axis)",
              fill=(235, 238, 244))
    def xy(angle, value):
        return (left + angle / 12.0 * plot_w,
                top + (y_max - value) / (y_max - y_min) * plot_h)
    for value in (0.50, 0.60, 0.70, 0.80, 0.90, 1.00):
        y = xy(0.0, value)[1]
        draw.line((left, y, left + plot_w, y), fill=(54, 61, 72), width=1)
        draw.text((30, y - 7), f"{value:.2f}", fill=(180, 188, 200))
    for angle in (0, 4, 8, 12):
        x = xy(angle, y_min)[0]
        draw.line((x, top, x, top + plot_h), fill=(42, 48, 58), width=1)
        draw.text((x - 6, top + plot_h + 8), str(angle), fill=(190, 198, 210))
    draw.text((left + plot_w // 2 - 35, top + plot_h + 28),
              "Object field angle (deg)", fill=(190, 198, 210))
    by_case = {(row["field"], row["wavelength_nm"]): row for row in records}
    field_names = tuple(name for name, _ in FIELD_CASES)
    angle_values = tuple(angle for _, angle in FIELD_CASES)
    wavelengths = (610.0, 550.0, 460.0)
    band_colors = {610.0: (255, 112, 89), 550.0: (132, 222, 255),
                   460.0: (175, 255, 137)}
    def draw_segments(points, color, width=2, dashed=False):
        for start, end in zip(points, points[1:]):
            if not dashed:
                draw.line((start, end), fill=color, width=width)
                continue
            dx, dy = end[0] - start[0], end[1] - start[1]
            for fraction in np.arange(0.0, 1.0, 0.18):
                a = (start[0] + dx * fraction, start[1] + dy * fraction)
                b_fraction = min(fraction + 0.09, 1.0)
                b = (start[0] + dx * b_fraction, start[1] + dy * b_fraction)
                draw.line((a, b), fill=color, width=width)

    for wavelength in wavelengths:
        points = []
        for name, angle in zip(field_names, angle_values):
            row = by_case[(name, wavelength)]
            points.append(xy(angle, row["relative_illumination_proxy"]))
        draw_segments(points, band_colors[wavelength], width=3)
        for px, py in points:
            draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=band_colors[wavelength])
    cosine_points = []
    for angle in np.linspace(0.0, 12.0, 121):
        chief = np.interp(angle, angle_values,
                          [by_case[(name, 550.0)]["chief_ray_angle_image_deg"]
                           for name in field_names])
        value = math.cos(math.radians(chief)) ** 4
        cosine_points.append(xy(float(angle), value))
    draw_segments(cosine_points, (230, 230, 235), width=2, dashed=True)
    pupil_points = [xy(angle, by_case[(name, 550.0)]["relative_transmitted_pupil_area_ratio"])
                    for name, angle in zip(field_names, angle_values)]
    draw_segments(pupil_points, band_colors[550.0], width=2, dashed=True)
    for px, py in pupil_points:
        draw.rectangle((px - 3, py - 3, px + 3, py + 3), outline=band_colors[550.0])
    legend_y = top + plot_h + 48
    draw_segments([(left, legend_y + 7), (left + 26, legend_y + 7)],
                  (230, 230, 235), width=2, dashed=True)
    draw.text((left + 34, legend_y), "cos^4 rolloff (550 nm chief ray)",
              fill=(215, 220, 228))
    draw_segments([(left + 360, legend_y + 7), (left + 386, legend_y + 7)],
                  band_colors[550.0], width=2, dashed=True)
    draw.text((left + 394, legend_y), "relative pupil area (550 nm)",
              fill=band_colors[550.0])
    legend_y += 25
    draw.text((left, legend_y), "Combined pupil x cos^4 proxy:", fill=(215, 220, 228))
    for i, wavelength in enumerate(wavelengths):
        x = left + 220 + i * 180
        draw.line((x, legend_y + 7, x + 24, legend_y + 7),
                  fill=band_colors[wavelength], width=3)
        draw.text((x + 30, legend_y), f"{wavelength:.0f} nm", fill=band_colors[wavelength])
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=512)
    parser.add_argument("--output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-psf.png"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-psf.csv"))
    parser.add_argument("--density-output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-irradiance.png"))
    parser.add_argument("--pixel-psf-output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-pixel-psf.png"))
    parser.add_argument("--pixel-pitch-um", type=float, default=SENSOR_PIXEL_PITCH_UM,
                        help="sensor pixel pitch used by the pixel-integrated PSF (default: 5 um)")
    parser.add_argument("--illumination-output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-relative-illumination.png"))
    args = parser.parse_args()
    if args.pixel_pitch_um <= 0.0:
        parser.error("--pixel-pitch-um must be positive")
    records = []
    clouds = {}
    chief_angles = {}
    for name, angle_deg in FIELD_CASES:
        angle = math.radians(angle_deg)
        for wavelength in (610.0, 550.0, 460.0):
            points, chief_angle = trace_field(angle, wavelength, args.samples)
            clouds[(name, wavelength)] = points
            chief_angles[(name, wavelength)] = math.degrees(chief_angle)
            records.append({"field": name, "field_angle_deg": angle_deg,
                            "chief_ray_angle_image_deg": chief_angles[(name, wavelength)],
                            "wavelength_nm": wavelength, "launch_grid": args.samples,
                            "valid_rays": len(points), **metrics(points),
                            **pixel_psf_metrics(points, args.pixel_pitch_um)})
    on_axis_counts = {wavelength: next(
        row["valid_rays"] for row in records
        if row["field"] == "on_axis" and row["wavelength_nm"] == wavelength)
        for wavelength in (610.0, 550.0, 460.0)}
    for row in records:
        wavelength = row["wavelength_nm"]
        pupil_ratio = row["valid_rays"] / max(on_axis_counts[wavelength], 1)
        cos4 = math.cos(math.radians(row["chief_ray_angle_image_deg"])) ** 4
        row["relative_transmitted_pupil_area_ratio"] = pupil_ratio
        row["cos4_image_space_rolloff"] = cos4
        row["relative_illumination_proxy"] = pupil_ratio * cos4
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    make_illumination_chart(records, args.illumination_output)

    width, height, pane = len(FIELD_CASES) * 490, 590, 480
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    colors = {610.0: (255, 112, 89), 550.0: (132, 222, 255), 460.0: (175, 255, 137)}
    for col, (name, angle) in enumerate((name, math.radians(degrees))
                                        for name, degrees in FIELD_CASES):
        left = col * 490 + 6
        data = [clouds[(name, wavelength)] for wavelength in colors]
        nonempty = [p for p in data if len(p)]
        draw.text((left + 4, 5), f"{name} ({math.degrees(angle):.1f} deg)", fill=(235, 238, 244))
        draw.rectangle((left, 28, left + pane, 508), outline=(62, 68, 78))
        if not nonempty:
            draw.text((left + 20, 250), "No transmitted rays", fill=(255, 128, 128))
            continue
        all_points = np.concatenate(nonempty)
        center = np.median(all_points, axis=0)
        radius = max(float(np.quantile(np.linalg.norm(all_points - center, axis=1), .995)), 1e-6)
        scale = 0.44 * pane / (2.0 * radius)
        for wavelength, points in zip(colors, data):
            for x, y in points:
                sx = left + pane / 2 + (x - center[0]) * scale
                sy = 28 + pane / 2 - (y - center[1]) * scale
                draw.ellipse((sx - 1, sy - 1, sx + 1, sy + 1), fill=colors[wavelength])
        draw.text((left + 4, 516), f"display diameter {2*radius*1000:.1f} um; ray counts " +
                  "/".join(str(len(p)) for p in data), fill=(190, 198, 210))
        for i, wavelength in enumerate(colors):
            draw.text((left + 8 + i * 130, 540), f"{wavelength:.0f} nm", fill=colors[wavelength])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)

    # Push the equally weighted object-plane samples into sensor-plane bins.
    # This is a geometric irradiance estimate (a ray-count density), not a
    # coherent or calibrated spectral PSF. Per-band normalization is for shape
    # inspection only; a small display blur suppresses the launch-grid lattice.
    density = Image.new("RGB", (width, height), (18, 21, 26))
    density_draw = ImageDraw.Draw(density)
    spectral_channel = {610.0: 0, 550.0: 1, 460.0: 2}
    for col, (name, angle) in enumerate((name, math.radians(degrees))
                                        for name, degrees in FIELD_CASES):
        left = col * 490 + 6
        data = [clouds[(name, wavelength)] for wavelength in colors]
        nonempty = [points for points in data if len(points)]
        density_draw.text((left + 4, 5), f"{name} ({math.degrees(angle):.1f} deg)",
                          fill=(235, 238, 244))
        density_draw.rectangle((left, 28, left + pane, 508), outline=(62, 68, 78))
        if not nonempty:
            density_draw.text((left + 20, 250), "No transmitted rays", fill=(255, 128, 128))
            continue
        all_points = np.concatenate(nonempty)
        center = np.median(all_points, axis=0)
        radius = max(float(np.quantile(np.linalg.norm(all_points - center, axis=1), .999)),
                     1e-6)
        half_span = radius * 1.08
        panel_size = 256
        rgb = np.zeros((panel_size, panel_size, 3), dtype=np.float64)
        for wavelength, points in zip(colors, data):
            if not len(points):
                continue
            hist, _, _ = np.histogram2d(
                points[:, 1] - center[1], points[:, 0] - center[0], bins=panel_size,
                range=((-half_span, half_span), (-half_span, half_span)))
            hist = np.flipud(hist)
            values = gaussian_filter(hist / max(float(hist.max()), 1.0), sigma=2.0)
            values /= max(float(values.max()), 1e-12)
            rgb[:, :, spectral_channel[wavelength]] = np.sqrt(values)
        panel = Image.fromarray(np.clip(rgb * 235, 0, 255).astype(np.uint8)).resize(
            (448, 448), Image.Resampling.BILINEAR)
        density.paste(panel, (left + 16, 44))
        density_draw.text((left + 4, 516), f"equal-energy ray density; +/-{half_span*1000:.1f} um",
                          fill=(190, 198, 210))
        for i, wavelength in enumerate(colors):
            density_draw.text((left + 8 + i * 130, 540), f"{wavelength:.0f} nm",
                              fill=colors[wavelength])
    args.density_output.parent.mkdir(parents=True, exist_ok=True)
    density.save(args.density_output)
    make_pixel_psf_chart(clouds, args.pixel_psf_output, args.pixel_pitch_um)
    for row in records:
        print(f"{row['field']:14s} {row['wavelength_nm']:.0f}nm: "
              f"rms={row['rms_radius_um']:.2f}um r90={row['r90_um']:.2f}um "
              f"centroid=({row['centroid_x_um']:.2f},{row['centroid_y_um']:.2f})um "
              f"rays={row['valid_rays']}")
    print(f"saved {args.csv}, {args.output}, {args.density_output}, "
          f"{args.pixel_psf_output}, and {args.illumination_output}")


if __name__ == "__main__":
    main()
