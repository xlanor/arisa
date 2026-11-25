import os
import pathlib
import sys

OWN_DIRECTORIES = ("src/", "include/", "examples/", "switch/", "tests/")
BANDS = (32 * 1024, 64 * 1024, 128 * 1024)


def frames(build, root):
    found = []
    for path in build.rglob("*.su"):
        for line in path.read_text(errors="replace").splitlines():
            parts = line.rsplit("\t", 2)
            if len(parts) != 3:
                continue
            location, size, kind = parts
            if not size.isdigit():
                continue
            fields = location.split(":", 3)
            if len(fields) != 4:
                continue
            source, line_number, _, symbol = fields
            resolved = pathlib.Path(os.path.normpath(build / source))
            if not resolved.is_relative_to(root):
                continue
            relative = resolved.relative_to(root).as_posix()
            if not relative.startswith(OWN_DIRECTORIES):
                continue
            found.append((int(size), kind, f"{relative}:{line_number}", symbol))
    return found


def main():
    if len(sys.argv) < 2:
        print("usage: stack_census.py <build directory> [top]")
        return 2
    build = pathlib.Path(sys.argv[1]).resolve()
    top = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    root = pathlib.Path(__file__).resolve().parent.parent
    unique = {}
    for size, kind, source, symbol in frames(build, root):
        key = (source, symbol)
        if key not in unique or unique[key][0] < size:
            unique[key] = (size, kind)
    if not unique:
        print(f"no .su files under {build}: configure it with -fstack-usage")
        return 1
    ranked = sorted(
        ((size, kind, source, symbol) for (source, symbol), (size, kind) in unique.items()),
        reverse=True,
    )
    print(f"{len(ranked)} functions in arisa's own sources")
    for band in BANDS:
        count = sum(1 for size, *_ in ranked if size >= band)
        print(f"  {count} with frames of {band // 1024} KiB or more")
    print(f"top {top}:")
    for size, kind, source, symbol in ranked[:top]:
        name = symbol
        if len(name) > 140:
            name = name[:137] + "..."
        print(f"{size:>9}  {kind:<16} {source}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
