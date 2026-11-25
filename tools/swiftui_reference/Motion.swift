import AppKit
import SwiftUI

final class CurveSink {
    nonisolated(unsafe) static var samples: [Double] = []
    nonisolated(unsafe) static var finished: Double = -1
}

final class BlendSink {
    nonisolated(unsafe) static var series: [String: [Double]] = [:]
}

struct BlendProbe: CustomAnimation {
    let start: Animation
    let retargets: [(String, Animation)]
    let times: [Double]
    let at: Double

    func animate<V: VectorArithmetic>(value: V, time: TimeInterval, context: inout AnimationContext<V>) -> V? {
        if BlendSink.series.isEmpty {
            let magnitude = max(value.magnitudeSquared.squareRoot(), 1e-12)
            var doubled = value
            doubled.scale(by: 2)
            for (name, retarget) in retargets {
                var copy = context
                var merged = false
                var out: [Double] = []
                for t in times {
                    if !merged, t >= at - 1e-9 {
                        merged = retarget.shouldMerge(previous: start, value: doubled, time: t, context: &copy)
                    }
                    let current = merged ? retarget : start
                    let target = merged ? doubled : value
                    if let r = current.animate(value: target, time: t, context: &copy) {
                        out.append(r.magnitudeSquared.squareRoot() / magnitude)
                    } else {
                        out.append(merged ? 2 : 1)
                    }
                }
                BlendSink.series[name] = out
            }
        }
        return time < 0.05 ? value : nil
    }

    static func == (lhs: BlendProbe, rhs: BlendProbe) -> Bool { lhs.times == rhs.times && lhs.at == rhs.at }
    func hash(into hasher: inout Hasher) { hasher.combine(times); hasher.combine(at) }
}

struct CurveProbe: CustomAnimation {
    let target: Animation
    let times: [Double]

    func animate<V: VectorArithmetic>(value: V, time: TimeInterval, context: inout AnimationContext<V>) -> V? {
        if CurveSink.samples.isEmpty {
            let magnitude = max(value.magnitudeSquared.squareRoot(), 1e-12)
            var copy = context
            var out: [Double] = []
            for t in times {
                if let r = target.animate(value: value, time: t, context: &copy) {
                    out.append(r.magnitudeSquared.squareRoot() / magnitude * (r.magnitudeSquared > 0 && dot(r, value) < 0 ? -1 : 1))
                } else {
                    if CurveSink.finished < 0 { CurveSink.finished = t }
                    out.append(1)
                }
            }
            CurveSink.samples = out
        }
        return time < 0.05 ? value : nil
    }

    func dot<V: VectorArithmetic>(_ a: V, _ b: V) -> Double {
        var sum = a
        sum += b
        return (sum.magnitudeSquared - a.magnitudeSquared - b.magnitudeSquared) / 2
    }

    static func == (lhs: CurveProbe, rhs: CurveProbe) -> Bool { lhs.times == rhs.times }
    func hash(into hasher: inout Hasher) { hasher.combine(times) }
}

final class MotionModel: ObservableObject {
    @Published var on = false
}

struct MotionView: View {
    @ObservedObject var model: MotionModel
    var body: some View {
        Color.red.frame(width: 10, height: 10).offset(x: model.on ? 100 : 0)
    }
}

enum Motion {
    static let step = 0.02
    static let horizon = 1.5
    static var times: [Double] { stride(from: 0.0, through: horizon + 1e-9, by: step).map { $0 } }

