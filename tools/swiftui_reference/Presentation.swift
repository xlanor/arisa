import AppKit
import SwiftUI

enum Presentation {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            alert(store, scheme, name: "alert", buttons: ["OK", "Cancel"], informative: "Informative text that explains the alert.")
            alert(store, scheme, name: "alert_single", buttons: ["OK"], informative: "Informative text.")
            alert(store, scheme, name: "confirmation_dialog", buttons: ["Delete", "Archive", "Cancel"], informative: "")
        }
        let popover = NSPopover()
        store.put("popover.default_content_width", nil, popover.contentSize.width)
        store.put("popover.default_content_height", nil, popover.contentSize.height)
        store.put("scrim.sheet.opacity", nil, 0, "alpha")
        store.put("scrim.alert.opacity", nil, 0, "alpha")
        for scheme in Scheme.allCases {
            sheetContent(store, scheme)
        }
    }

    static func alert(_ store: Store, _ scheme: Scheme, name: String, buttons: [String], informative: String) {
        var result: (CGSize, [CGRect], CGRect?, Raster?) = (.zero, [], nil, nil)
        scheme.appearance.performAsCurrentDrawingAppearance {
            let alert = NSAlert()
            alert.messageText = "Title"
            alert.informativeText = informative
            for b in buttons { alert.addButton(withTitle: b) }
            alert.window.appearance = scheme.appearance
            alert.layout()
            let content = alert.window.contentView!
            content.layoutSubtreeIfNeeded()
            let frames = alert.buttons.map { $0.convert($0.bounds, to: content) }
            let icon = content.subviews.compactMap { $0 as? NSImageView }.first.map { $0.convert($0.bounds, to: content) }
            result = (alert.window.frame.size, frames.map { flip($0, height: content.bounds.height, flipped: content.isFlipped) }, icon.map { flip($0, height: content.bounds.height, flipped: content.isFlipped) }, nil)
            let size = content.bounds.size
            let rep = Raster.render(content, size: size, scale: Harness.scale, scheme: scheme)
            result.3 = rep
        }
        let (size, frames, icon, raster) = result
        let key = "presentation.\(name)"
        store.put("\(key).width", scheme, size.width)
        store.put("\(key).height", scheme, size.height)
        let sorted = frames.sorted { $0.minY == $1.minY ? $0.minX < $1.minX : $0.minY < $1.minY }
        if let first = sorted.first, let lowest = sorted.map({ $0.maxY }).max(), let leftmost = sorted.map({ $0.minX }).min() {
            store.put("\(key).button_height", scheme, first.height)
            store.put("\(key).button_width", scheme, first.width)
            store.put("\(key).button_bottom_inset", scheme, size.height - lowest)
            store.put("\(key).button_side_inset", scheme, leftmost)
        }
        if sorted.count >= 2 {
            let stacked = sorted[0].minY != sorted[1].minY
            store.put("\(key).button_gap", scheme, stacked ? sorted[1].minY - sorted[0].maxY : sorted[1].minX - sorted[0].maxX)
            store.put("\(key).buttons_stacked", scheme, stacked ? 1 : 0, "bool")
        }
        if let icon {
            store.put("\(key).icon_size", scheme, icon.width)
            store.put("\(key).icon_top", scheme, icon.minY)
        }
        if let raster {
            store.colour("\(key).background", scheme, raster.sampleFlat(CGPoint(x: 6, y: 6)))
            let shot = Shot(raster: raster, frames: [:], size: size, scheme: scheme)
            store.image(name, shot, CGRect(origin: .zero, size: size), pad: 0)
        }
    }

    static func flip(_ r: CGRect, height: CGFloat, flipped: Bool) -> CGRect {
        flipped ? r : CGRect(x: r.minX, y: height - r.maxY, width: r.width, height: r.height)
    }

    static func sheetContent(_ store: Store, _ scheme: Scheme) {
        let view = VStack(spacing: 0) {
            Text("Sheet").font(.headline).probe("title")
            Text("Sheet content").probe("body")
        }
        .padding()
        .probe("sheet")
        let shot = Harness.capture(view, scheme: scheme)
        store.put("presentation.sheet.content_padding", scheme, shot.frame("title").minY - shot.frame("sheet").minY)
    }
}
