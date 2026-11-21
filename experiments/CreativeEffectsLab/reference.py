"""Offline CPU references. Images are linear float RGBA with straight alpha."""

import numpy as np


def _over(canvas, rgb, alpha):
    """Composite into a premultiplied working buffer."""
    keep = 1 - alpha
    canvas[..., :3] = rgb * alpha[..., None] + canvas[..., :3] * keep[..., None]
    canvas[..., 3] = alpha + canvas[..., 3] * keep


def _straight(canvas):
    alpha = canvas[..., 3:4]
    np.divide(canvas[..., :3], alpha, out=canvas[..., :3], where=alpha > 0)
    return canvas


def _shift(mask, dx, dy):
    height, width = mask.shape
    result = np.zeros_like(mask)
    x0, x1 = max(0, dx), min(width, width + dx)
    y0, y1 = max(0, dy), min(height, height + dy)
    if x0 < x1 and y0 < y1:
        result[y0:y1, x0:x1] = mask[y0-dy:y1-dy, x0-dx:x1-dx]
    return result


def paper_cutout(image, palette, thickness=2, shadow_offset=(3, 4)):
    """Map to an ordered 3–5 color palette, from bottom paper to top."""
    palette = np.asarray(palette, dtype=np.float32)
    if palette.shape not in ((3, 3), (4, 3), (5, 3)) or thickness < 0:
        raise ValueError("Expected 3–5 RGB colors and nonnegative thickness")
    rgb, alpha = image[..., :3], image[..., 3]
    labels = np.argmin(np.sum((rgb[..., None, :] - palette) ** 2, axis=-1), axis=-1)
    output = np.zeros_like(image)
    dx, dy = shadow_offset
    for index, color in enumerate(palette):
        mask = alpha * (labels == index)
        if thickness:
            # Scale the depth of fine details by their local support. A
            # one-pixel line keeps its original face without gaining a backing
            # strip wider than the line itself.
            interior = np.minimum.reduce(
                [mask, _shift(mask, 1, 0), _shift(mask, -1, 0),
                 _shift(mask, 0, 1), _shift(mask, 0, -1)])
            backing = np.maximum.reduce(
                [interior, _shift(interior, 1, 0), _shift(interior, -1, 0),
                 _shift(interior, 0, 1), _shift(interior, 0, -1)])
            edge = np.zeros_like(mask)
            for depth in range(1, thickness + 1):
                np.maximum(edge, _shift(backing, 0, depth), out=edge)
            shadow = _shift(backing, dx, dy)
            _over(output, np.zeros(3), shadow * 0.32)
            _over(output, color * 0.65, edge)
        _over(output, color, mask)
    return _straight(output)


def neon_ignition(size, cubics, progress, width, color, intensity=1):
    """Render connected cubic Bézier segments. Coordinates and width are pixels."""
    image = np.zeros((*size, 4), dtype=np.float32)
    if progress <= 0 or width <= 0 or intensity <= 0:
        return image
    t = np.linspace(0, 1, 65)[:, None]
    points = []
    for index, (a, b, c, d) in enumerate(np.asarray(cubics, dtype=float)):
        curve = (1-t)**3*a + 3*(1-t)**2*t*b + 3*(1-t)*t**2*c + t**3*d
        points.extend(curve if index == 0 else curve[1:])
    if len(points) < 2:
        return image
    points = np.asarray(points)
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    remaining = min(progress, 1) * lengths.sum()
    y, x = np.ogrid[:size[0], :size[1]]
    distance_sq = np.full(size, np.inf)
    tip = points[0]
    for start, end, length in zip(points[:-1], points[1:], lengths):
        if remaining <= 0:
            break
        if length == 0:
            continue
        delta = (end - start) * min(remaining / length, 1)
        tip = start + delta
        projection = np.clip(((x-start[0])*delta[0] + (y-start[1])*delta[1])
                             / np.dot(delta, delta), 0, 1)
        d2 = (x-start[0]-projection*delta[0])**2 + (y-start[1]-projection*delta[1])**2
        np.minimum(distance_sq, d2, out=distance_sq)
        remaining -= length
    core = np.clip(width/2 + 0.5 - np.sqrt(distance_sq), 0, 1)
    glow = np.exp(-distance_sq / (2 * width**2))
    rgb = np.asarray(color) * intensity
    _over(image, rgb, glow * 0.55)
    _over(image, rgb * 2, core)
    if progress < 1 and np.any(lengths > 0):
        tip_distance_sq = (x - tip[0]) ** 2 + (y - tip[1]) ** 2
        tip_glow = np.exp(-tip_distance_sq / (2 * width**2))
        _over(image, rgb * 3, tip_glow * 0.65 * min(1, (1 - progress) * 20))
    return _straight(image)


def time_mosaic(read_frame, frame, first_frame, rows=3, columns=3, delay=1,
                order="row"):
    """Read by absolute frame index. The callback must render on a cache miss."""
    if rows < 1 or columns < 1 or delay < 0 or frame < first_frame:
        raise ValueError("Invalid mosaic dimensions or frame range")
    if order not in ("row", "column", "snake"):
        raise ValueError("Unknown tile order")
    current = read_frame(frame)
    height, width = current.shape[:2]
    if rows > height or columns > width:
        raise ValueError("Each tile needs at least one pixel")
    output = np.empty_like(current)
    for row in range(rows):
        y0, y1 = row * height // rows, (row + 1) * height // rows
        for column in range(columns):
            x0, x1 = column * width // columns, (column + 1) * width // columns
            if order == "column":
                index = column * rows + row
            elif order == "snake" and row % 2:
                index = row * columns + columns - 1 - column
            else:
                index = row * columns + column
            source_frame = max(first_frame, frame - index * delay)
            source = current if source_frame == frame else read_frame(source_frame)
            if source.shape != current.shape:
                raise ValueError("Source frames must have matching dimensions")
            output[y0:y1, x0:x1] = source[y0:y1, x0:x1]
    return output
