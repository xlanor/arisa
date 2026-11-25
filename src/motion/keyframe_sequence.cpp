#include <arisa/motion/keyframe_sequence.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace arisa::motion {

auto KeyframeSequence::length_of(const Keyframe& keyframe) noexcept -> double {
  switch (keyframe.kind) {
    case Kind::move:
      return 0.0;
    case Kind::spring:
      return keyframe.duration.has_value()
                 ? std::max(*keyframe.duration, 0.0)
                 : keyframe.spring.settling_duration();
    case Kind::linear:
    case Kind::cubic:
      return std::max(keyframe.duration.value_or(0.0), 0.0);
  }
  return 0.0;
}

KeyframeSequence::KeyframeSequence(AnimatableVector initial,
                                   std::span<const Keyframe> keyframes) {
  double time = 0.0;
  AnimatableVector from = initial;
  AnimatableVector incoming;
  for (std::size_t index = 0; index < keyframes.size(); ++index) {
    const Keyframe& keyframe = keyframes[index];
    const double length = length_of(keyframe);
    Segment segment{.kind = keyframe.kind,
                    .start = time,
                    .length = length,
                    .from = from,
                    .to = keyframe.to,
                    .entry = {},
                    .exit = {},
                    .curve = keyframe.curve,
                    .spring = keyframe.spring};
    switch (keyframe.kind) {
      case Kind::move:
        from = keyframe.to;
        incoming = AnimatableVector{};
        break;
      case Kind::linear:
        if (length > 0.0) {
          const AnimatableVector distance = keyframe.to - from;
          segment.entry = distance * (keyframe.curve.velocity(0.0) / length);
          segment.exit = distance * (keyframe.curve.velocity(1.0) / length);
        }
        from = keyframe.to;
        incoming = segment.exit;
        break;
      case Kind::cubic: {
        const bool follows_cubic = index > 0 &&
                                   keyframes[index - 1].kind == Kind::cubic &&
                                   !segments_.empty();
        if (keyframe.start_velocity.has_value()) {
          segment.entry = *keyframe.start_velocity;
        } else if (follows_cubic && segments_.back().length + length > 0.0) {
          segment.entry = (keyframe.to - segments_.back().from) *
                          (1.0 / (segments_.back().length + length));
        } else {
          segment.entry = incoming;
        }
        if (keyframe.end_velocity.has_value()) {
          segment.exit = *keyframe.end_velocity;
        } else if (index + 1 < keyframes.size() &&
                   keyframes[index + 1].kind != Kind::move) {
          const double span = length + length_of(keyframes[index + 1]);
          if (span > 0.0) {
            segment.exit = (keyframes[index + 1].to - from) * (1.0 / span);
          }
        }
        from = keyframe.to;
        incoming = segment.exit;
        break;
      }
      case Kind::spring: {
        segment.entry = keyframe.start_velocity.value_or(incoming);
        if (keyframe.duration.has_value()) {
          const Sample landed = evaluate(segment, length);
          from = landed.value;
          incoming = landed.velocity;
        } else {
          from = keyframe.to;
          incoming = AnimatableVector{};
        }
        break;
      }
    }
    if (length > 0.0) {
      segments_.push_back(segment);
    }
    time += length;
  }
  duration_ = time;
  last_ = Sample{.value = from, .velocity = {}};
}

auto KeyframeSequence::evaluate(const Segment& segment, double local) noexcept
    -> Sample {
  const double length = segment.length;
  switch (segment.kind) {
    case Kind::linear: {
      const double unit = local / length;
      const AnimatableVector distance = segment.to - segment.from;
      return Sample{
          .value = segment.from + (distance * segment.curve.value(unit)),
          .velocity = distance * (segment.curve.velocity(unit) / length)};
    }
    case Kind::cubic: {
      const double unit = local / length;
      const double squared = unit * unit;
      const double cubed = squared * unit;
      const double start = (2.0 * cubed) - (3.0 * squared) + 1.0;
      const double leaving = cubed - (2.0 * squared) + unit;
      const double end = (-2.0 * cubed) + (3.0 * squared);
      const double arriving = cubed - squared;
      const double start_rate = (6.0 * squared) - (6.0 * unit);
      const double leaving_rate = (3.0 * squared) - (4.0 * unit) + 1.0;
      const double end_rate = (-6.0 * squared) + (6.0 * unit);
      const double arriving_rate = (3.0 * squared) - (2.0 * unit);
      return Sample{.value = (segment.from * start) +
                             (segment.entry * (leaving * length)) +
                             (segment.to * end) +
                             (segment.exit * (arriving * length)),
                    .velocity = (segment.from * (start_rate / length)) +
                                (segment.entry * leaving_rate) +
                                (segment.to * (end_rate / length)) +
                                (segment.exit * arriving_rate)};
    }
    case Kind::spring: {
      std::array<double, AnimatableVector::capacity> values{};
      std::array<double, AnimatableVector::capacity> velocities{};
      for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
        const Spring::State state =
            segment.spring.state(segment.from[index] - segment.to[index],
                                 segment.entry[index], local);
        values[index] = segment.to[index] + state.displacement;
        velocities[index] = state.velocity;
      }
      return Sample{
          .value = AnimatableVector{values[0], values[1], values[2], values[3]},
          .velocity = AnimatableVector{velocities[0], velocities[1],
                                       velocities[2], velocities[3]}};
    }
    case Kind::move:
      return Sample{.value = segment.to, .velocity = {}};
  }
  return Sample{.value = segment.to, .velocity = {}};
}

auto KeyframeSequence::sample(double time) const noexcept -> Sample {
  for (const Segment& segment : segments_) {
    if (time < segment.start) {
      return Sample{.value = segment.from, .velocity = {}};
    }
    if (time < segment.start + segment.length) {
      return evaluate(segment, time - segment.start);
    }
  }
  return last_;
}

}
