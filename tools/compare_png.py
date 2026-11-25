import os
import pathlib
import shutil
import sys

from check_png import decode

LEVEL_THRESHOLD = 16
MAXIMUM_OFF_FRACTION = 0.0005
MAXIMUM_MEAN_DIFFERENCE = 0.1
NEIGHBOURHOOD = (-1, 0, 1)


def close(capture_row, index, golden_row, other):
    return all(abs(capture_row[index + channel] - golden_row[other + channel]) <= LEVEL_THRESHOLD
               for channel in range(3))


def displaced(capture_row, index, golden_rows, row, width):
    column = index // 4
    for dy in NEIGHBOURHOOD:
        if not 0 <= row + dy < len(golden_rows):
            continue
        golden_row = golden_rows[row + dy]
        for dx in NEIGHBOURHOOD:
            if 0 <= column + dx < width and close(capture_row, index, golden_row, (column + dx) * 4):
                return True
    return False


def compare(capture, golden):
    capture_width, capture_height, capture_rows = decode(capture)
    golden_width, golden_height, golden_rows = decode(golden)
    if (capture_width, capture_height) != (golden_width, golden_height):
        return False, f"{capture} is {capture_width}x{capture_height}, golden is {golden_width}x{golden_height}"
    total = 0
    off = 0
    pixels = capture_width * capture_height
    for row, (capture_row, golden_row) in enumerate(zip(capture_rows, golden_rows)):
        if capture_row == golden_row:
            continue
        for index in range(0, len(capture_row), 4):
            worst = 0
            for channel in range(3):
                difference = abs(capture_row[index + channel] - golden_row[index + channel])
                total += difference
                worst = max(worst, difference)
            if worst > LEVEL_THRESHOLD and not displaced(capture_row, index, golden_rows, row, capture_width):
                off += 1
    mean = total / (pixels * 3)
    summary = (f"{off} pixels off by more than {LEVEL_THRESHOLD} with no match in their 3x3 golden "
               f"neighbourhood (limit {pixels * MAXIMUM_OFF_FRACTION:.0f}), mean difference {mean:.4f} "
               f"(limit {MAXIMUM_MEAN_DIFFERENCE})")
    if off > pixels * MAXIMUM_OFF_FRACTION or mean > MAXIMUM_MEAN_DIFFERENCE:
        return False, f"{capture} differs from {golden}: {summary}"
    return True, f"{capture} matches {golden}: {summary}"


def main():
    if len(sys.argv) != 3:
        print("usage: compare_png.py capture.png golden.png")
        return 2
    capture, golden = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    if os.environ.get("ARISA_UPDATE_GOLDENS") == "1":
        golden.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(capture, golden)
        print(f"updated {golden}")
        return 0
    if not golden.exists():
        print(f"{golden} is missing; run make update-goldens")
        return 1
    matches, message = compare(capture, golden)
    print(message)
    return 0 if matches else 1


if __name__ == "__main__":
    sys.exit(main())
