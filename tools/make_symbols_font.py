import pathlib
import struct
import sys

UNITS_PER_EM = 1000
ASCENT = 800
DESCENT = -200
ADVANCE = 1000
SPACE = 0x0020
SYMBOLS = (0xE0E0, 0xE0E1, 0xE0E2, 0xE0E3, 0xE0E4, 0xE0E5)


def square():
    points = ((100, 0), (100, 700), (900, 700), (900, 0))
    flags = bytes([0x01] * len(points))
    coordinates = bytearray()
    previous = 0
    for x, _ in points:
        coordinates += struct.pack(">h", x - previous)
        previous = x
    ys = bytearray()
    previous = 0
    for _, y in points:
        ys += struct.pack(">h", y - previous)
        previous = y
    header = struct.pack(">hhhhh", 1, 100, 0, 900, 700)
    body = struct.pack(">HH", len(points) - 1, 0) + flags + coordinates + ys
    glyph = header + body
    return glyph + b"\0" * (-len(glyph) % 4)


def glyf_and_loca(count):
    outline = square()
    glyf = bytearray()
    offsets = [0, 0, 0]
    for _ in range(count - 2):
        glyf += outline
        offsets.append(len(glyf))
    loca = b"".join(struct.pack(">I", offset) for offset in offsets)
    return bytes(glyf), loca


def cmap(mapping):
    segments = [(code, code, glyph) for code, glyph in sorted(mapping.items())]
    segments.append((0xFFFF, 0xFFFF, 0))
    count = len(segments)
    search = 1
    selector = 0
    while search * 2 <= count:
        search *= 2
        selector += 1
    ends = b"".join(struct.pack(">H", end) for _, end, _ in segments)
    starts = b"".join(struct.pack(">H", start) for start, _, _ in segments)
    deltas = b"".join(
        struct.pack(">H", (glyph - start) & 0xFFFF if glyph else 1)
        for start, _, glyph in segments)
    ranges = b"\0\0" * count
    body = (struct.pack(">HHHH", count * 2, search * 2, selector,
                        (count - search) * 2) + ends + b"\0\0" + starts +
            deltas + ranges)
    subtable = struct.pack(">HHH", 4, 6 + len(body), 0) + body
    return struct.pack(">HHHHI", 0, 1, 3, 1, 12) + subtable


def name():
    records = ((1, "Arisa Test Symbols"), (2, "Regular"),
               (4, "Arisa Test Symbols"), (6, "ArisaTestSymbols"))
    strings = bytearray()
    entries = bytearray()
    for identifier, text in records:
        encoded = text.encode("utf-16-be")
        entries += struct.pack(">HHHHHH", 3, 1, 0x409, identifier,
                               len(encoded), len(strings))
        strings += encoded
    header = struct.pack(">HHH", 0, len(records), 6 + len(entries))
    return header + bytes(entries) + bytes(strings)


def os2():
    return struct.pack(
        ">HhHHHhhhhhhhhhhh10sIIII4sHHHhhhHHIIhhHHH", 4, ADVANCE, 400, 5, 0,
        650, 700, 0, 140, 650, 700, 0, 480, 50, 250, 0, b"\0" * 10, 0, 0,
        0x00000002, 0, b"NONE", 0x0040, SPACE, max(SYMBOLS), ASCENT, DESCENT,
        0, ASCENT, -DESCENT, 1, 0, 500, 700, 0, SPACE, 0)


def checksum(data):
    padded = data + b"\0" * (-len(data) % 4)
    return sum(struct.unpack(f">{len(padded) // 4}I", padded)) & 0xFFFFFFFF


def build():
    count = 2 + len(SYMBOLS)
    mapping = {SPACE: 1}
    mapping.update({code: 2 + index for index, code in enumerate(SYMBOLS)})
    glyf, loca = glyf_and_loca(count)
    tables = {
        b"OS/2": os2(),
        b"cmap": cmap(mapping),
        b"glyf": glyf,
        b"head": struct.pack(">IIIIHHqqhhhhHHhhh", 0x00010000, 0x00010000, 0,
                             0x5F0F3CF5, 0x000B, UNITS_PER_EM, 0, 0, 0,
                             DESCENT, ADVANCE, ASCENT, 0, 8, 2, 1, 0),
        b"hhea": struct.pack(">IhhhHhhhhhhhhhhhH", 0x00010000, ASCENT, DESCENT,
                             0, ADVANCE, 0, 0, 900, 1, 0, 0, 0, 0, 0, 0, 0,
                             count),
        b"hmtx": b"".join(
            struct.pack(">Hh", 500 if index < 2 else ADVANCE,
                        0 if index < 2 else 100) for index in range(count)),
        b"loca": loca,
        b"maxp": struct.pack(">IHHHHHHHHHHHHHH", 0x00010000, count, 4, 1, 0, 0,
                             2, 0, 0, 0, 0, 0, 0, 0, 0),
        b"name": name(),
        b"post": struct.pack(">IIhhIIIII", 0x00030000, 0, -100, 50, 0, 0, 0,
                             0, 0),
    }
    tags = sorted(tables)
    search = 1
    selector = 0
    while search * 2 <= len(tags):
        search *= 2
        selector += 1
    header = struct.pack(">IHHHH", 0x00010000, len(tags), search * 16,
                         selector, len(tags) * 16 - search * 16)
    offset = len(header) + 16 * len(tags)
    directory = bytearray()
    body = bytearray()
    for tag in tags:
        data = tables[tag]
        directory += struct.pack(">4sIII", tag, checksum(data), offset,
                                 len(data))
        padded = data + b"\0" * (-len(data) % 4)
        body += padded
        offset += len(padded)
    font = bytearray(header + directory + body)
    head = font.index(b"head", len(header))
    head_offset = struct.unpack(">I", font[head + 8:head + 12])[0]
    adjustment = (0xB1B0AFBA - checksum(bytes(font))) & 0xFFFFFFFF
    font[head_offset + 8:head_offset + 12] = struct.pack(">I", adjustment)
    return bytes(font)


if __name__ == "__main__":
    pathlib.Path(sys.argv[1]).write_bytes(build())
