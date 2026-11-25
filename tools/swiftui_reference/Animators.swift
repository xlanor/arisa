import AppKit
import QuartzCore
import SwiftUI

final class LogicalSink {
    nonisolated(unsafe) static var logical: Double = -1
    nonisolated(unsafe) static var removed: Double = -1
    nonisolated(unsafe) static var done = false
}

struct LogicalProbe: CustomAnimation {
    let target: Animation

    func animate<V: VectorArithmetic>(value: V, time: TimeInterval, context: inout AnimationContext<V>) -> V? {
        if !LogicalSink.done {
            var copy = context
            var t = 0.0
            while t <= 4.0 {
                let result = target.animate(value: value, time: t, context: &copy)
                if LogicalSink.logical < 0, copy.isLogicallyComplete { LogicalSink.logical = t }
                if result == nil { LogicalSink.removed = t; break }
                t += 0.001
            }
            LogicalSink.done = true
        }
        return time < 0.05 ? value : nil
    }

    static func == (lhs: LogicalProbe, rhs: LogicalProbe) -> Bool { true }
    func hash(into hasher: inout Hasher) {}
}

final class PhaseSink {
    nonisolated(unsafe) static var changes: [(Int, Double)] = []
    nonisolated(unsafe) static var start = 0.0
}

final class PhaseModel: ObservableObject {
    @Published var trigger = 0
}

struct PhaseProbeView: View {
    @ObservedObject var model: PhaseModel
    let animation: Animation
    let triggered: Bool

    func note(_ phase: Int) -> Int {
        if PhaseSink.changes.last?.0 != phase {
            PhaseSink.changes.append((phase, CACurrentMediaTime() - PhaseSink.start))
        }
        return phase
    }

    var body: some View {
        if triggered {
            Color.red.frame(width: 10, height: 10)
                .phaseAnimator([0, 1, 2], trigger: model.trigger) { content, phase in
                    content.offset(x: CGFloat(note(phase)) * 20)
                } animation: { _ in animation }
        } else {
            Color.red.frame(width: 10, height: 10)
                .phaseAnimator([0, 1, 2]) { content, phase in
                    content.offset(x: CGFloat(note(phase)) * 20)
                } animation: { _ in animation }
        }
    }
}

struct FreezeProbe: CustomAnimation {
    let progress: Double

    func animate<V: VectorArithmetic>(value: V, time: TimeInterval, context: inout AnimationContext<V>) -> V? {
        var scaled = value
        scaled.scale(by: progress)
        return scaled
    }
}

final class ReplaceModel: ObservableObject {
    @Published var shown: Bool
    init(shown: Bool) { self.shown = shown }
}

struct ReplaceProbeView: View {
    @ObservedObject var model: ReplaceModel
    let up: Bool

    var body: some View {
        ZStack {
            Color.clear.frame(width: 200, height: 200)
            if model.shown {
                Color.red.frame(width: 100, height: 100).transition(up ? .blurReplace(.upUp) : .blurReplace(.downUp))
            }
        }
        .frame(width: 200, height: 200)
    }
}

enum Animators {
    static func run(_ store: Store) {
        logical(store)
        phases(store)
        replace(store)
    }

