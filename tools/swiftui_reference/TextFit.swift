import AppKit
import SwiftUI

enum TextFit {
    static let sentence = "The quick brown fox jumps over the lazy dog"

    static func run(_ store: Store) {
        let scheme = Scheme.light
        spacing(store, scheme)
        glyphs(store, scheme)
        fitting(store, scheme)
        alignment(store, scheme)
        aspect(store, scheme)
    }

    static func ink(_ shot: Shot, _ name: String) -> CGRect {
        let box = shot.frame(name)
        return shot.raster.differingBounds(in: box.insetBy(dx: -4, dy: -4), from: shot.raster.background, tolerance: 0.25) ?? .zero
    }

    static func height<V: View>(_ view: V, _ scheme: Scheme) -> CGFloat {
        Harness.capture(view.probe("t"), scheme: scheme).frame("t").height
    }

    static func width<V: View>(_ view: V, _ scheme: Scheme) -> CGFloat {
        Harness.capture(view.probe("t"), scheme: scheme).frame("t").width
    }

    static func spacing(_ store: Store, _ scheme: Scheme) {
        let one = height(Text("Ab"), scheme)
        let two = height(Text("Ab\nAb"), scheme)
        let three = height(Text("Ab\nAb\nAb"), scheme)
        store.put("text_fit.line.one", nil, one)
        store.put("text_fit.line.two", nil, two)
        store.put("text_fit.line.three", nil, three)
        store.put("text_fit.line_spacing10.one", nil, height(Text("Ab").lineSpacing(10), scheme))
        store.put("text_fit.line_spacing10.two", nil, height(Text("Ab\nAb").lineSpacing(10), scheme))
        store.put("text_fit.line_spacing10.three", nil, height(Text("Ab\nAb\nAb").lineSpacing(10), scheme))
        store.put("text_fit.reserves.limit3_one_line", nil, height(Text("Ab").lineLimit(3, reservesSpace: true), scheme))
        store.put("text_fit.reserves.limit3_spacing10", nil, height(Text("Ab").lineLimit(3, reservesSpace: true).lineSpacing(10), scheme))
        store.put("text_fit.reserves.limit2_five_lines", nil, height(Text("A\nB\nC\nD\nE").lineLimit(2, reservesSpace: true), scheme))
        store.put("text_fit.line_limit2.five_lines", nil, height(Text("A\nB\nC\nD\nE").lineLimit(2), scheme))
        let plain = Harness.capture(Text("Ab").probe("t"), scheme: scheme)
        let raised = Harness.capture(Text("Ab").baselineOffset(8).probe("t"), scheme: scheme)
        let lowered = Harness.capture(Text("Ab").baselineOffset(-8).probe("t"), scheme: scheme)
        store.put("text_fit.baseline8.height", nil, raised.frame("t").height)
        store.put("text_fit.baseline-8.height", nil, lowered.frame("t").height)
        store.put("text_fit.baseline0.ink_top", nil, ink(plain, "t").minY - plain.frame("t").minY)
        store.put("text_fit.baseline8.ink_top", nil, ink(raised, "t").minY - raised.frame("t").minY)
        store.put("text_fit.baseline-8.ink_top", nil, ink(lowered, "t").minY - lowered.frame("t").minY)
        store.image("text_fit_baseline", raised, raised.frame("t"))
    }

