"""Render the app's simple vector mark as a PNG without external dependencies."""
import struct
import zlib
from pathlib import Path

size = 1024
pixels = bytearray()
for y in range(size):
    pixels.append(0)  # PNG row filter
    for x in range(size):
        dx, dy = max(0, abs(x - 512) - 262), max(0, abs(y - 512) - 262)
        inside = dx * dx + dy * dy <= 180 * 180
        colour = (16, 17, 20, 255) if inside else (0, 0, 0, 0)
        if (260 <= x < 380 and 360 <= y < 700) or (452 <= x < 572 and 264 <= y < 700) or (644 <= x < 764 and 456 <= y < 700):
            colour = (98, 245, 180, 255)
        pixels.extend(colour)
def chunk(name, data):
    return struct.pack('>I', len(data)) + name + data + struct.pack('>I', zlib.crc32(name + data))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(pixels, 9)) + chunk(b'IEND', b'')
Path(__file__).resolve().parents[1].joinpath('packaging/icon.png').write_bytes(png)
