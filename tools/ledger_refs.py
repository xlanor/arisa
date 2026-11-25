import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEDGER = ROOT / "docs" / "swiftui-ledger.tsv"
HEADERS = ROOT / "include" / "arisa"
ALIASES = {"callAsFunction": "operator()", "isEOFilled": "eo_fill", "init": ""}


def snake(name):
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).lower()


def candidates(row):
    symbol = row["symbol"]
    head = symbol.split("(")[0]
    parts = [p for p in head.split(".") if p]
    if row["kind"] == "init":
        owner = symbol.split(".init")[0].split(".")[-1]
        return [owner]
    if row["kind"] == "type":
        return [parts[-1]]
    if symbol.endswith(".(a:b:)") or symbol.endswith(".(lhs:rhs:)"):
        return ["operator=="]
    name = ALIASES.get(parts[-1], parts[-1])
    names = [name, snake(name)] if name else []
    if parts[-1] in ALIASES and ALIASES[parts[-1]]:
        names.append(ALIASES[parts[-1]])
    return [n for n in dict.fromkeys(names) if n]


def declaration(name, kind):
    word = re.escape(name)
    if kind in ("type", "init"):
        return re.compile(rf"\b(class|struct|enum class|enum|using)\s+{word}\b|\b{word}\s*\(")
    if name.startswith("operator"):
        return re.compile(re.escape(name))
    return re.compile(rf"(\bauto\s+{word}\s*\(|\b{word}\s*[\{{=;(]|\b{word}\b\s*,)")


def resolve(row, files):
    path, _, number = row["arisa"].rpartition(":")
    if not path or not number.isdigit():
        return None, "unparsable"
    lines = files.get(path)
    if lines is None:
        target = HEADERS / path
        if not target.exists():
            return None, "missing file"
        lines = target.read_text().split("\n")
        files[path] = lines
    old = int(number)
    hits = []
    for name in candidates(row):
        pattern = declaration(name, row["kind"])
        hits += [i + 1 for i, text in enumerate(lines) if pattern.search(text)]
    if not hits:
        return None, "no declaration"
    best = min(hits, key=lambda line: abs(line - old))
    return f"{path}:{best}", "ok"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    with LEDGER.open(newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        fields = reader.fieldnames
        rows = list(reader)
    files = {}
    moved = 0
    unresolved = []
    for row in rows:
        if row["status"] not in ("done", "partial") or not row["arisa"]:
            continue
        ref, state = resolve(row, files)
        if ref is None:
            unresolved.append((row["symbol"], row["arisa"], state))
            continue
        if ref != row["arisa"]:
            moved += 1
            row["arisa"] = ref
    if not args.check:
        with LEDGER.open("w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
    print(f"{moved} refs {'would move' if args.check else 'moved'}, {len(unresolved)} unresolved")
    for symbol, ref, state in unresolved:
        print(f"  {state}: {symbol} {ref}")
    return 1 if args.check and (moved or unresolved) else 0


if __name__ == "__main__":
    sys.exit(main())
