import AppKit
import SwiftUI

final class MergeSink {
    nonisolated(unsafe) static var results: [String: Bool] = [:]
}

struct MergeProbe: CustomAnimation {
    let pairs: [(String, Animation, Animation)]

    func animate<V: VectorArithmetic>(value: V, time: TimeInterval, context: inout AnimationContext<V>) -> V? {
        if MergeSink.results.isEmpty {
            for (name, next, previous) in pairs {
                var copy = context
                MergeSink.results[name] = next.shouldMerge(previous: previous, value: value, time: 0.1, context: &copy)
            }
        }
        return time < 0.05 ? value : nil
    }

    static func == (lhs: MergeProbe, rhs: MergeProbe) -> Bool { true }
    func hash(into hasher: inout Hasher) {}
}

enum MotionParity {
    static func text(_ store: Store, _ key: String, _ value: Double, _ unit: String = "") {
        store.put(key, nil, text: String(format: "%.7f", value), unit)
    }

    static func series(_ store: Store, _ name: String, _ values: [Double]) {
        for (t, v) in zip(Motion.times, values) {
            store.put(String(format: "motion_parity.%@.sample.%04d", name, Int((t * 1000).rounded())), nil, text: String(format: "%.7f", v), "")
        }
    }

    static func run(_ store: Store) {
        springs(store)
        functions(store)
        animations(store)
        curves(store)
        merges(store)
        logical(store)
        vectors(store)
    }

    static func springs(_ store: Store) {
        let springs: [(String, Spring)] = [
            ("smooth", .smooth),
            ("bouncy", .bouncy),
            ("response_0_3_damping_0_6", Spring(response: 0.3, dampingRatio: 0.6)),
            ("mass_2_stiffness_100_damping_10", Spring(mass: 2, stiffness: 100, damping: 10)),
            ("mass_1_stiffness_100_damping_40", Spring(mass: 1, stiffness: 100, damping: 40)),
            ("mass_1_stiffness_100_damping_40_over", Spring(mass: 1, stiffness: 100, damping: 40, allowOverDamping: true)),
            ("settling_1_damping_0_5", Spring(settlingDuration: 1, dampingRatio: 0.5)),
            ("settling_0_5_damping_1", Spring(settlingDuration: 0.5, dampingRatio: 1)),
            ("settling_1_damping_0_8_epsilon_0_01", Spring(settlingDuration: 1, dampingRatio: 0.8, epsilon: 0.01)),
            ("settling_2_damping_0_3", Spring(settlingDuration: 2, dampingRatio: 0.3)),
        ]
        for (name, spring) in springs {
            let key = "motion_parity.spring.\(name)"
            text(store, "\(key).mass", spring.mass)
            text(store, "\(key).stiffness", spring.stiffness)
            text(store, "\(key).damping", spring.damping)
            text(store, "\(key).duration", spring.duration, "s")
            text(store, "\(key).response", spring.response, "s")
            text(store, "\(key).damping_ratio", spring.dampingRatio)
            text(store, "\(key).bounce", spring.bounce)
            text(store, "\(key).settling_duration", spring.settlingDuration, "s")
        }
    }

    static func functions(_ store: Store) {
        let springs: [(String, Spring)] = [
            ("smooth", .smooth), ("bouncy", .bouncy),
            ("response_0_3_damping_0_6", Spring(response: 0.3, dampingRatio: 0.6)),
            ("over", Spring(mass: 1, stiffness: 100, damping: 40, allowOverDamping: true)),
        ]
        for (name, spring) in springs {
            let key = "motion_parity.function.\(name)"
            series(store, "\(name).value_target_2_velocity_1", Motion.times.map { spring.value(target: 2.0, initialVelocity: 1.0, time: $0) })
            series(store, "\(name).velocity_target_2_velocity_1", Motion.times.map { spring.velocity(target: 2.0, initialVelocity: 1.0, time: $0) })
            series(store, "\(name).value_from_1_to_3_velocity_2", Motion.times.map { spring.value(fromValue: 1.0, toValue: 3.0, initialVelocity: 2.0, time: $0) })
            series(store, "\(name).velocity_from_1_to_3_velocity_2", Motion.times.map { spring.velocity(fromValue: 1.0, toValue: 3.0, initialVelocity: 2.0, time: $0) })
            text(store, "\(key).settling_target_1", spring.settlingDuration(target: 1.0, epsilon: 0.001), "s")
            text(store, "\(key).settling_target_10", spring.settlingDuration(target: 10.0, epsilon: 0.001), "s")
            text(store, "\(key).settling_target_1_velocity_5_epsilon_0_01", spring.settlingDuration(target: 1.0, initialVelocity: 5.0, epsilon: 0.01), "s")
            text(store, "\(key).settling_from_1_to_3_velocity_2_epsilon_0_001", spring.settlingDuration(fromValue: 1.0, toValue: 3.0, initialVelocity: 2.0, epsilon: 0.001), "s")
            text(store, "\(key).force_target_1_position_0_3_velocity_2", spring.force(target: 1.0, position: 0.3, velocity: 2.0))
            text(store, "\(key).force_from_1_to_3_position_2_velocity_1", spring.force(fromValue: 1.0, toValue: 3.0, position: 2.0, velocity: 1.0))
            for step in [1.0 / 60.0, 0.1, 0.5] {
                var value = 0.0
                var velocity = 0.0
                var time = 0.0
                var index = 0
                while time < 1.0 - 1e-9 {
                    spring.update(value: &value, velocity: &velocity, target: 1.0, deltaTime: step)
                    time += step
                    index += 1
                    if index <= 12 {
                        let tag = String(format: "%@.update_%04d.%02d", key, Int((step * 1000).rounded()), index)
                        text(store, "\(tag).value", value)
                        text(store, "\(tag).velocity", velocity)
                    }
                }
            }
        }
    }