    static func glyphs(_ store: Store, _ scheme: Scheme) {
        let base = width(Text("AAAA"), scheme)
        store.put("text_fit.glyph.plain_AAAA", nil, base)
        store.put("text_fit.glyph.kerning5_AAAA", nil, width(Text("AAAA").kerning(5), scheme))
        store.put("text_fit.glyph.tracking5_AAAA", nil, width(Text("AAAA").tracking(5), scheme))
        store.put("text_fit.glyph.plain_office", nil, width(Text("office"), scheme))
        store.put("text_fit.glyph.kerning5_office", nil, width(Text("office").kerning(5), scheme))
        store.put("text_fit.glyph.tracking5_office", nil, width(Text("office").tracking(5), scheme))
        store.put("text_fit.digits.plain_111", nil, width(Text("111"), scheme))
        store.put("text_fit.digits.plain_888", nil, width(Text("888"), scheme))
        store.put("text_fit.digits.monospaced_digit_111", nil, width(Text("111").monospacedDigit(), scheme))
        store.put("text_fit.digits.monospaced_digit_888", nil, width(Text("888").monospacedDigit(), scheme))
        store.put("text_fit.digits.monospaced_111", nil, width(Text("111").monospaced(), scheme))
        store.put("text_fit.digits.monospaced_iii", nil, width(Text("iii").monospaced(), scheme))
        store.put("text_fit.case.upper", nil, width(Text("Hello").textCase(.uppercase), scheme))
        store.put("text_fit.case.HELLO", nil, width(Text("HELLO"), scheme))
        store.put("text_fit.case.lower", nil, width(Text("HeLLo").textCase(.lowercase), scheme))
        store.put("text_fit.case.hello", nil, width(Text("hello"), scheme))
    }

    static func fitting(_ store: Store, _ scheme: Scheme) {
        let natural = width(Text(sentence), scheme)
        store.put("text_fit.fit.natural_width", nil, natural)
        let plain = Harness.capture(Text(sentence).probe("t"), scheme: scheme)
        let plainInk = ink(plain, "t")
        for factor in [0.5, 0.8] {
            let shot = Harness.capture(Text(sentence).lineLimit(1).minimumScaleFactor(factor).frame(width: natural * 0.6, alignment: .leading).probe("t"), scheme: scheme)
            let shrunk = ink(shot, "t")
            store.put("text_fit.minimum_scale.\(Store.format(factor)).ink_height_ratio", nil, shrunk.height / plainInk.height, "ratio")
            store.put("text_fit.minimum_scale.\(Store.format(factor)).ink_width", nil, shrunk.width)
            store.put("text_fit.minimum_scale.\(Store.format(factor)).frame_height", nil, shot.frame("t").height)
            store.image("text_fit_minimum_scale_\(Store.format(factor))", shot, shot.frame("t"))
        }
        let wrapped = Harness.capture(Text(sentence).minimumScaleFactor(0.5).frame(width: natural * 0.6, height: plain.frame("t").height).probe("t"), scheme: scheme)
        store.put("text_fit.minimum_scale.no_limit_fixed_height.ink_height_ratio", nil, ink(wrapped, "t").height / plainInk.height, "ratio")
        for fraction in [0.99, 0.98, 0.97, 0.96, 0.95, 0.94, 0.92, 0.9] {
            let w = natural * fraction
            let tight = Harness.capture(Text(sentence).lineLimit(1).allowsTightening(true).frame(width: w, alignment: .leading).probe("t"), scheme: scheme)
            let loose = Harness.capture(Text(sentence).lineLimit(1).frame(width: w, alignment: .leading).probe("t"), scheme: scheme)
            let key = "text_fit.tightening.\(Store.format(fraction))"
            store.put("\(key).tight_ink_width", nil, ink(tight, "t").width)
            store.put("\(key).loose_ink_width", nil, ink(loose, "t").width)
            if fraction == 0.96 {
                store.image("text_fit_tightening", tight, tight.frame("t"))
            }
        }
        for (name, mode) in [("head", Text.TruncationMode.head), ("middle", .middle), ("tail", .tail)] {
            let shot = Harness.capture(Text(sentence).lineLimit(1).truncationMode(mode).frame(width: 160, alignment: .leading).probe("t"), scheme: scheme)
            store.put("text_fit.truncation.\(name).ink_width", nil, ink(shot, "t").width)
            store.image("text_fit_truncation_\(name)", shot, shot.frame("t"))
        }
    }

