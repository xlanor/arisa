import AppKit
import SwiftUI

enum Presentations {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            suppression(store, scheme)
            critical(store, scheme)
            inspector(store, scheme, width: nil, key: "inspector.default")
            inspector(store, scheme, width: 300, key: "inspector.fixed_300")
        }
        let delay = UserDefaults.standard.object(forKey: "NSInitialToolTipDelay") as? Double
        store.put("help.initial_delay_default_set", nil, delay == nil ? 0 : 1, "bool")
        store.put("help.initial_delay", nil, delay ?? 0, "ms")
    }

    static func suppression(_ store: Store, _ scheme: Scheme) {
        var heights: (CGFloat, CGFloat) = (0, 0)
        var title = ""
        var toggle = CGRect.zero
        var lowest = CGRect.zero
        scheme.appearance.performAsCurrentDrawingAppearance {
            for shows in [false, true] {
                let alert = NSAlert()
                alert.messageText = "Title"
                alert.informativeText = "Informative text."
                alert.addButton(withTitle: "Delete")
                alert.addButton(withTitle: "Cancel")
                alert.showsSuppressionButton = shows
                alert.window.appearance = scheme.appearance
                alert.layout()
                let content = alert.window.contentView!
                content.layoutSubtreeIfNeeded()
                if shows {
                    heights.1 = alert.window.frame.height
                    if let button = alert.suppressionButton {
                        title = button.title
                        toggle = Presentation.flip(button.convert(button.bounds, to: content), height: content.bounds.height, flipped: content.isFlipped)
                    }
                    let frames = alert.buttons.map { Presentation.flip($0.convert($0.bounds, to: content), height: content.bounds.height, flipped: content.isFlipped) }
                    lowest = frames.max { $0.maxY < $1.maxY } ?? .zero
                } else {
                    heights.0 = alert.window.frame.height
                }
            }
        }
        store.put("dialog.suppression.title", scheme, text: title, "text")
        store.put("dialog.suppression.height_added", scheme, heights.1 - heights.0)
        store.put("dialog.suppression.toggle_height", scheme, toggle.height)
        store.put("dialog.suppression.toggle_below_buttons", scheme, toggle.minY >= lowest.maxY ? 1 : 0, "bool")
        store.put("dialog.suppression.toggle_leading", scheme, toggle.minX)
    }

    static func critical(_ store: Store, _ scheme: Scheme) {
        var icon = CGRect.zero
        var raster: Raster?
        var size = CGSize.zero
        scheme.appearance.performAsCurrentDrawingAppearance {
            let alert = NSAlert()
            alert.messageText = "Title"
            alert.informativeText = "Informative text."
            alert.alertStyle = .critical
            alert.addButton(withTitle: "OK")
            alert.window.appearance = scheme.appearance
            alert.layout()
            let content = alert.window.contentView!
            content.layoutSubtreeIfNeeded()
            if let view = content.subviews.compactMap({ $0 as? NSImageView }).first {
                icon = Presentation.flip(view.convert(view.bounds, to: content), height: content.bounds.height, flipped: content.isFlipped)
            }
            size = content.bounds.size
            raster = Raster.render(content, size: size, scale: Harness.scale, scheme: scheme)
        }
        store.put("dialog.critical.icon_size", scheme, icon.width)
        store.put("dialog.critical.icon_top", scheme, icon.minY)
        if let raster {
            let shot = Shot(raster: raster, frames: [:], size: size, scheme: scheme)
            store.image("dialog_critical", shot, CGRect(origin: .zero, size: size), pad: 0)
        }
    }

    static func inspector(_ store: Store, _ scheme: Scheme, width: CGFloat?, key: String) {
        let view = Color.clear
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .probe("content")
            .inspector(isPresented: .constant(true)) {
                Group {
                    if let width {
                        Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("inspector").inspectorColumnWidth(width)
                    } else {
                        Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("inspector")
                    }
                }
            }
        let shot = Harness.capture(view, scheme: scheme, width: 1000, height: 500, settle: 0.3)
        store.put("\(key).width", scheme, shot.frame("inspector").width)
        store.put("\(key).content_width", scheme, shot.frame("content").width)
    }
}
