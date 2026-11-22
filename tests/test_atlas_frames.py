"""Check the original black frames in every cell, using only Python's standard library."""

import struct
import sys
import zlib
from pathlib import Path


CARD_WIDTH, CARD_HEIGHT = 179, 264
WIDTH, HEIGHT = CARD_WIDTH * 12, CARD_HEIGHT * 4


def png_rows(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not a PNG")
    offset, compressed, image_header = 8, bytearray(), None
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        chunk = data[offset + 8 : offset + 8 + length]
        if len(chunk) != length:
            raise ValueError("Truncated PNG")
        if kind == b"IHDR":
            image_header = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            break
        offset += length + 12

    # This asset is intentionally stored as an 8-bit, non-interlaced RGB PNG.
    if image_header != (WIDTH, HEIGHT, 8, 2, 0, 0, 0):
        raise ValueError("Unexpected atlas PNG format or dimensions")
    raw = zlib.decompress(compressed)
    stride = WIDTH * 3
    if len(raw) != (stride + 1) * HEIGHT:
        raise ValueError("Unexpected PNG image data size")

    previous = bytearray(stride)
    for y in range(HEIGHT):
        start = y * (stride + 1)
        filter_type = raw[start]
        row = bytearray(raw[start + 1 : start + 1 + stride])
        for i in range(stride):
            left = row[i - 3] if i >= 3 else 0
            above = previous[i]
            upper_left = previous[i - 3] if i >= 3 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError("Unsupported PNG filter")
            row[i] = (row[i] + predictor) & 255
        yield y, row
        previous = row


def check_frames(path):
    for y, row in png_rows(path):
        row_in_card = y % CARD_HEIGHT
        if row_in_card in range(1, 4) or row_in_card in range(260, 263):
            sections = [(3, 176)]  # top or bottom frame, away from corners
        elif 4 <= row_in_card < 260:
            sections = [(1, 4), (175, 178)]  # left and right frames
        else:
            continue
        for column in range(12):
            for left, right in sections:
                start = (column * CARD_WIDTH + left) * 3
                end = (column * CARD_WIDTH + right) * 3
                if max(row[start:end]) >= 45:
                    raise ValueError(f"Black frame missing at month {column + 1}, row {y // CARD_HEIGHT}")


if __name__ == "__main__":
    try:
        check_frames(sys.argv[1])
        print("All 48 cards retain their four black frames.")
    except (IndexError, OSError, ValueError, zlib.error) as error:
        print(f"Atlas frame verification failed: {error}", file=sys.stderr)
        sys.exit(1)
