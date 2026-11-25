import AppKit
import SwiftUI

final class ActiveApplication: NSApplication {
    override var isActive: Bool { true }
}

final class KeyWindow: NSWindow {
    override var isKeyWindow: Bool { true }
    override var isMainWindow: Bool { true }
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
    @objc(_hasActiveAppearance) var activeAppearance: Bool { true }
    @objc(_hasActiveAppearanceIgnoringKeyFocus) var activeIgnoringKeyFocus: Bool { true }
    @objc(hasKeyAppearance) var keyAppearance: Bool { true }
    @objc(hasMainAppearance) var mainAppearance: Bool { true }
    @objc(_hasKeyAppearance) var privateKeyAppearance: Bool { true }
    @objc(_hasMainAppearance) var privateMainAppearance: Bool { true }
}

enum Scheme: String, CaseIterable {
    case light
    case dark

    var appearance: NSAppearance { NSAppearance(named: self == .light ? .aqua : .darkAqua)! }
    var colorScheme: ColorScheme { self == .light ? .light : .dark }
}

struct ProbeFrames: PreferenceKey {
    static let defaultValue: [String: CGRect] = [:]
    static func reduce(value: inout [String: CGRect], nextValue: () -> [String: CGRect]) {
        value.merge(nextValue()) { $1 }
    }
}

extension View {
    func probe(_ name: String) -> some View {
        background(
            GeometryReader { proxy in
                Color.clear.preference(key: ProbeFrames.self, value: [name: proxy.frame(in: .named("reference.root"))])
            }
        )
    }
}

final class FrameSink {
    var frames: [String: CGRect] = [:]
}

struct Shot {
    let raster: Raster
    let frames: [String: CGRect]
    let size: CGSize
    let scheme: Scheme

    func frame(_ name: String) -> CGRect { frames[name] ?? .zero }
}

final class Harness {
    static let scale: CGFloat = 2
    static let margin: CGFloat = 12

    static func capture<V: View>(_ view: V, scheme: Scheme, width: CGFloat? = nil, height: CGFloat? = nil, settle: TimeInterval = 0.12) -> Shot {
        let sink = FrameSink()
        let root = view
            .padding(margin)
            .coordinateSpace(.named("reference.root"))
            .onPreferenceChange(ProbeFrames.self) { sink.frames = $0 }
            .environment(\.colorScheme, scheme.colorScheme)
            .environment(\.controlActiveState, .key)
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 600, height: 400), styleMask: [.borderless], backing: .buffered, defer: true)
        window.isOpaque = false
        window.backgroundColor = .clear
        window.appearance = scheme.appearance
        let host = NSHostingView(rootView: root)
        host.appearance = scheme.appearance
        window.contentView = host
        var size = host.fittingSize
        if let width { size.width = width }
        if let height { size.height = height }
        size.width = ceil(size.width)
        size.height = ceil(size.height)
        window.setContentSize(size)
        host.frame = NSRect(origin: .zero, size: size)
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(settle))
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.02))
        let raster = Raster.render(host, size: size, scale: scale, scheme: scheme)
        window.contentView = nil
        return Shot(raster: raster, frames: sink.frames, size: size, scheme: scheme)
    }

    static func render(view: NSView, scheme: Scheme, size: CGSize) -> Raster {
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: size.width, height: size.height), styleMask: [.borderless], backing: .buffered, defer: true)
        window.isOpaque = false
        window.backgroundColor = .clear
        window.appearance = scheme.appearance
        let container = NSView(frame: NSRect(origin: .zero, size: size))
        container.appearance = scheme.appearance
        container.addSubview(view)
        window.contentView = container
        container.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        let raster = Raster.render(container, size: size, scale: scale, scheme: scheme)
        window.contentView = nil
        return raster
    }
}
