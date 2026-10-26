"""Generate the code-native KVM icon (SVG, PNG, multi-size ICO). Python stdlib only."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1] / 'resources'
# Shared vector scene: one keyboard fans out to three displays.
shapes = [(0, 0, 256, 256, 48, '#142238'),
          (122, 86, 12, 60, 4, '#53d9c5'), (48, 104, 160, 10, 4, '#53d9c5'),
          (48, 82, 10, 30, 3, '#53d9c5'), (198, 82, 10, 30, 3, '#53d9c5')]
for x in (26, 102, 178):
    shapes += [(x, 40, 52, 46, 7, '#dce9ff'), (x + 6, 46, 40, 32, 3, '#326695')]
shapes += [(30, 138, 196, 78, 12, '#dce9ff'), (38, 146, 180, 62, 6, '#233e5c')]
for y in (154, 174):
    for x in range(48, 200, 24):
        shapes.append((x, y, 16, 12, 2, '#53d9c5'))
shapes.append((78, 194, 100, 6, 2, '#dce9ff'))
svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">\n'
for x, y, w, h, r, color in shapes:
    svg += f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{color}"/>\n'
(root / 'app.svg').write_text(svg + '</svg>\n')

def png(size):
    scale = size * 4 / 256
    n = size * 4
    pixels = bytearray(n * n * 4)
    for x, y, w, h, radius, color in shapes:
        x, y, w, h, radius = [v * scale for v in (x, y, w, h, radius)]
        rgba = bytes.fromhex(color[1:]) + b'\xff'
        for py in range(int(y), min(n, int(y + h + 1))):
            for px in range(int(x), min(n, int(x + w + 1))):
                cx = max(x + radius, min(px + .5, x + w - radius))
                cy = max(y + radius, min(py + .5, y + h - radius))
                if (px + .5 - cx)**2 + (py + .5 - cy)**2 <= radius**2:
                    offset = (py * n + px) * 4
                    pixels[offset:offset + 4] = rgba
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            offsets = [((y * 4 + dy) * n + x * 4 + dx) * 4 for dy in range(4) for dx in range(4)]
            raw.extend(sum(pixels[o + c] for o in offsets) // 16 for c in range(4))
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')

sizes = (16, 24, 32, 48, 64, 128, 256)
images = [png(size) for size in sizes]
(root / 'app.png').write_bytes(images[-1])
header = struct.pack('<HHH', 0, 1, len(sizes))
offset = 6 + 16 * len(sizes)
for size, image in zip(sizes, images):
    header += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(image), offset)
    offset += len(image)
(root / 'windows' / 'app.ico').write_bytes(header + b''.join(images))
