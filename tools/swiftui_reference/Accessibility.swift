import AppKit
import SwiftUI

enum AccessibilityParity {
    static func host<V: View>(_ view: V) -> (NSWindow, NSHostingView<V>) {
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 400, height: 300), styleMask: [.borderless], backing: .buffered, defer: true)
        let host = NSHostingView(rootView: view)
        window.contentView = host
        host.frame = NSRect(x: 0, y: 0, width: 400, height: 300)
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.15))
        return (window, host)
    }

    static func children(_ node: NSObject) -> [Any] {
        if let ordered = node.value(forKey: "accessibilityChildrenInNavigationOrder") as? [Any], !ordered.isEmpty {
            return ordered
        }
        if let view = node as? NSView { return view.accessibilityChildren() ?? [] }
        return (node.value(forKey: "accessibilityChildren") as? [Any]) ?? []
    }

    static func leaves(_ element: Any) -> [NSAccessibilityElementProtocol] {
        guard let node = element as? NSObject else { return [] }
        let children = children(node)
        if children.isEmpty {
            if let leaf = node as? NSAccessibilityElementProtocol { return [leaf] }
            return []
        }
        var found: [NSAccessibilityElementProtocol] = []
        for child in children {
            found += leaves(child)
        }
        return found
    }

    static func label(_ element: NSAccessibilityElementProtocol) -> String {
        guard let node = element as? NSObject else { return "" }
        if let label = node.value(forKey: "accessibilityLabel") as? String, !label.isEmpty {
            return label
        }
        return (node.value(forKey: "accessibilityValue") as? String) ?? ""
    }

    static func labels<V: View>(_ view: V) -> [String] {
        let (window, host) = host(view)
        let found = leaves(host).map(label).filter { !$0.isEmpty }
        window.contentView = nil
        return found
    }

    static func actions<V: View>(_ view: V) -> [String] {
        let (window, host) = host(view)
        var names: [String] = []
        for leaf in leaves(host) {
            if let node = leaf as? NSObject,
               let custom = node.value(forKey: "accessibilityCustomActions") as? [NSAccessibilityCustomAction] {
                names += custom.map(\.name)
            }
        }
        window.contentView = nil
        return names
    }

    static func run(_ store: Store) {
        _ = NSApplication.shared.perform(NSSelectorFromString("accessibilitySetValue:forAttribute:"), with: NSNumber(value: true), with: "AXManualAccessibility")
        _ = NSApplication.shared.perform(NSSelectorFromString("accessibilitySetValue:forAttribute:"), with: NSNumber(value: true), with: "AXEnhancedUserInterface")
        let order = labels(VStack {
            Text("Third")
            Text("First").accessibilitySortPriority(2)
            VStack {
                Text("Inner low")
                Text("Inner high").accessibilitySortPriority(1)
            }
            .accessibilityElement(children: .contain)
            .accessibilitySortPriority(1)
        })
        store.put("accessibility.sort_priority", nil, text: order.joined(separator: "|"), "")
        let content = labels(Text("Inbox").accessibilityLabel { label in
            HStack { label; Text("3 unread") }
        })
        store.put("accessibility.label_content", nil, text: content.joined(separator: "|"), "")
        let combined = labels(HStack {
            Image(systemName: "globe")
            Text("12")
            Text("files")
        }.accessibilityElement(children: .combine))
        store.put("accessibility.combine", nil, text: combined.joined(separator: "|"), "")
        let named = actions(Button("Mail") {}
            .accessibilityAction(.delete) {}
            .accessibilityAction(.escape) {}
            .accessibilityAction(named: "Archive") {}
            .accessibilityActions { Button("Flag") {}; Button("Move") {} })
        store.put("accessibility.actions", nil, text: named.joined(separator: "|"), "")
        let children = labels(Color.clear.frame(width: 80, height: 40)
            .accessibilityElement(children: .contain)
            .accessibilityChildren { HStack { Text("North"); Text("South") } })
        store.put("accessibility.children", nil, text: children.joined(separator: "|"), "")
        let propagated = labels(VStack {
            Text("Low")
            Text("High").accessibilitySortPriority(5)
        }
        .accessibilityElement(children: .contain)
        .accessibilitySortPriority(1))
        store.put("accessibility.sort_priority_inside", nil, text: propagated.joined(separator: "|"), "")
        let unsorted = labels(VStack {
            Text("Low")
            Text("High").accessibilitySortPriority(5)
        }
        .accessibilityElement(children: .contain))
        store.put("accessibility.sort_priority_contained", nil, text: unsorted.joined(separator: "|"), "")
        let ordered = actions(Button("Mail") {}
            .accessibilityAction(named: "Bravo") {}
            .accessibilityActions { Button("Alpha") {}; Button("Charlie") {} }
            .accessibilityAction(named: "Delta") {})
        store.put("accessibility.action_order", nil, text: ordered.joined(separator: "|"), "")
        let represented = labels(Color.clear.frame(width: 120, height: 12)
            .accessibilityRepresentation { Slider(value: .constant(0.5)).accessibilityLabel("Volume") })
        store.put("accessibility.representation", nil, text: represented.joined(separator: "|"), "")
    }
}
