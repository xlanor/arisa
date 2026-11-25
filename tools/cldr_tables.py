import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree

LOCALES = [
    ("en", "en", "US"),
    ("en_GB", "en-GB", "GB"),
    ("ja", "ja", "JP"),
    ("fr", "fr", "FR"),
    ("fr_CA", "fr-CA", "CA"),
    ("de", "de", "DE"),
    ("it", "it", "IT"),
    ("es", "es", "ES"),
    ("es_419", "es-419", "MX"),
    ("zh", "zh-Hans", "CN"),
    ("zh_Hant", "zh-Hant", "TW"),
    ("ko", "ko", "KR"),
    ("nl", "nl", "NL"),
    ("pt", "pt", "BR"),
    ("pt_PT", "pt-PT", "PT"),
    ("ru", "ru", "RU"),
    ("ar", "ar", "EG"),
    ("he", "he", "IL"),
]

LENGTHS = ["full", "long", "medium", "short"]
UNITS = ["second", "minute", "hour", "day", "week", "month", "year"]
CATEGORIES = ["zero", "one", "two", "few", "many", "other"]
DAYS = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"]


class Cldr:
    def __init__(self, root):
        self.root = pathlib.Path(root)
        self.files = {}
        supplemental = ElementTree.parse(
            self.root / "common/supplemental/supplementalData.xml"
        ).getroot()
        self.parents = {}
        for entry in supplemental.iter("parentLocale"):
            if entry.get("component") is not None:
                continue
            for locale in entry.get("locales").split():
                self.parents[locale] = entry.get("parent")
        self.supplemental = supplemental
        self.plurals = ElementTree.parse(
            self.root / "common/supplemental/plurals.xml"
        ).getroot()

    def document(self, locale):
        if locale not in self.files:
            path = self.root / "common/main" / f"{locale}.xml"
            self.files[locale] = (
                ElementTree.parse(path).getroot() if path.exists() else None
            )
        return self.files[locale]

    def chain(self, locale):
        result = []
        current = locale
        while current is not None:
            result.append(current)
            if current == "root":
                break
            if current in self.parents:
                current = self.parents[current]
            elif "_" in current:
                current = current.rsplit("_", 1)[0]
            else:
                current = "root"
        return result

    def value(self, locale, xpath):
        for each in self.chain(locale):
            document = self.document(each)
            if document is None:
                continue
            for found in document.findall(xpath):
                if found.get("alt") is not None:
                    continue
                if found.text is None or found.text == "↑↑↑":
                    continue
                return found.text
        return None

    def plain_pattern(self, locale, kind):
        for each in self.chain(locale):
            document = self.document(each)
            if document is None:
                continue
            for formats in document.findall(f".//numbers/{kind}Formats"):
                if formats.get("numberSystem", "latn") != "latn":
                    continue
                for length in formats.findall(f"{kind}FormatLength"):
                    if length.get("type") is not None:
                        continue
                    for pattern in length.findall(f"{kind}Format/pattern"):
                        if pattern.get("alt") is None and pattern.text not in (None, "\u2191\u2191\u2191"):
                            return pattern.text
        raise KeyError(f"{locale}: {kind}")

    def first(self, locale, *xpaths):
        for xpath in xpaths:
            found = self.value(locale, xpath)
            if found is not None:
                return found
        raise KeyError(f"{locale}: {xpaths[0]}")

    def optional(self, locale, *xpaths):
        for xpath in xpaths:
            found = self.value(locale, xpath)
            if found is not None:
                return found
        return ""

    def plural_rules(self, language):
        for rules in self.plurals.iter("pluralRules"):
            if language in rules.get("locales").split():
                found = {}
                for rule in rules.iter("pluralRule"):
                    found[rule.get("count")] = rule.text.split("@")[0].strip()
                return found
        raise KeyError(language)

    def first_days(self):
        days = {}
        for entry in self.supplemental.iter("firstDay"):
            if entry.get("alt") is not None:
                continue
            for territory in entry.get("territories").split():
                days[territory] = DAYS.index(entry.get("day"))
        return days


GREGORIAN = ".//calendars/calendar[@type='gregorian']"


