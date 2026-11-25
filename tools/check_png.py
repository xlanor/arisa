import pathlib
import struct
import sys
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunks(data):
    offset = len(SIGNATURE)
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset:offset + 8])
        yield kind, data[offset + 8:offset + 8 + length]
        offset += 12 + length


def paeth(left, up, corner):
    estimate = left + up - corner
    distances = (abs(estimate - left), abs(estimate - up), abs(estimate - corner))
    if distances[0] <= distances[1] and distances[0] <= distances[2]:
        return left
    return up if distances[1] <= distances[2] else corner


def decode(path):
    data = pathlib.Path(path).read_bytes()
    if not data.startswith(SIGNATURE):
        raise ValueError("not a PNG file")
    header = None
    compressed = bytearray()
    for kind, body in chunks(data):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.extend(body)
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour != 6 or interlace != 0:
        raise ValueError("expected 8-bit RGBA without interlacing")
    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    previous = bytearray(stride)
    rows = []
    position = 0
    for _ in range(height):
        kind = raw[position]
        line = bytearray(raw[position + 1:position + 1 + stride])
        position += 1 + stride
        for index in range(stride):
            left = line[index - 4] if index >= 4 else 0
            up = previous[index]
            corner = previous[index - 4] if index >= 4 else 0
            if kind == 1:
                line[index] = (line[index] + left) & 0xFF
            elif kind == 2:
                line[index] = (line[index] + up) & 0xFF
            elif kind == 3:
                line[index] = (line[index] + ((left + up) >> 1)) & 0xFF
            elif kind == 4:
                line[index] = (line[index] + paeth(left, up, corner)) & 0xFF
        rows.append(bytes(line))
        previous = line
    return width, height, rows


def main():
    path, width, height = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    other = None
    if len(sys.argv) == 6 and sys.argv[4] == "--differs-from":
        other = sys.argv[5]
    elif len(sys.argv) != 4:
        print("usage: check_png.py path width height [--differs-from other.png]")
        return 2
    decoded_width, decoded_height, rows = decode(path)
    if (decoded_width, decoded_height) != (width, height):
        print(f"{path} is {decoded_width}x{decoded_height}, expected {width}x{height}")
        return 1
    colours = set()
    for row in rows:
        for index in range(0, len(row), 4):
            colours.add(row[index:index + 4])
        if len(colours) > 16:
            break
    if len(colours) < 2:
        print(f"{path} is a single flat colour")
        return 1
    if other is not None:
        other_width, other_height, other_rows = decode(other)
        if (other_width, other_height) == (decoded_width, decoded_height) and other_rows == rows:
            print(f"{path} is identical to {other}")
            return 1
        print(f"{path} differs from {other}")
    print(f"{path}: {decoded_width}x{decoded_height}, at least {len(colours)} colours")
    return 0


if __name__ == "__main__":
    sys.exit(main())