    static func animations(_ store: Store) {
        let animations: [(String, Animation)] = [
            ("interpolating_default", .interpolatingSpring),
            ("interpolating_stiffness_100_damping_10", .interpolatingSpring(stiffness: 100, damping: 10)),
            ("interpolating_stiffness_170_damping_15_velocity_5", .interpolatingSpring(stiffness: 170, damping: 15, initialVelocity: 5)),
            ("interpolating_mass_2_stiffness_100_damping_20", .interpolatingSpring(mass: 2, stiffness: 100, damping: 20)),
            ("interpolating_duration_0_5_bounce_0_3_velocity_2", .interpolatingSpring(duration: 0.5, bounce: 0.3, initialVelocity: 2)),
            ("interpolating_bouncy_velocity_3", .interpolatingSpring(.bouncy, initialVelocity: 3)),
            ("interpolating_smooth_velocity_minus_2", .interpolatingSpring(.smooth, initialVelocity: -2)),
            ("interactive_duration_0_3_extra_0_1", .interactiveSpring(duration: 0.3, extraBounce: 0.1)),
            ("interactive_duration_default", .interactiveSpring(duration: 0.15)),
            ("timing_circular_ease_in_0_5", .timingCurve(.circularEaseIn, duration: 0.5)),
            ("timing_circular_ease_out_0_5", .timingCurve(.circularEaseOut, duration: 0.5)),
            ("timing_circular_ease_in_out_0_5", .timingCurve(.circularEaseInOut, duration: 0.5)),
            ("timing_ease_in_0_5", .timingCurve(.easeIn, duration: 0.5)),
            ("timing_ease_out_0_5", .timingCurve(.easeOut, duration: 0.5)),
            ("timing_ease_in_out_0_5", .timingCurve(.easeInOut, duration: 0.5)),
            ("timing_linear_1", .timingCurve(.linear, duration: 1)),
            ("timing_bezier_0_5", .timingCurve(.bezier(startControlPoint: UnitPoint(x: 0.2, y: 0.9), endControlPoint: UnitPoint(x: 0.3, y: 1.4)), duration: 0.5)),
            ("timing_inverse_ease_in_0_5", .timingCurve(UnitCurve.easeIn.inverse, duration: 0.5)),
        ]
        for (name, animation) in animations {
            let (values, finished) = Motion.sample(animation)
            if values.isEmpty { continue }
            series(store, "\(name).animation", values)
            text(store, "motion_parity.\(name).animation_finished", finished, "s")
        }
    }

    static func curves(_ store: Store) {
        let curves: [(String, UnitCurve)] = [
            ("linear", .linear), ("ease_in", .easeIn), ("ease_out", .easeOut), ("ease_in_out", .easeInOut),
            ("circular_ease_in", .circularEaseIn), ("circular_ease_out", .circularEaseOut),
            ("circular_ease_in_out", .circularEaseInOut),
            ("bezier", .bezier(startControlPoint: UnitPoint(x: 0.2, y: 0.9), endControlPoint: UnitPoint(x: 0.3, y: 1.4))),
        ]
        for (name, curve) in curves {
            for step in 0...20 {
                let progress = Double(step) / 20.0
                let tag = String(format: "motion_parity.curve.%@.%03d", name, step * 5)
                text(store, "\(tag).value", curve.value(at: progress))
                text(store, "\(tag).velocity", curve.velocity(at: progress))
                text(store, "\(tag).inverse_value", curve.inverse.value(at: progress))
                text(store, "\(tag).inverse_velocity", curve.inverse.velocity(at: progress))
                text(store, "\(tag).inverse_inverse_value", curve.inverse.inverse.value(at: progress))
            }
            store.put("motion_parity.curve.\(name).inverse_inverse_equal", nil, curve.inverse.inverse == curve ? 1 : 0, "")
        }
    }