def months(cldr, locale, context, width):
    names = []
    for month in range(1, 13):
        names.append(
            cldr.first(
                locale,
                f"{GREGORIAN}/months/monthContext[@type='{context}']/monthWidth[@type='{width}']/month[@type='{month}']",
                f"{GREGORIAN}/months/monthContext[@type='format']/monthWidth[@type='{width}']/month[@type='{month}']",
                f"{GREGORIAN}/months/monthContext[@type='format']/monthWidth[@type='wide']/month[@type='{month}']",
            )
        )
    return names


def weekdays(cldr, locale, context, width):
    names = []
    for day in DAYS:
        names.append(
            cldr.first(
                locale,
                f"{GREGORIAN}/days/dayContext[@type='{context}']/dayWidth[@type='{width}']/day[@type='{day}']",
                f"{GREGORIAN}/days/dayContext[@type='format']/dayWidth[@type='{width}']/day[@type='{day}']",
                f"{GREGORIAN}/days/dayContext[@type='format']/dayWidth[@type='abbreviated']/day[@type='{day}']",
                f"{GREGORIAN}/days/dayContext[@type='format']/dayWidth[@type='wide']/day[@type='{day}']",
            )
        )
    return names


def period(cldr, locale, which):
    return cldr.first(
        locale,
        f"{GREGORIAN}/dayPeriods/dayPeriodContext[@type='format']/dayPeriodWidth[@type='abbreviated']/dayPeriod[@type='{which}']",
        f"{GREGORIAN}/dayPeriods/dayPeriodContext[@type='format']/dayPeriodWidth[@type='wide']/dayPeriod[@type='{which}']",
    )


def symbol(cldr, locale, name):
    return cldr.first(
        locale,
        f".//numbers/symbols[@numberSystem='latn']/{name}",
        f".//numbers/symbols/{name}",
    )


def relative(cldr, locale, unit, direction, category):
    return cldr.optional(
        locale,
        f".//dates/fields/field[@type='{unit}']/relativeTime[@type='{direction}']/relativeTimePattern[@count='{category}']",
    )


def record(cldr, locale, identifier, region):
    language = locale.split("_")[0]
    rules = cldr.plural_rules(language)
    entry = {
        "identifier": identifier,
        "region": region,
        "decimal": symbol(cldr, locale, "decimal"),
        "group": symbol(cldr, locale, "group"),
        "minus": symbol(cldr, locale, "minusSign"),
        "plus": symbol(cldr, locale, "plusSign"),
        "percent_sign": symbol(cldr, locale, "percentSign"),
        "decimal_pattern": cldr.plain_pattern(locale, "decimal"),
        "percent_pattern": cldr.plain_pattern(locale, "percent"),
        "minimum_grouping": int(
            cldr.optional(locale, ".//numbers/minimumGroupingDigits") or "1"
        ),
        "months_wide": months(cldr, locale, "format", "wide"),
        "months_abbreviated": months(cldr, locale, "format", "abbreviated"),
        "months_standalone": months(cldr, locale, "stand-alone", "wide"),
        "weekdays_wide": weekdays(cldr, locale, "format", "wide"),
        "weekdays_abbreviated": weekdays(cldr, locale, "format", "abbreviated"),
        "weekdays_short": weekdays(cldr, locale, "format", "short"),
        "weekdays_narrow": weekdays(cldr, locale, "stand-alone", "narrow"),
        "am": period(cldr, locale, "am"),
        "pm": period(cldr, locale, "pm"),
        "date_patterns": [
            cldr.first(
                locale,
                f"{GREGORIAN}/dateFormats/dateFormatLength[@type='{length}']/dateFormat/pattern",
            )
            for length in LENGTHS
        ],
        "time_patterns": [
            cldr.first(
                locale,
                f"{GREGORIAN}/timeFormats/timeFormatLength[@type='{length}']/timeFormat/pattern",
            )
            for length in LENGTHS
        ],
        "date_time_patterns": [
            cldr.first(
                locale,
                f"{GREGORIAN}/dateTimeFormats/dateTimeFormatLength[@type='{length}']/dateTimeFormat[@type='standard']/pattern",
                f"{GREGORIAN}/dateTimeFormats/dateTimeFormatLength[@type='{length}']/dateTimeFormat/pattern",
            )
            for length in LENGTHS
        ],
        "month_year": cldr.first(
            locale,
            f"{GREGORIAN}/dateTimeFormats/availableFormats/dateFormatItem[@id='yMMMM']",
        ),
        "numeric_date": cldr.first(
            locale,
            f"{GREGORIAN}/dateTimeFormats/availableFormats/dateFormatItem[@id='yMd']",
        ),
        "abbreviated_date": cldr.first(
            locale,
            f"{GREGORIAN}/dateTimeFormats/availableFormats/dateFormatItem[@id='yMMMd']",
        ),
        "plural_rules": [rules.get(category, "") for category in CATEGORIES[:5]],
        "relative": [
            [
                [relative(cldr, locale, unit, direction, category) for category in CATEGORIES]
                for direction in ["past", "future"]
            ]
            for unit in UNITS
        ],
    }
    return entry


