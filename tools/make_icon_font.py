import json
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

FIRST = 0x100000
VARIATIONS = "FILL=0 GRAD=0 opsz=24 wght=400"


def symbol_names(header):
    text = pathlib.Path(header).read_text()
    block = re.search(r"names\{(.*?)\};", text, re.S)
    return re.findall(r'"([a-z0-9_]+)"', block.group(1))


def codepoints(path):
    table = {}
    for line in pathlib.Path(path).read_text().splitlines():
        name, value = line.split()
        table[name] = int(value, 16)
    return table


def ligature_glyphs(source, names):
    table = {}
    for name in names:
        shaped = subprocess.run(
            ["hb-shape", "--no-glyph-names", "--output-format=json", source, name],
            check=True,
            capture_output=True,
            text=True,
        )
        glyphs = json.loads(shaped.stdout)
        if len(glyphs) == 1:
            table[name] = glyphs[0]["g"]
    return table


def tables(data):
    count = struct.unpack(">H", data[4:6])[0]
    found = {}
    for index in range(count):
        tag, _, offset, length = struct.unpack(
            ">4sIII", data[12 + index * 16 : 28 + index * 16]
        )
        found[tag] = data[offset : offset + length]
    return found


def read_cmap(cmap):
    count = struct.unpack(">H", cmap[2:4])[0]
    mapping = {}
    for index in range(count):
        _, _, offset = struct.unpack(">HHI", cmap[4 + index * 8 : 12 + index * 8])
        form = struct.unpack(">H", cmap[offset : offset + 2])[0]
        if form == 12:
            groups = struct.unpack(">I", cmap[offset + 12 : offset + 16])[0]
            for group in range(groups):
                start, end, glyph = struct.unpack(
                    ">III", cmap[offset + 16 + group * 12 : offset + 28 + group * 12]
                )
                for code in range(start, end + 1):
                    mapping[code] = glyph + code - start
        elif form == 4:
            segments = struct.unpack(">H", cmap[offset + 6 : offset + 8])[0] // 2
            ends = offset + 14
            starts = ends + segments * 2 + 2
            deltas = starts + segments * 2
            ranges = deltas + segments * 2
            for segment in range(segments):
                end = struct.unpack(">H", cmap[ends + segment * 2 : ends + segment * 2 + 2])[0]
                start = struct.unpack(">H", cmap[starts + segment * 2 : starts + segment * 2 + 2])[0]
                delta = struct.unpack(">h", cmap[deltas + segment * 2 : deltas + segment * 2 + 2])[0]
                where = ranges + segment * 2
                shift = struct.unpack(">H", cmap[where : where + 2])[0]
                for code in range(start, end + 1):
                    if code == 0xFFFF:
                        continue
                    if shift == 0:
                        glyph = (code + delta) & 0xFFFF
                    else:
                        at = where + shift + (code - start) * 2
                        glyph = struct.unpack(">H", cmap[at : at + 2])[0]
                        if glyph != 0:
                            glyph = (glyph + delta) & 0xFFFF
                    if glyph != 0:
                        mapping.setdefault(code, glyph)
    return mapping


def write_cmap(mapping):
    groups = b"".join(
        struct.pack(">III", code, code, glyph) for code, glyph in sorted(mapping.items())
    )
    subtable = struct.pack(">HHIII", 12, 0, 16 + len(groups), 0, len(mapping)) + groups
    return struct.pack(">HHHHI", 0, 1, 3, 10, 12) + subtable


def checksum(data):
    padded = data + b"\0" * (-len(data) % 4)
    return sum(struct.unpack(f">{len(padded) // 4}I", padded)) & 0xFFFFFFFF


def assemble(found):
    tags = sorted(found)
    count = len(tags)
    power = 1
    while power * 2 <= count:
        power *= 2
    header = struct.pack(
        ">IHHHH", 0x00010000, count, power * 16, power.bit_length() - 1, count * 16 - power * 16
    )
    offset = 12 + count * 16
    directory = b""
    body = b""
    head_at = 0
    for tag in tags:
        data = found[tag]
        if tag == b"head":
            data = data[:8] + b"\0\0\0\0" + data[12:]
            head_at = offset
        directory += struct.pack(">4sIII", tag, checksum(data), offset, len(data))
        padded = data + b"\0" * (-len(data) % 4)
        body += padded
        offset += len(padded)
    font = bytearray(header + directory + body)
    adjustment = (0xB1B0AFBA - checksum(bytes(font))) & 0xFFFFFFFF
    font[head_at + 8 : head_at + 12] = struct.pack(">I", adjustment)
    return bytes(font)


def subset(source, selection, variable):
    with tempfile.TemporaryDirectory() as scratch:
        output = pathlib.Path(scratch) / "subset.ttf"
        subprocess.run(
            [
                "hb-subset",
                source,
                *([f"--variations={VARIATIONS}"] if variable else []),
                selection,
                "--no-hinting",
                "--layout-features-=*",
                "--drop-tables+=GSUB,GPOS,GDEF,STAT,meta,DSIG",
                f"--output-file={output}",
            ],
            check=True,
        )
        return tables(output.read_bytes())


def main():
    source, points, header, output = sys.argv[1:5]
    first = int(sys.argv[5], 16) if len(sys.argv) > 5 else FIRST
    names = symbol_names(header)
    variable = b"fvar" in tables(pathlib.Path(source).read_bytes())
    if points == "-":
        glyphs = ligature_glyphs(source, names)
        missing = [name for name in names if name not in glyphs]
        if missing:
            sys.exit(f"unknown symbols: {missing}")
        kept = sorted(set(glyphs.values()))
        found = subset(source, "--gids=" + ",".join(str(glyph) for glyph in kept), variable)
        count = struct.unpack(">H", found[b"maxp"][4:6])[0]
        if count != len(kept) + 1:
            sys.exit(f"subset has {count} glyphs, expected {len(kept) + 1}")
        rank = {glyph: index + 1 for index, glyph in enumerate(kept)}
        mapping = {first + index: rank[glyphs[name]] for index, name in enumerate(names)}
    else:
        table = codepoints(points)
        missing = [name for name in names if name not in table]
        if missing:
            sys.exit(f"unknown symbols: {missing}")
        wanted = sorted({table[name] for name in names})
        found = subset(
            source, "--unicodes=" + ",".join(f"{code:X}" for code in wanted), variable
        )
        original = read_cmap(found[b"cmap"])
        mapping = {first + index: original[table[name]] for index, name in enumerate(names)}
    found[b"cmap"] = write_cmap(mapping)
    pathlib.Path(output).write_bytes(assemble(found))
    print(f"{output}: {len(names)} symbols, {pathlib.Path(output).stat().st_size} bytes")


if __name__ == "__main__":
    main()
