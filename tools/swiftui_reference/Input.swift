import AppKit
import SwiftUI

enum Input {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        inspector(store, scheme)
        split(store, scheme)
        let divider = NSSplitView()
        divider.dividerStyle = .thin
        store.put("input.split_divider_thin", nil, Double(divider.dividerThickness))
        divider.dividerStyle = .paneSplitter
        store.put("input.split_divider_pane", nil, Double(divider.dividerThickness))
        store.put("input.double_click_interval", nil, NSEvent.doubleClickInterval * 1000, "ms")
        store.put("input.key_repeat_delay", nil, NSEvent.keyRepeatDelay * 1000, "ms")
        store.put("input.key_repeat_interval", nil, NSEvent.keyRepeatInterval * 1000, "ms")
    }

    static func inspector(_ store: Store, _ scheme: Scheme) {
        let view = Color.clear
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .probe("content")
            .inspector(isPresented: .constant(true)) {
                Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("inspector")
                    .inspectorColumnWidth(min: 200, ideal: 270, max: 360)
            }
        let shot = Harness.capture(view, scheme: scheme, width: 1000, height: 500, settle: 0.3)
        store.put("input.inspector_range.width", scheme, shot.frame("inspector").width)
    }

    static func split(_ store: Store, _ scheme: Scheme) {
        let view = NavigationSplitView {
            List {
                Text("Inbox")
            }
            .probe("sidebar")
            .navigationSplitViewColumnWidth(min: 120, ideal: 160, max: 260)
        } detail: {
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("detail")
        }
        let shot = Harness.capture(view, scheme: scheme, width: 1024, height: 576, settle: 0.3)
        store.put("input.split_range.sidebar_width", scheme, shot.frame("sidebar").width)
    }
}