def literal(text):
    escaped = []
    for character in text:
        code = ord(character)
        if character == "\\":
            escaped.append("\\\\")
        elif character == '"':
            escaped.append('\\"')
        elif code < 0x20 or code == 0x7F:
            escaped.append(f"\\x{code:02x}")
        else:
            escaped.append(character)
    return '"' + "".join(escaped) + '"'


def array(values):
    return "{" + ", ".join(literal(value) for value in values) + "}"


def emit(cldr, output):
    records = [record(cldr, locale, identifier, region) for locale, identifier, region in LOCALES]
    lines = [
        "#include <text/locale_record.hpp>",
        "",
        "#include <array>",
        "#include <span>",
        "#include <utility>",
        "",
        "namespace arisa::text {",
        "",
        "auto LocaleData::records() noexcept -> std::span<const Record> {",
        f"  static constexpr std::array<Record, {len(records)}> table{{{{",
    ]
    for entry in records:
        relative_rows = []
        for unit in entry["relative"]:
            directions = ", ".join("{" + array(categories)[1:-1] + "}" for categories in unit)
            relative_rows.append("{{" + directions + "}}")
        lines.append("    Record{")
        lines.append(f"        .identifier = {literal(entry['identifier'])},")
        lines.append(f"        .region = {literal(entry['region'])},")
        for name in ["decimal", "group", "minus", "plus", "percent_sign", "decimal_pattern", "percent_pattern"]:
            lines.append(f"        .{name} = {literal(entry[name])},")
        lines.append(f"        .minimum_grouping = {entry['minimum_grouping']},")
        for name in [
            "months_wide",
            "months_abbreviated",
            "months_standalone",
            "weekdays_wide",
            "weekdays_abbreviated",
            "weekdays_short",
            "weekdays_narrow",
        ]:
            lines.append(f"        .{name} = {{{array(entry[name])[1:-1]}}},")
        lines.append(f"        .am = {literal(entry['am'])},")
        lines.append(f"        .pm = {literal(entry['pm'])},")
        for name in ["date_patterns", "time_patterns", "date_time_patterns"]:
            lines.append(f"        .{name} = {{{array(entry[name])[1:-1]}}},")
        for name in ["month_year", "numeric_date", "abbreviated_date"]:
            lines.append(f"        .{name} = {literal(entry[name])},")
        lines.append(f"        .plural_rules = {{{array(entry['plural_rules'])[1:-1]}}},")
        lines.append("        .relative = {{" + ", ".join(relative_rows) + "}},")
        lines.append("    },")
    lines.append("}};")
    lines.append("  return table;")
    lines.append("}")
    days = sorted((territory, day) for territory, day in cldr.first_days().items() if day != 1)
    lines.append("")
    lines.append("auto LocaleData::first_days() noexcept")
    lines.append("    -> std::span<const std::pair<std::string_view, unsigned>> {")
    lines.append(f"  static constexpr std::array<std::pair<std::string_view, unsigned>, {len(days)}> table{{{{")
    for territory, day in days:
        lines.append(f"    {{{literal(territory)}, {day}U}},")
    lines.append("}};")
    lines.append("  return table;")
    lines.append("}")
    lines.append("")
    lines.append("}")
    path = pathlib.Path(output)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    subprocess.run(["clang-format", "-i", str(path)], check=True)


def main():
    if len(sys.argv) != 3:
        print("usage: cldr_tables.py <cldr-core-directory> <output.cpp>", file=sys.stderr)
        return 2
    emit(Cldr(sys.argv[1]), sys.argv[2])
    return 0


if __name__ == "__main__":
    sys.exit(main())
