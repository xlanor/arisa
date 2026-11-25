#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace arisa::motion {

class KeyframeSequence {
 public:
  enum class Kind : std::uint8_t {
    linear,
    spring,
    cubic,
    move,
  };

  struct Keyframe {
    Kind kind{Kind::linear};
    AnimatableVector to;
    std::optional<double> duration;
    UnitCurve curve{UnitCurve::linear};
    Spring spring;
    std::optional<AnimatableVector> start_velocity;
    std::optional<AnimatableVector> end_velocity;
  };

  struct Sample {
    AnimatableVector value;
    AnimatableVector velocity;
  };

  KeyframeSequence(AnimatableVector initial,
                   std::span<const Keyframe> keyframes);

  auto sample(double time) const noexcept -> Sample;

  auto value(double time) const noexcept -> AnimatableVector {
    return sample(time).value;
  }

  auto duration() const noexcept -> double { return duration_; }

 private:
  struct Segment {
    Kind kind{Kind::linear};
    double start{0.0};
    double length{0.0};
    AnimatableVector from;
    AnimatableVector to;
    AnimatableVector entry;
    AnimatableVector exit;
    UnitCurve curve{UnitCurve::linear};
    Spring spring;
  };

  static auto length_of(const Keyframe& keyframe) noexcept -> double;

  static auto evaluate(const Segment& segment, double local) noexcept -> Sample;

  std::vector<Segment> segments_;
  Sample last_;
  double duration_{0.0};
};

}