    static func sample(_ animation: Animation) -> ([Double], Double) {
        CurveSink.samples = []
        CurveSink.finished = -1
        let model = MotionModel()
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 100), styleMask: [.borderless], backing: .buffered, defer: true)
        let host = NSHostingView(rootView: MotionView(model: model))
        window.contentView = host
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        withAnimation(Animation(CurveProbe(target: animation, times: times))) { model.on = true }
        let deadline = Date().addingTimeInterval(2)
        while CurveSink.samples.isEmpty, Date() < deadline {
            RunLoop.main.run(until: Date().addingTimeInterval(0.02))
        }
        window.contentView = nil
        return (CurveSink.samples, CurveSink.finished)
    }

    static func put(_ store: Store, _ name: String, _ values: [Double]) {
        for (t, v) in zip(times, values) {
            store.put(String(format: "motion.%@.sample.%04d", name, Int((t * 1000).rounded())), nil, text: String(format: "%.7f", v), "progress")
        }
    }

    static func run(_ store: Store) {
        let springs: [(String, Spring)] = [
            ("smooth", .smooth), ("snappy", .snappy), ("bouncy", .bouncy), ("spring_init", Spring()),
            ("duration_0_3_bounce_minus_0_3", Spring(duration: 0.3, bounce: -0.3)),
            ("duration_1_bounce_0_5", Spring(duration: 1.0, bounce: 0.5)),
            ("response_0_3_damping_0_6", Spring(response: 0.3, dampingRatio: 0.6)),
            ("mass_1_stiffness_100_damping_10", Spring(mass: 1, stiffness: 100, damping: 10)),
            ("smooth_0_35", .smooth(duration: 0.35)),
            ("snappy_0_2", .snappy(duration: 0.2)),
            ("bouncy_extra_0_1", .bouncy(duration: 0.5, extraBounce: 0.1)),
        ]
        for (name, spring) in springs {
            let key = "motion.\(name)"
            store.put("\(key).response", nil, spring.response, "s")
            store.put("\(key).damping_ratio", nil, spring.dampingRatio, "")
            store.put("\(key).duration", nil, spring.duration, "s")
            store.put("\(key).bounce", nil, spring.bounce, "")
            store.put("\(key).settling_duration", nil, spring.settlingDuration, "s")
            store.put("\(key).stiffness", nil, spring.stiffness, "")
            store.put("\(key).damping", nil, spring.damping, "")
            put(store, "\(name).spring_value", times.map { spring.value(target: 1.0, initialVelocity: 0.0, time: $0) })
        }
        let animations: [(String, Animation)] = [
            ("default", .default), ("smooth", .smooth), ("snappy", .snappy), ("bouncy", .bouncy),
            ("spring", .spring()), ("interactive_spring", .interactiveSpring()),
            ("ease_in_out", .easeInOut), ("ease_in", .easeIn), ("ease_out", .easeOut), ("linear", .linear),
        ]
        for (name, animation) in animations {
            let (values, finished) = sample(animation)
            if values.isEmpty { continue }
            put(store, "\(name).animation", values)
            store.put("motion.\(name).animation_finished", nil, finished, "s")
            store.put("motion.\(name).description", nil, text: String(describing: animation).replacingOccurrences(of: "\t", with: " "), "")
        }
        fitDefault(store)
    }

    static func blend(_ store: Store) {
        let start = Animation.spring(response: 0.6, dampingFraction: 1.0)
        let retargets: [(String, Animation)] = [
            ("same_spring", start),
            ("response_0_2_blend_0", .spring(response: 0.2, dampingFraction: 1.0, blendDuration: 0)),
            ("response_0_2_blend_0_25", .spring(response: 0.2, dampingFraction: 1.0, blendDuration: 0.25)),
            ("response_0_2_blend_0_5", .spring(response: 0.2, dampingFraction: 1.0, blendDuration: 0.5)),
            ("response_0_2_blend_1", .spring(response: 0.2, dampingFraction: 1.0, blendDuration: 1.0)),
            ("damping_0_5_blend_0_5", .spring(response: 0.6, dampingFraction: 0.5, blendDuration: 0.5)),
        ]
        BlendSink.series = [:]
        let model = MotionModel()
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 100), styleMask: [.borderless], backing: .buffered, defer: true)
        let host = NSHostingView(rootView: MotionView(model: model))
        window.contentView = host
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        let at = 0.2
        withAnimation(Animation(BlendProbe(start: start, retargets: retargets, times: times, at: at))) { model.on = true }
        let deadline = Date().addingTimeInterval(2)
        while BlendSink.series.isEmpty, Date() < deadline {
            RunLoop.main.run(until: Date().addingTimeInterval(0.02))
        }
        window.contentView = nil
        store.put("motion.blend.start_response", nil, 0.6, "s")
        store.put("motion.blend.retarget_at", nil, at, "s")
        store.put("motion.blend.retarget_scale", nil, 2, "")
        for (name, _) in retargets {
            if let values = BlendSink.series[name] {
                put(store, "blend_\(name)", values)
            }
        }
    }

    static func fitDefault(_ store: Store) {
        let (values, _) = sample(.default)
        guard !values.isEmpty else { return }
        var best = (0.0, 0.0, Double.infinity)
        var response = 0.2
        while response <= 1.2 {
            var damping = 0.5
            while damping <= 1.3 {
                let spring = Spring(response: response, dampingRatio: damping)
                var error = 0.0
                for (t, v) in zip(times, values) { error = max(error, abs(spring.value(target: 1.0, time: t) - v)) }
                if error < best.2 { best = (response, damping, error) }
                damping += 0.005
            }
            response += 0.005
        }
        store.put("motion.default.fit_spring_response", nil, best.0, "s")
        store.put("motion.default.fit_spring_damping_ratio", nil, best.1, "")
        store.put("motion.default.fit_spring_max_error", nil, best.2, "progress")
        var bezier = (0.0, Double.infinity)
        var duration = 0.1
        while duration <= 1.5 {
            var error = 0.0
            for (t, v) in zip(times, values) {
                let u = min(1, t / duration)
                error = max(error, abs(UnitCurve.easeInOut.value(at: u) - v))
            }
            if error < bezier.1 { bezier = (duration, error) }
            duration += 0.005
        }
        store.put("motion.default.fit_ease_in_out_duration", nil, bezier.0, "s")
        store.put("motion.default.fit_ease_in_out_max_error", nil, bezier.1, "progress")
    }
}
