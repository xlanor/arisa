import Foundation
import SwiftUI

enum Formats {
    static let locales: [(String, String)] = [
        ("en", "en_US"), ("en-GB", "en_GB"), ("ja", "ja_JP"), ("fr", "fr_FR"), ("fr-CA", "fr_CA"),
        ("de", "de_DE"), ("it", "it_IT"), ("es", "es_ES"), ("es-419", "es_MX"), ("zh-Hans", "zh_Hans_CN"),
        ("zh-Hant", "zh_Hant_TW"), ("ko", "ko_KR"), ("nl", "nl_NL"), ("pt", "pt_BR"), ("pt-PT", "pt_PT"),
        ("ru", "ru_RU"), ("ar", "ar_EG@numbers=latn"), ("he", "he_IL"),
    ]
    static let samples: [Double] = [0, 1, 2, 3, 5, 11, 21, 22, 100, 101, 1_000_000, 1.5]
    static let codes = ["USD", "EUR", "JPY", "GBP", "CNY", "KRW", "CAD", "AUD", "CHF", "HKD", "TWD", "BRL", "RUB", "INR", "MXN", "ILS", "EGP", "SEK"]

    static var lines: [String] = []

    static func put(_ locale: String, _ kind: String, _ key: String, _ sample: String, _ text: String) {
        let clean = text.replacingOccurrences(of: "\t", with: " ").replacingOccurrences(of: "\n", with: " ")
        lines.append("\(locale)\t\(kind)\t\(key)\t\(sample)\t\(clean)")
    }

    static func sampleName(_ value: Double) -> String {
        value == value.rounded() ? String(Int64(value)) : String(value)
    }

    static func run(_ store: Store) {
        lines = []
        for (id, posix) in locales {
            let locale = Locale(identifier: posix)
            numbers(id, locale)
            currency(id, locale)
            bytes(id, locale)
            measurements(id, locale)
            durations(id, locale)
            named(id, locale)
        }
        live()
        for code in codes {
            let formatter = NumberFormatter()
            formatter.numberStyle = .currency
            formatter.currencyCode = code
            put("*", "currency_digits", code, "", String(formatter.maximumFractionDigits))
        }
        let text = "locale\tkind\tkey\tsample\ttext\n" + lines.joined(separator: "\n") + "\n"
        try? text.write(to: store.imageDirectory.appendingPathComponent("formats.tsv"), atomically: true, encoding: .utf8)
        store.put("formats.rows", nil, Double(lines.count), "count")
    }

    static func numbers(_ id: String, _ locale: Locale) {
        let formatter = NumberFormatter()
        formatter.locale = locale
        formatter.numberStyle = .decimal
        formatter.maximumFractionDigits = 3
        for value in samples {
            put(id, "number", "decimal", sampleName(value), formatter.string(from: NSNumber(value: value)) ?? "")
        }
    }

    static func currency(_ id: String, _ locale: Locale) {
        let formatter = NumberFormatter()
        formatter.locale = locale
        formatter.numberStyle = .currency
        put(id, "currency_pattern", "symbol", "", formatter.positiveFormat)
        put(id, "currency_pattern", "symbol_negative", "", formatter.negativeFormat)
        let iso = NumberFormatter()
        iso.locale = locale
        iso.numberStyle = .currencyISOCode
        put(id, "currency_pattern", "iso", "", iso.positiveFormat)
        for code in codes {
            formatter.currencyCode = code
            put(id, "currency_symbol", code, "", formatter.currencySymbol)
            put(id, "currency_sample", code, "1234.5", 1234.5.formatted(.currency(code: code).locale(locale)))
            put(id, "currency_sample", code, "-0.5", (-0.5).formatted(.currency(code: code).locale(locale)))
            put(id, "currency_sample_iso", code, "1234.5", 1234.5.formatted(.currency(code: code).presentation(.isoCode).locale(locale)))
        }
    }

