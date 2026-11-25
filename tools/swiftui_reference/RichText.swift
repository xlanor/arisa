import AppKit
import SwiftUI

enum RichText {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        widths(store, scheme)
        scales(store, scheme)
        decorations(store, scheme)
        faces(store, scheme)
        styled(store, scheme)
    }

    static func size<V: View>(_ view: V, _ scheme: Scheme) -> CGSize {
        Harness.capture(view.probe("t"), scheme: scheme).frame("t").size
    }

    static func rows(_ shot: Shot, _ other: Shot) -> [Int] {
        let box = shot.frame("t")
        var found: [Int] = []
        let r = shot.raster
        let top = r.px(box.minY) - 4, bottom = r.px(box.maxY) + 4
        let left = r.px(box.minX), right = r.px(box.maxX)
        for y in max(0, top)..<min(r.height, bottom) {
            var differing = 0
            for x in max(0, left)..<min(r.width, right) {
                if r.flat(x, y).distance(other.raster.flat(x, y)) > 0.3 { differing += 1 }
            }
            if differing * 2 > (right - left) { found.append(y) }
        }
        return found
    }

    static func widths(_ store: Store, _ scheme: Scheme) {
        let text = "Width Sample"
        let standard = size(Text(text), scheme).width
        store.put("rich_text.width.standard", nil, standard)
        store.put("rich_text.width.compressed", nil, size(Text(text).fontWidth(.compressed), scheme).width)
        store.put("rich_text.width.condensed", nil, size(Text(text).fontWidth(.condensed), scheme).width)
        store.put("rich_text.width.expanded", nil, size(Text(text).fontWidth(.expanded), scheme).width)
    }

    static func scales(_ store: Store, _ scheme: Scheme) {
        for (name, font) in [("body", Font.body), ("title", Font.title), ("caption", Font.caption)] {
            let plain = size(Text("Scale Sample").font(font), scheme)
            let secondary = size(Text("Scale Sample").font(font).textScale(.secondary), scheme)
            store.put("rich_text.scale.\(name).width", nil, plain.width)
            store.put("rich_text.scale.\(name).secondary_width", nil, secondary.width)
            store.put("rich_text.scale.\(name).height", nil, plain.height)
            store.put("rich_text.scale.\(name).secondary_height", nil, secondary.height)
        }
    }

    static func decorations(_ store: Store, _ scheme: Scheme) {
        let font = Font.system(size: 20)
        let plain = Harness.capture(Text("Hxgy").font(font).probe("t"), scheme: scheme)
        let under = Harness.capture(Text("Hxgy").font(font).underline().probe("t"), scheme: scheme)
        let struck = Harness.capture(Text("Hxgy").font(font).strikethrough().probe("t"), scheme: scheme)
        let box = plain.frame("t")
        let scale = plain.raster.scale
        let underRows = rows(under, plain)
        let struckRows = rows(struck, plain)
        store.put("rich_text.decoration.height", nil, box.height)
        if let first = underRows.first, let last = underRows.last {
            store.put("rich_text.decoration.underline_top", nil, Double(CGFloat(first) / scale - box.minY))
            store.put("rich_text.decoration.underline_thickness", nil, Double(CGFloat(last - first + 1) / scale))
        }
        if let first = struckRows.first, let last = struckRows.last {
            store.put("rich_text.decoration.strikethrough_top", nil, Double(CGFloat(first) / scale - box.minY))
            store.put("rich_text.decoration.strikethrough_thickness", nil, Double(CGFloat(last - first + 1) / scale))
        }
        let red = Harness.capture(Text("Hxgy").font(font).underline(color: .red).probe("t"), scheme: scheme)
        if let row = underRows.first {
            store.colour("rich_text.decoration.underline_colour", scheme, red.raster.flat(red.raster.px(box.midX), row))
            store.colour("rich_text.decoration.underline_default_colour", scheme, under.raster.flat(under.raster.px(box.midX), row))
        }
        store.image("rich_text_decorations", struck, box)
    }

    static func faces(_ store: Store, _ scheme: Scheme) {
        store.put("rich_text.face.plain_word", nil, size(Text("Italic words"), scheme).width)
        store.put("rich_text.face.italic_word", nil, size(Text("Italic words").italic(), scheme).width)
        store.put("rich_text.face.monospaced_iii", nil, size(Text("iii").monospaced(), scheme).width)
        store.put("rich_text.face.monospaced_WWW", nil, size(Text("WWW").monospaced(), scheme).width)
        store.put("rich_text.face.design_monospaced_iii", nil, size(Text("iii").fontDesign(.monospaced), scheme).width)
        store.put("rich_text.face.design_serif_word", nil, size(Text("Italic words").fontDesign(.serif), scheme).width)
        store.put("rich_text.face.design_rounded_word", nil, size(Text("Italic words").fontDesign(.rounded), scheme).width)
        store.put("rich_text.face.custom_missing_word", nil, size(Text("Italic words").font(.custom("NoSuchFace", size: 13)), scheme).width)
    }

    static func styled(_ store: Store, _ scheme: Scheme) {
        store.put("rich_text.markdown.bold", nil, size(Text("**Bold**"), scheme).width)
        store.put("rich_text.markdown.plain_bold", nil, size(Text("Bold").bold(), scheme).width)
        store.put("rich_text.markdown.code", nil, size(Text("`code`"), scheme).width)
        store.put("rich_text.markdown.monospaced_code", nil, size(Text("code").monospaced(), scheme).width)
        store.put("rich_text.markdown.unmatched", nil, size(Text("a *b"), scheme).width)
        store.put("rich_text.markdown.literal_unmatched", nil, size(Text(verbatim: "a *b"), scheme).width)
        let joined = size(Text("Price ") + Text("$59").bold() + Text(" $79").strikethrough(), scheme).width
        let parts = size(Text("Price "), scheme).width + size(Text("$59").bold(), scheme).width + size(Text(" $79"), scheme).width
        store.put("rich_text.concat.joined", nil, joined)
        store.put("rich_text.concat.parts", nil, parts)
        let link = Harness.capture(Text("[Link](https://example.com)").font(.system(size: 20)).probe("t"), scheme: scheme)
        let box = link.frame("t")
        var deepest = RGBA(r: 1, g: 1, b: 1, a: 1)
        for y in link.raster.px(box.minY)..<link.raster.px(box.maxY) {
            for x in link.raster.px(box.minX)..<link.raster.px(box.maxX) {
                let c = link.raster.flat(x, y)
                if c.luminance < deepest.luminance { deepest = c }
            }
        }
        store.colour("rich_text.markdown.link_colour", scheme, deepest)
        let plainLink = Harness.capture(Text("Link").font(.system(size: 20)).probe("t"), scheme: scheme)
        store.put("rich_text.markdown.link_underline_rows", nil, Double(rows(link, plainLink).count), "rows")
        store.image("rich_text_link", link, box)
    }
}
