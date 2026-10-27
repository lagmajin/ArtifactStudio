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


def trace_field(field_angle, wavelength_nm, samples):
    # Collimated source rays start on a square grid at an object-side plane.
    # The grid spans the entrance pupil footprint at surface zero.
    direction = np.array((math.sin(field_angle), 0.0, math.cos(field_angle)))
    extent = 35.0
    grid_offset_x = -(IRIS_Z + 100.0) * math.tan(field_angle)
    grid = np.linspace(-extent, extent, samples, endpoint=False) + extent / samples
    spots = []
    for y in grid:
        for x in grid:
            origin = np.array((x + grid_offset_x, y, -100.0))
            d = direction.copy()
            alive = True
            for i, surface in enumerate(SURFACES):
                if i == 7:
                    t = (IRIS_Z - origin[2]) / d[2]
                    iris = origin + t * d
                    if t <= 0.0 or not in_hexagon(iris[0], iris[1], STOP_RADIUS):
                        alive = False
                        break
                    origin = iris
                point = intersect(origin, d, surface)
                if point is None:
                    alive = False
                    break
                n1 = 1.0 if i == 0 else refractive_index(SURFACES[i - 1], wavelength_nm)
                n2 = refractive_index(surface, wavelength_nm)
                d = refract(d, point, surface, n1, n2)
                if d is None:
                    alive = False
                    break
                origin = point
            if alive and d[2] > 1e-10:
                sensor = origin + ((SENSOR_Z - origin[2]) / d[2]) * d
                spots.append(sensor[:2])
    return np.asarray(spots, dtype=np.float64).reshape(-1, 2)


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=512)
    parser.add_argument("--output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-psf.png"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-psf.csv"))
    parser.add_argument("--density-output", type=Path, default=Path(
        "tools/LensFlareLab/examples/direct-transmission-geometric-irradiance.png"))
    args = parser.parse_args()
    fields = (("on_axis", 0.0), ("field_5deg", math.radians(5.0)),
              ("field_10deg", math.radians(10.0)))
    records = []
    clouds = {}
    for name, angle in fields:
        for wavelength in (610.0, 550.0, 460.0):
            points = trace_field(angle, wavelength, args.samples)
            clouds[(name, wavelength)] = points
            records.append({"field": name, "field_angle_deg": math.degrees(angle),
                            "wavelength_nm": wavelength, "launch_grid": args.samples,
                            "valid_rays": len(points), **metrics(points)})
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    width, height, pane = 1470, 590, 480
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    colors = {610.0: (255, 112, 89), 550.0: (132, 222, 255), 460.0: (175, 255, 137)}
    for col, (name, angle) in enumerate(fields):
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
    for col, (name, angle) in enumerate(fields):
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
    for row in records:
        print(f"{row['field']:14s} {row['wavelength_nm']:.0f}nm: "
              f"rms={row['rms_radius_um']:.2f}um r90={row['r90_um']:.2f}um "
              f"centroid=({row['centroid_x_um']:.2f},{row['centroid_y_um']:.2f})um "
              f"rays={row['valid_rays']}")
    print(f"saved {args.csv}, {args.output}, and {args.density_output}")


if __name__ == "__main__":
    main()