    static func bytes(_ id: String, _ locale: Locale) {
        let values: [Int64] = [0, 1, 2, 5, 21, 499, 500, 501, 900, 949, 950, 999, 1000, 1024, 1500, 1536, 12_345, 1_500_000, 1_572_864, 999_499, 999_500, 1_049_999, 99_950_000, 999_999_999, 1_005_000_000, 3_221_225_472, 2_000_000_000_000, 5_000_000_000_000_000]
        let styles: [(String, ByteCountFormatStyle.Style)] = [("file", .file), ("memory", .memory), ("decimal", .decimal), ("binary", .binary)]
        for (name, style) in styles {
            for value in values {
                put(id, "bytes", name, String(value), value.formatted(.byteCount(style: style).locale(locale)))
            }
        }
        for value: Int64 in [0, 1, 2, 5, 21] {
            put(id, "bytes_unit", "bytes", String(value), value.formatted(.byteCount(style: .file, allowedUnits: .bytes, spellsOutZero: false).locale(locale)))
        }
        let units: [(String, ByteCountFormatStyle.Units, Int64)] = [("kb", .kb, 1000), ("mb", .mb, 1_000_000), ("gb", .gb, 1_000_000_000), ("tb", .tb, 1_000_000_000_000), ("pb", .pb, 1_000_000_000_000_000)]
        for (name, unit, base) in units {
            put(id, "bytes_unit", name, "2", (base * 2).formatted(.byteCount(style: .file, allowedUnits: unit, spellsOutZero: false).locale(locale)))
        }
        put(id, "bytes_zero", "file", "0", Int64(0).formatted(.byteCount(style: .file, spellsOutZero: true).locale(locale)))
        put(id, "bytes_actual", "file", "1500000", Int64(1_500_000).formatted(.byteCount(style: .file, includesActualByteCount: true).locale(locale)))
    }

    static let lengthUnits: [(String, Dimension)] = [
        ("meter", UnitLength.meters), ("kilometer", UnitLength.kilometers), ("centimeter", UnitLength.centimeters),
        ("millimeter", UnitLength.millimeters), ("mile", UnitLength.miles), ("foot", UnitLength.feet),
        ("inch", UnitLength.inches), ("yard", UnitLength.yards),
        ("kilogram", UnitMass.kilograms), ("gram", UnitMass.grams), ("pound", UnitMass.pounds), ("ounce", UnitMass.ounces),
        ("celsius", UnitTemperature.celsius), ("fahrenheit", UnitTemperature.fahrenheit),
        ("liter", UnitVolume.liters), ("milliliter", UnitVolume.milliliters),
        ("kilometer-per-hour", UnitSpeed.kilometersPerHour), ("mile-per-hour", UnitSpeed.milesPerHour),
        ("meter-per-second", UnitSpeed.metersPerSecond),
        ("second", UnitDuration.seconds), ("minute", UnitDuration.minutes), ("hour", UnitDuration.hours),
    ]

    static func measurements(_ id: String, _ locale: Locale) {
        let widths: [(String, Measurement<Dimension>.FormatStyle.UnitWidth)] = [("wide", .wide), ("abbreviated", .abbreviated), ("narrow", .narrow)]
        for (name, unit) in lengthUnits {
            for (width, style) in widths {
                for value in samples {
                    let measurement = Measurement<Dimension>(value: value, unit: unit)
                    let format = Measurement<Dimension>.FormatStyle(width: style, locale: locale, usage: .asProvided)
                    put(id, "measurement", "\(name).\(width)", sampleName(value), measurement.formatted(format))
                }
            }
        }
    }

    static func named(_ id: String, _ locale: Locale) {
        let formatter = RelativeDateTimeFormatter()
        formatter.locale = locale
        formatter.dateTimeStyle = .named
        put(id, "named", "now", "", formatter.localizedString(fromTimeInterval: 0))
        put(id, "named", "yesterday", "", formatter.localizedString(from: DateComponents(day: -1)))
        put(id, "named", "tomorrow", "", formatter.localizedString(from: DateComponents(day: 1)))
    }

