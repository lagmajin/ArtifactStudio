from pathlib import Path
import struct
import zlib

out = Path(__file__).parent / "assets"
out.mkdir(parents=True, exist_ok=True)
W = H = 96

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

def pixel(x, y, frame):
    # Checkerboard background, moving red square, and frame-specific corner marker.
    base = (232, 232, 232) if ((x // 12 + y // 12) % 2 == 0) else (185, 185, 185)
    left = 8 + frame * 8
    if left <= x < left + 18 and 36 <= y < 54:
        return (235, 48, 38)
    if 4 <= x < 12 and 4 <= y < 12:
        return (35 * (frame + 1), 70, 210)
    return base

def write_png(path, frame):
    rows = b"".join(b"\x00" + b"".join(bytes(pixel(x, y, frame)) for x in range(W)) for y in range(H))
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">2I5B", W, H, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(rows, 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)

for n in (1, 2, 3, 5, 6):
    write_png(out / f"frame_{n:04d}.png", n - 1)
