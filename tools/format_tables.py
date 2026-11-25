import csv
import pathlib
import re
import subprocess
import sys

LOCALES = [
    "en", "en-GB", "ja", "fr", "fr-CA", "de", "it", "es", "es-419", "zh-Hans",
    "zh-Hant", "ko", "nl", "pt", "pt-PT", "ru", "ar", "he",
]
CATEGORIES = ["zero", "one", "two", "few", "many", "other"]
CODES = ["USD", "EUR", "JPY", "GBP", "CNY", "KRW", "CAD", "AUD", "CHF", "HKD", "TWD", "BRL", "RUB", "INR", "MXN", "ILS", "EGP", "SEK"]
BYTE_UNITS = ["kb", "mb", "gb", "tb", "pb"]
DURATIONS = ["second", "minute", "hour", "day", "week", "month", "year"]
UNITS = [
    "meter", "kilometer", "centimeter", "millimeter", "mile", "foot", "inch", "yard",
    "kilogram", "gram", "pound", "ounce", "celsius", "fahrenheit", "liter", "milliliter",
    "kilometer-per-hour", "mile-per-hour", "meter-per-second", "second", "minute", "hour",
]
WIDTHS = ["wide", "abbreviated", "narrow"]


class Rules:
    def __init__(self, conditions):
        self.conditions = conditions

    @staticmethod
    def operands(sample):
        text = sample
        if "." in text:
            whole, fraction = text.split(".")
        else:
            whole, fraction = text, ""
        n = float(text)
        i = int(whole)
        v = len(fraction)
        f = int(fraction) if fraction else 0
        t = int(fraction.rstrip("0")) if fraction.rstrip("0") else 0
        w = len(fraction.rstrip("0"))
        return {"n": n, "i": i, "v": v, "f": f, "t": t, "w": w, "e": 0, "c": 0}

    @staticmethod
    def relation(text, values):
        match = re.match(r"\s*([nivftwec])\s*(?:%\s*(\d+))?\s*(!=|=)\s*(.+)", text)
        name, modulo, operator, ranges = match.groups()
        value = values[name]
        if modulo:
            value = value % int(modulo)
        hit = False
        for part in ranges.split(","):
            part = part.strip()
            if ".." in part:
                low, high = part.split("..")
                hit = hit or (value == int(value) and int(low) <= value <= int(high))
            else:
                hit = hit or value == float(part)
        return hit if operator == "=" else not hit

    def category(self, sample):
        values = self.operands(sample)
        for name, condition in zip(CATEGORIES[:5], self.conditions):
            if not condition:
                continue
            if any(all(self.relation(r, values) for r in conjunction.split(" and ")) for conjunction in condition.split(" or ")):
                return name
        return "other"


def plural_rules(source):
    text = pathlib.Path(source).read_text(encoding="utf-8")
    found = {}
    for chunk in text.split("Record{")[1:]:
        identifier = re.search(r'\.identifier = "([^"]+)"', chunk).group(1)
        rules = re.search(r"\.plural_rules\s*=\s*\{(.*?)\},\s*\.relative", chunk, re.S).group(1)
        groups = re.findall(r'((?:"(?:[^"\\]|\\.)*"\s*)+)', rules)
        found[identifier] = Rules(["".join(re.findall(r'"((?:[^"\\]|\\.)*)"', group)) for group in groups])
    return found