    static func merges(_ store: Store) {
        let pairs: [(String, Animation, Animation)] = [
            ("spring_after_spring", .spring(), .spring()),
            ("spring_after_ease_in", .spring(), .easeIn),
            ("smooth_after_bouncy", .smooth, .bouncy),
            ("ease_in_after_ease_in", .easeIn, .easeIn),
            ("ease_in_after_spring", .easeIn, .spring()),
            ("linear_after_linear", .linear, .linear),
            ("interpolating_after_interpolating", .interpolatingSpring, .interpolatingSpring),
            ("interpolating_after_spring", .interpolatingSpring, .spring()),
            ("spring_after_interpolating", .spring(), .interpolatingSpring),
            ("interactive_after_interactive", .interactiveSpring(), .interactiveSpring()),
            ("spring_delay_after_spring", .spring().delay(0.1), .spring()),
            ("spring_speed_after_spring", .spring().speed(2), .spring()),
            ("spring_repeat_after_spring", .spring().repeatCount(2), .spring()),
            ("spring_logical_after_spring", .spring().logicallyComplete(after: 0.1), .spring()),
        ]
        MergeSink.results = [:]
        let model = MotionModel()
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 100), styleMask: [.borderless], backing: .buffered, defer: true)
        let host = NSHostingView(rootView: MotionView(model: model))
        window.contentView = host
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        withAnimation(Animation(MergeProbe(pairs: pairs))) { model.on = true }
        let deadline = Date().addingTimeInterval(2)
        while MergeSink.results.isEmpty, Date() < deadline {
            RunLoop.main.run(until: Date().addingTimeInterval(0.02))
        }
        window.contentView = nil
        for (name, _, _) in pairs {
            store.put("motion_parity.merge.\(name)", nil, (MergeSink.results[name] ?? false) ? 1 : 0, "")
        }
    }

    static func logical(_ store: Store) {
        let animations: [(String, Animation)] = [
            ("spring_after_0_1", .spring().logicallyComplete(after: 0.1)),
            ("linear_1_after_0_25", .linear(duration: 1).logicallyComplete(after: 0.25)),
            ("linear_0_2_after_0_5", .linear(duration: 0.2).logicallyComplete(after: 0.5)),
            ("smooth_after_0_3_delay_0_2", .smooth.logicallyComplete(after: 0.3).delay(0.2)),
            ("smooth_delay_0_2_after_0_3", .smooth.delay(0.2).logicallyComplete(after: 0.3)),
            ("linear_1_after_0_4_speed_2", .linear(duration: 1).logicallyComplete(after: 0.4).speed(2)),
            ("interpolating_default", .interpolatingSpring),
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
            text(store, "motion_parity.logical.\(name)", LogicalSink.logical, "s")
            text(store, "motion_parity.removed.\(name)", LogicalSink.removed, "s")
        }
    }

    static func vectors(_ store: Store) {
        let pair = AnimatablePair(3.0, AnimatablePair(4.0, 12.0))
        text(store, "motion_parity.vector.pair.magnitude_squared", pair.magnitudeSquared)
        var scaled = pair
        scaled.scale(by: -0.5)
        text(store, "motion_parity.vector.pair.scaled.first", scaled.first)
        text(store, "motion_parity.vector.pair.scaled.second.second", scaled.second.second)
        let values = AnimatableValues(1.0, 2.0, 2.0)
        text(store, "motion_parity.vector.values.magnitude_squared", values.magnitudeSquared)
        var towards = 1.0
        towards.interpolate(towards: 3.0, amount: 0.25)
        text(store, "motion_parity.vector.double.interpolate_1_3_0_25", towards)
        text(store, "motion_parity.vector.double.interpolated_1_3_1_5", (1.0).interpolated(towards: 3.0, amount: 1.5))
        let angle = Angle.degrees(30)
        text(store, "motion_parity.vector.angle.magnitude", angle.magnitude)
        text(store, "motion_parity.vector.angle.negated_degrees", (-angle).degrees)
    }
}