    static func alignment(_ store: Store, _ scheme: Scheme) {
        for (name, mode) in [("leading", TextAlignment.leading), ("center", .center), ("trailing", .trailing)] {
            let view = Text("Wide first line here\nShort").multilineTextAlignment(mode).frame(width: 300).probe("t")
            let shot = Harness.capture(view, scheme: scheme)
            let box = shot.frame("t")
            let lines = shot.raster.inkLines(in: box)
            store.put("text_fit.alignment.\(name).text_width", nil, box.width)
            if lines.count >= 2 {
                store.put("text_fit.alignment.\(name).first_left", nil, lines[0].minX - box.minX)
                store.put("text_fit.alignment.\(name).second_left", nil, lines[1].minX - box.minX)
                store.put("text_fit.alignment.\(name).second_right", nil, box.maxX - lines[1].maxX)
            }
            let inner = Harness.capture(Text("Wide first line here\nShort").multilineTextAlignment(mode).probe("t"), scheme: scheme)
            store.put("text_fit.alignment.\(name).unframed_width", nil, inner.frame("t").width)
            store.image("text_fit_alignment_\(name)", shot, box)
        }
    }

    static func picture() -> NSImage {
        let image = NSImage(size: NSSize(width: 40, height: 20))
        image.lockFocus()
        NSColor.systemRed.setFill()
        NSRect(x: 0, y: 0, width: 40, height: 20).fill()
        image.unlockFocus()
        return image
    }

    static func aspect(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, AnyView)] = [
            ("fit_2_in_300x100", AnyView(Color.red.aspectRatio(2, contentMode: .fit).probe("c").frame(width: 300, height: 100))),
            ("fit_2_in_100x300", AnyView(Color.red.aspectRatio(2, contentMode: .fit).probe("c").frame(width: 100, height: 300))),
            ("fill_2_in_300x100", AnyView(Color.red.aspectRatio(2, contentMode: .fill).probe("c").frame(width: 300, height: 100))),
            ("fill_2_in_100x300", AnyView(Color.red.aspectRatio(2, contentMode: .fill).probe("c").frame(width: 100, height: 300))),
            ("fit_2_width_only_300", AnyView(VStack { Color.red.aspectRatio(2, contentMode: .fit).probe("c") }.frame(width: 300))),
            ("fill_2_width_only_300", AnyView(VStack { Color.red.aspectRatio(2, contentMode: .fill).probe("c") }.frame(width: 300))),
            ("image_fit_in_200x200", AnyView(Image(nsImage: picture()).resizable().scaledToFit().probe("c").frame(width: 200, height: 200))),
            ("image_fill_in_200x200", AnyView(Image(nsImage: picture()).resizable().scaledToFill().probe("c").frame(width: 200, height: 200))),
            ("image_ratio_1_fit_in_200x100", AnyView(Image(nsImage: picture()).resizable().aspectRatio(1, contentMode: .fit).probe("c").frame(width: 200, height: 100))),
            ("fixed_image_top_leading_in_100", AnyView(Image(nsImage: picture()).probe("c").frame(width: 100, height: 100, alignment: .topLeading))),
            ("fixed_image_center_in_100", AnyView(Image(nsImage: picture()).probe("c").frame(width: 100, height: 100))),
            ("fixed_image_bottom_trailing_in_100", AnyView(Image(nsImage: picture()).probe("c").frame(width: 100, height: 100, alignment: .bottomTrailing))),
            ("circle_20_top_leading_in_100", AnyView(Circle().frame(width: 20, height: 20).probe("c").frame(width: 100, height: 100, alignment: .topLeading))),
        ]
        for (name, view) in cases {
            let shot = Harness.capture(view.probe("o"), scheme: scheme)
            let outer = shot.frame("o")
            let inner = shot.frame("c")
            store.put("text_fit.aspect.\(name).x", nil, inner.minX - outer.minX)
            store.put("text_fit.aspect.\(name).y", nil, inner.minY - outer.minY)
            store.put("text_fit.aspect.\(name).width", nil, inner.width)
            store.put("text_fit.aspect.\(name).height", nil, inner.height)
            store.put("text_fit.aspect.\(name).outer_width", nil, outer.width)
            store.put("text_fit.aspect.\(name).outer_height", nil, outer.height)
        }
    }
}