def literal(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def load(path):
    rows = {}
    with open(path, encoding="utf-8") as handle:
        reader = csv.reader(handle, delimiter="\t", quoting=csv.QUOTE_NONE)
        next(reader)
        for row in reader:
            if len(row) < 5:
                continue
            locale, kind, key, sample, text = row[:5]
            rows.setdefault((locale, kind, key), []).append((sample, text))
    return rows


def number_of(rows, locale, sample):
    for found, text in rows.get((locale, "number", "decimal"), []):
        if found == sample:
            return text
    return sample


def patterns(rows, rules, locale, kind, key):
    chosen = {}
    for sample, text in rows.get((locale, kind, key), []):
        category = rules[locale].category(sample)
        if category in chosen:
            continue
        number = number_of(rows, locale, sample)
        if number in text:
            chosen[category] = text.replace(number, "{0}", 1)
        else:
            chosen[category] = text
    if "other" not in chosen and chosen:
        chosen["other"] = chosen.get("many") or next(iter(chosen.values()))
    other = chosen.get("other", "")
    return [chosen.get(category, "") if chosen.get(category, "") != other or category == "other" else "" for category in CATEGORIES]


def single(rows, locale, kind, key):
    found = rows.get((locale, kind, key), [])
    return found[0][1] if found else ""


def template(rows, locale, kind, sample, code, symbol=None):
    text = ""
    for found, value in rows.get((locale, kind, code), []):
        if found == sample:
            text = value
    if symbol is None:
        symbol = single(rows, locale, "currency_symbol", code)
    match = re.search(r"[0-9](?:[0-9.,\u00a0\u202f\u2019' ]*[0-9])?", text)
    if not match:
        return "{1}{0}"
    text = text[:match.start()] + "{0}" + text[match.end():]
    minus = text.find("-")
    if minus >= 0:
        text = text[:minus] + "{2}" + text[minus + 1:]
    return text.replace(symbol, "{1}", 1)


def pair_pattern(rows, rules, locale):
    text = single(rows, locale, "live_offset_pair", "past_2h5m")
    first = dict(rows.get((locale, "duration", "hour.wide"), [])).get("2", "")
    second = dict(rows.get((locale, "duration", "minute.wide"), [])).get("5", "")
    if first and second and first in text and second in text:
        return text.replace(first, "{0}", 1).replace(second, "{1}", 1)
    return "{0}, {1}"


def array(values):
    return "{" + ", ".join(literal(value) for value in values) + "}"


def emit(rows, rules, output):
    lines = [
        "#include <text/format_record.hpp>",
        "",
        "#include <array>",
        "#include <span>",
        "",
        "namespace arisa::text {",
        "",
        "auto LocaleData::formats() noexcept -> std::span<const FormatRecord> {",
        f"  static constexpr std::array<FormatRecord, {len(LOCALES)}> table{{{{",
    ]
    for locale in LOCALES:
        lines.append("    FormatRecord{")
        lines.append(f"        .identifier = {literal(locale)},")
        lines.append(f"        .currency = {literal(template(rows, locale, 'currency_sample', '1234.5', 'EUR'))},")
        lines.append(f"        .currency_negative = {literal(template(rows, locale, 'currency_sample', '-0.5', 'EUR'))},")
        lines.append(f"        .currency_iso = {literal(template(rows, locale, 'currency_sample_iso', '1234.5', 'EUR', 'EUR'))},")
        lines.append(f"        .currency_symbols = {array([single(rows, locale, 'currency_symbol', code) for code in CODES])},")
        lines.append(f"        .bytes = {array(patterns(rows, rules, locale, 'bytes_unit', 'bytes'))},")
        units = []
        for unit in BYTE_UNITS:
            text = single(rows, locale, "bytes_unit", unit)
            number = number_of(rows, locale, "2")
            units.append(text.replace(number, "{0}", 1))
        lines.append(f"        .byte_units = {array(units)},")
        lines.append(f"        .zero_bytes = {literal(single(rows, locale, 'bytes_zero', 'file'))},")
        lines.append(f"        .pair = {literal(pair_pattern(rows, rules, locale))},")
        lines.append(f"        .now = {literal(single(rows, locale, 'named', 'now'))},")
        lines.append(f"        .yesterday = {literal(single(rows, locale, 'named', 'yesterday'))},")
        lines.append(f"        .tomorrow = {literal(single(rows, locale, 'named', 'tomorrow'))},")
        groups = []
        for width in ["wide", "abbreviated"]:
            group = [array(patterns(rows, rules, locale, "duration", f"{unit}.{width}")) for unit in DURATIONS]
            groups.append("{{" + ", ".join(group) + "}}")
        lines.append("        .durations = {{" + ", ".join(groups) + "}},")
        groups = []
        for unit in UNITS:
            group = [array(patterns(rows, rules, locale, "measurement", f"{unit}.{width}")) for width in WIDTHS]
            groups.append("{{" + ", ".join(group) + "}}")
        lines.append("        .units = {{" + ", ".join(groups) + "}},")
        lines.append("    },")
    lines.append("}};")
    lines.append("  return table;")
    lines.append("}")
    lines.append("")
    lines.append("auto LocaleData::currency_digits() noexcept")
    lines.append("    -> std::span<const std::pair<std::string_view, unsigned>> {")
    digits = [(code, single(rows, "*", "currency_digits", code) or "2") for code in CODES]
    lines.append(f"  static constexpr std::array<std::pair<std::string_view, unsigned>, {len(digits)}> table{{{{")
    for code, value in digits:
        lines.append(f"    {{{literal(code)}, {value}U}},")
    lines.append("}};")
    lines.append("  return table;")
    lines.append("}")
    lines.append("")
    lines.append("}")
    path = pathlib.Path(output)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    subprocess.run(["clang-format", "-i", str(path)], check=True)


def main():
    if len(sys.argv) != 4:
        print("usage: format_tables.py <formats.tsv> <locale_tables.cpp> <output.cpp>", file=sys.stderr)
        return 2
    emit(load(sys.argv[1]), plural_rules(sys.argv[2]), sys.argv[3])
    return 0


if __name__ == "__main__":
    sys.exit(main())
