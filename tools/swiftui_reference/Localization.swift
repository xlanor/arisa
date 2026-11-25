import AppKit
import SwiftUI

enum Localization {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        mirroring(store, scheme)
        grid(store, scheme)
        images(store, scheme)
        numbers(store)
        dates(store)
        relative(store)
        casing(store)
        weight(store, scheme)
    }

    static func rtl<V: View>(_ view: V) -> some View {
        view.environment(\.layoutDirection, .rightToLeft)
    }

    static func box(_ width: CGFloat, _ name: String) -> some View {
        Color.blue.frame(width: width, height: 20).probe(name)
    }

    static func mirroring(_ store: Store, _ scheme: Scheme) {
        let stack = HStack(spacing: 10) { box(40, "a"); box(60, "b"); box(80, "c") }
            .frame(width: 300, alignment: .leading).probe("container")
        for (name, shot) in [("ltr", Harness.capture(stack, scheme: scheme)), ("rtl", Harness.capture(rtl(stack), scheme: scheme))] {
            let origin = shot.frame("container").minX
            for item in ["a", "b", "c"] {
                store.put("localization.hstack.\(name).\(item)_x", nil, shot.frame(item).minX - origin)
            }
        }
        let padded = box(40, "a").padding(.leading, 20).frame(width: 200, alignment: .leading).probe("container")
        let shot = Harness.capture(rtl(padded), scheme: scheme)
        store.put("localization.padding_leading.rtl.x", nil, shot.frame("a").minX - shot.frame("container").minX)
        let aligned = box(40, "a").frame(maxWidth: 200, alignment: .leading).probe("container")
        let framed = Harness.capture(rtl(aligned), scheme: scheme)
        store.put("localization.frame_leading.rtl.x", nil, framed.frame("a").minX - framed.frame("container").minX)
        let zstack = ZStack(alignment: .leading) { Color.gray.frame(width: 200, height: 40); box(40, "a") }.probe("container")
        let layered = Harness.capture(rtl(zstack), scheme: scheme)
        store.put("localization.zstack_leading.rtl.x", nil, layered.frame("a").minX - layered.frame("container").minX)
        let fixed = HStack(spacing: 10) { box(40, "a"); box(60, "b") }
            .layoutDirectionBehavior(.fixed).frame(width: 200, alignment: .leading).probe("container")
        let held = Harness.capture(rtl(fixed), scheme: scheme)
        store.put("localization.behavior_fixed.rtl.a_x", nil, held.frame("a").minX - held.frame("container").minX)
        store.put("localization.behavior_fixed.rtl.b_x", nil, held.frame("b").minX - held.frame("container").minX)
    }

    static func grid(_ store: Store, _ scheme: Scheme) {
        let view = Grid(horizontalSpacing: 10) {
            GridRow { box(40, "a"); box(60, "b"); box(80, "c") }
        }.frame(width: 300, alignment: .leading).probe("container")
        let shot = Harness.capture(rtl(view), scheme: scheme)
        let origin = shot.frame("container").minX
        for item in ["a", "b", "c"] {
            store.put("localization.grid.rtl.\(item)_x", nil, shot.frame(item).minX - origin)
        }
        let lazy = LazyVGrid(columns: [GridItem(.fixed(40)), GridItem(.fixed(60))], spacing: 10) { box(40, "a"); box(60, "b") }
            .frame(width: 200, alignment: .leading).probe("container")
        let flowed = Harness.capture(rtl(lazy), scheme: scheme)
        store.put("localization.lazy_grid.rtl.a_x", nil, flowed.frame("a").minX - flowed.frame("container").minX)
        store.put("localization.lazy_grid.rtl.b_x", nil, flowed.frame("b").minX - flowed.frame("container").minX)
    }

    static func images(_ store: Store, _ scheme: Scheme) {
        let plain = rtl(Image(systemName: "chevron.right").font(.system(size: 30)).probe("t"))
        let flipped = rtl(Image(systemName: "chevron.right").font(.system(size: 30)).flipsForRightToLeftLayoutDirection(true).probe("t"))
        let a = Harness.capture(plain, scheme: scheme)
        let b = Harness.capture(flipped, scheme: scheme)
        store.image("localization_chevron_rtl", a, a.frame("t"))
        store.image("localization_chevron_flipped_rtl", b, b.frame("t"))
        let slider = rtl(Slider(value: .constant(0.25)).frame(width: 200).probe("t"))
        let s = Harness.capture(slider, scheme: scheme)
        store.image("localization_slider_rtl", s, s.frame("t"))
        let r = s.raster
        let box = s.frame("t")
        let y = r.px(box.midY)
        let tint = r.flat(r.px(box.maxX - 10), y)
        let track = r.flat(r.px(box.minX + 10), y)
        store.colour("localization.slider.rtl.right_end", scheme, tint)
        store.colour("localization.slider.rtl.left_end", scheme, track)
    }

    static func numbers(_ store: Store) {
        let value = 1234567.891
        for tag in ["en_US", "de_DE", "fr_FR", "ja_JP", "es_ES", "ar", "he_IL", "ru_RU"] {
            let locale = Locale(identifier: tag)
            store.put("localization.number.\(tag)", nil, text: value.formatted(.number.locale(locale)), "text")
            store.put("localization.percent.\(tag)", nil, text: (0.256).formatted(.percent.locale(locale)), "text")
            store.put("localization.number_small.\(tag)", nil, text: (1234.5).formatted(.number.locale(locale)), "text")
        }
    }

    static func dates(_ store: Store) {
        let date = Date(timeIntervalSince1970: 1_795_352_700)
        let zone = TimeZone(secondsFromGMT: 0)!
        for tag in ["en_US", "en_GB", "de_DE", "fr_FR", "ja_JP", "ar", "he_IL"] {
            let locale = Locale(identifier: tag)
            let styles: [(String, Date.FormatStyle.DateStyle, Date.FormatStyle.TimeStyle)] = [
                ("numeric", .numeric, .omitted), ("abbreviated", .abbreviated, .omitted),
                ("long", .long, .omitted), ("complete", .complete, .omitted),
                ("time", .omitted, .shortened), ("both", .abbreviated, .shortened),
            ]
            for (name, date_style, time_style) in styles {
                var style = Date.FormatStyle(date: date_style, time: time_style)
                style.locale = locale
                style.timeZone = zone
                store.put("localization.date.\(name).\(tag)", nil, text: date.formatted(style), "text")
            }
        }
    }

    static func relative(_ store: Store) {
        for tag in ["en_US", "de_DE", "ja_JP", "ar"] {
            let formatter = RelativeDateTimeFormatter()
            formatter.locale = Locale(identifier: tag)
            formatter.unitsStyle = .full
            formatter.dateTimeStyle = .numeric
            for (name, offset) in [("past_hours", -3.0 * 3600), ("future_days", 2.0 * 86400), ("past_minute", -60.0)] {
                store.put("localization.relative.\(name).\(tag)", nil, text: formatter.localizedString(fromTimeInterval: offset), "text")
            }
        }
    }

    static func casing(_ store: Store) {
        store.put("localization.upper.tr", nil, text: "istanbul".uppercased(with: Locale(identifier: "tr")), "text")
        store.put("localization.lower.tr", nil, text: "ISTANBUL".lowercased(with: Locale(identifier: "tr")), "text")
        store.put("localization.upper.el", nil, text: "άλφα".uppercased(with: Locale(identifier: "el")), "text")
        store.put("localization.lower.sigma", nil, text: "ΟΔΟΣ".lowercased(with: Locale(identifier: "el")), "text")
    }

    static func weight(_ store: Store, _ scheme: Scheme) {
        let regular = Harness.capture(Text("Bold Text Sample").probe("t"), scheme: scheme).frame("t").width
        let bold = Harness.capture(Text("Bold Text Sample").environment(\.legibilityWeight, .bold).probe("t"), scheme: scheme).frame("t").width
        let semibold = Harness.capture(Text("Bold Text Sample").fontWeight(.semibold).probe("t"), scheme: scheme).frame("t").width
        store.put("localization.legibility.regular_width", nil, regular)
        store.put("localization.legibility.bold_width", nil, bold)
        store.put("localization.legibility.semibold_width", nil, semibold)
    }
}