    static func durations(_ id: String, _ locale: Locale) {
        var calendar = Calendar(identifier: .gregorian)
        calendar.locale = locale
        let units: [(String, NSCalendar.Unit, Calendar.Component)] = [
            ("year", .year, .year), ("month", .month, .month), ("week", .weekOfMonth, .weekOfMonth), ("day", .day, .day),
            ("hour", .hour, .hour), ("minute", .minute, .minute), ("second", .second, .second),
        ]
        for (styleName, style) in [("wide", DateComponentsFormatter.UnitsStyle.full), ("abbreviated", .abbreviated)] {
            for (name, unit, component) in units {
                for value in samples where value == value.rounded() && value < 1_000_000 {
                    let formatter = DateComponentsFormatter()
                    formatter.calendar = calendar
                    formatter.unitsStyle = style
                    formatter.allowedUnits = unit
                    var parts = DateComponents()
                    parts.setValue(Int(value), for: component)
                    put(id, "duration", "\(name).\(styleName)", sampleName(value), formatter.string(from: parts) ?? "")
                }
            }
            let formatter = DateComponentsFormatter()
            formatter.calendar = calendar
            formatter.unitsStyle = style
            formatter.allowedUnits = [.hour, .minute]
            put(id, "duration_pair", styleName, "2h5m", formatter.string(from: DateComponents(hour: 2, minute: 5)) ?? "")
        }
        let now = Date(timeIntervalSinceReferenceDate: 800_000_000)
        let past = now.addingTimeInterval(-(2 * 3600 + 5 * 60))
        put(id, "live_offset_pair", "past_2h5m", "", String(SystemFormatStyle.DateOffset(to: past).locale(locale).format(now).characters))
        put(id, "live_offset_pair", "future_2h5m", "", String(SystemFormatStyle.DateOffset(to: now.addingTimeInterval(2 * 3600 + 5 * 60)).locale(locale).format(now).characters))
        put(id, "live_offset_pair", "past_3d1h_always", "", String(SystemFormatStyle.DateOffset(to: now.addingTimeInterval(-3 * 86_400 - 3600), sign: .always()).locale(locale).format(now).characters))
    }

    static func live() {
        let now = Date(timeIntervalSinceReferenceDate: 800_000_000)
        let en = Locale(identifier: "en_US")
        let cases: [(String, Date)] = [
            ("future_2h5m7s", now.addingTimeInterval(2 * 3600 + 5 * 60 + 7)),
            ("past_40s", now.addingTimeInterval(-40)),
            ("past_3d1h", now.addingTimeInterval(-3 * 86_400 - 3600)),
            ("future_95s", now.addingTimeInterval(95)),
            ("past_400d", now.addingTimeInterval(-400 * 86_400)),
        ]
        for (name, anchor) in cases {
            let reference = SystemFormatStyle.DateReference(to: anchor).locale(en)
            put("en", "live_reference", name, "", String(reference.format(now).characters))
            put("en", "live_reference_next", name, "", String(format: "%.3f", (reference.discreteInput(after: now) ?? now).timeIntervalSince(now)))
            let offset = SystemFormatStyle.DateOffset(to: anchor).locale(en)
            put("en", "live_offset", name, "", String(offset.format(now).characters))
            put("en", "live_offset_next", name, "", String(format: "%.3f", (offset.discreteInput(after: now) ?? now).timeIntervalSince(now)))
            let single = SystemFormatStyle.DateOffset(to: anchor, maxFieldCount: 1).locale(en)
            put("en", "live_offset_one", name, "", String(single.format(now).characters))
            let range = min(anchor, now)..<max(anchor, now).addingTimeInterval(1)
            let down = SystemFormatStyle.Timer(countingDownIn: range).locale(en)
            put("en", "live_timer_down", name, "", String(down.format(now).characters))
            put("en", "live_timer_down_next", name, "", String(format: "%.3f", (down.discreteInput(after: now) ?? now).timeIntervalSince(now)))
            let up = SystemFormatStyle.Timer(countingUpIn: range).locale(en)
            put("en", "live_timer_up", name, "", String(up.format(now).characters))
            let hours = SystemFormatStyle.Timer(countingUpIn: range, showsHours: false).locale(en)
            put("en", "live_timer_no_hours", name, "", String(hours.format(now).characters))
        }
        let watch = SystemFormatStyle.Stopwatch(startingAt: now.addingTimeInterval(-65.3)).locale(en)
        put("en", "live_stopwatch", "default", "", String(watch.format(now).characters))
        put("en", "live_stopwatch_next", "default", "", String(format: "%.3f", (watch.discreteInput(after: now) ?? now).timeIntervalSince(now)))
        let coarse = SystemFormatStyle.Stopwatch(startingAt: now.addingTimeInterval(-3725.3), showsHours: true, maxPrecision: .seconds(1)).locale(en)
        put("en", "live_stopwatch", "hours_seconds", "", String(coarse.format(now).characters))
        let fine = SystemFormatStyle.Stopwatch(startingAt: now.addingTimeInterval(-5.25), maxPrecision: .milliseconds(100)).locale(en)
        put("en", "live_stopwatch", "tenths", "", String(fine.format(now).characters))
    }
}