    static func replace(_ store: Store) {
        for up in [false, true] {
            for inserting in [true, false] {
                for progress in [0.25, 0.5, 0.75] {
                    let model = ReplaceModel(shown: !inserting)
                    let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 200), styleMask: [.borderless], backing: .buffered, defer: true)
                    window.isOpaque = false
                    window.backgroundColor = .clear
                    let host = NSHostingView(rootView: ReplaceProbeView(model: model, up: up))
                    host.frame = NSRect(x: 0, y: 0, width: 200, height: 200)
                    window.contentView = host
                    host.layoutSubtreeIfNeeded()
                    RunLoop.main.run(until: Date().addingTimeInterval(0.1))
                    withAnimation(Animation(FreezeProbe(progress: progress))) { model.shown = inserting }
                    RunLoop.main.run(until: Date().addingTimeInterval(0.2))
                    host.layoutSubtreeIfNeeded()
                    let raster = Raster.render(host, size: CGSize(width: 200, height: 200), scale: 2, scheme: .light)
                    window.contentView = nil
                    let name = "animators.blur_replace.\(up ? "up_up" : "down_up").\(inserting ? "insert" : "remove").\(Int(progress * 100))"
                    var row: [Double] = []
                    for x in 0..<200 { row.append(raster.alpha(x * 2, 200)) }
                    let peak = row.max() ?? 0
                    store.put("\(name).centre_alpha", nil, raster.alpha(200, 200), "")
                    store.put("\(name).peak_alpha", nil, peak, "")
                    if peak > 0.02 {
                        let half = row.enumerated().filter { $0.element >= peak * 0.5 }.map { $0.offset }
                        let low = row.enumerated().filter { $0.element >= peak * 0.1 }.map { $0.offset }
                        let high = row.enumerated().filter { $0.element >= peak * 0.9 }.map { $0.offset }
                        store.put("\(name).half_width", nil, Double((half.last ?? 0) - (half.first ?? 0) + 1), "pt")
                        store.put("\(name).ramp_width", nil, Double((high.first ?? 0) - (low.first ?? 0)), "pt")
                    }
                }
            }
        }
    }

    static func logical(_ store: Store) {
        let animations: [(String, Animation)] = [
            ("default", .default), ("spring", .spring()), ("smooth", .smooth), ("snappy", .snappy),
            ("bouncy", .bouncy), ("spring_duration_1_bounce_0_5", .spring(duration: 1.0, bounce: 0.5)),
            ("spring_response_0_3_damping_0_6", .spring(response: 0.3, dampingFraction: 0.6)),
            ("ease_in_out", .easeInOut), ("linear_0_5", .linear(duration: 0.5)),
            ("ease_out_delay_0_2", .easeOut(duration: 0.3).delay(0.2)),
            ("linear_0_2_speed_2", .linear(duration: 0.2).speed(2)),
            ("linear_0_2_repeat_3", .linear(duration: 0.2).repeatCount(3, autoreverses: true)),
        ]
        for (name, animation) in animations {
            LogicalSink.logical = -1
            LogicalSink.removed = -1
            LogicalSink.done = false
            let model = MotionModel()
            let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 100), styleMask: [.borderless], backing: .buffered, defer: true)
            let host = NSHostingView(rootView: MotionView(model: model))
            window.contentView = host
            host.layoutSubtreeIfNeeded()
            RunLoop.main.run(until: Date().addingTimeInterval(0.05))
            withAnimation(Animation(LogicalProbe(target: animation))) { model.on = true }
            let deadline = Date().addingTimeInterval(2)
            while !LogicalSink.done, Date() < deadline {
                RunLoop.main.run(until: Date().addingTimeInterval(0.02))
            }
            window.contentView = nil
            store.put("animators.logical.\(name)", nil, LogicalSink.logical, "s")
            store.put("animators.removed.\(name)", nil, LogicalSink.removed, "s")
        }
    }

    static func phases(_ store: Store) {
        let cases: [(String, Animation, Bool)] = [
            ("linear_0_2_repeating", .linear(duration: 0.2), false),
            ("linear_0_2_triggered", .linear(duration: 0.2), true),
            ("spring_0_3_repeating", .spring(duration: 0.3, bounce: 0), false),
        ]
        for (name, animation, triggered) in cases {
            PhaseSink.changes = []
            let model = PhaseModel()
            let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 100), styleMask: [.borderless], backing: .buffered, defer: true)
            let host = NSHostingView(rootView: PhaseProbeView(model: model, animation: animation, triggered: triggered))
            PhaseSink.start = CACurrentMediaTime()
            window.contentView = host
            host.layoutSubtreeIfNeeded()
            if triggered {
                RunLoop.main.run(until: Date().addingTimeInterval(0.3))
                PhaseSink.start = CACurrentMediaTime()
                model.trigger += 1
            }
            RunLoop.main.run(until: Date().addingTimeInterval(1.6))
            window.contentView = nil
            let changes = triggered ? PhaseSink.changes.filter { $0.1 >= 0 } : PhaseSink.changes
            for (index, change) in changes.prefix(8).enumerated() {
                store.put("animators.phases.\(name).\(index).phase", nil, Double(change.0), "")
                store.put("animators.phases.\(name).\(index).at", nil, change.1, "s")
            }
            store.put("animators.phases.\(name).count", nil, Double(changes.count), "")
        }
    }
}
