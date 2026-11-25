#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>

#include <array>
#include <cstddef>

namespace arisa::motion {

class Spring {
 public:
  static constexpr double minimum_response = 1.0e-4;
  static constexpr double longest_settle = 10.0;
  static constexpr double blend_step = 1.0 / 480.0;
  static constexpr double default_epsilon = 1.0e-3;
  static constexpr double settle_step = 0.1;
  static constexpr double longest_scan = 1000.0;

  struct Duration {
    double duration{0.5};
    double bounce{0.0};
  };

  struct Response {
    double response{0.5};
    double damping_ratio{1.0};
  };

  struct Physical {
    double mass{1.0};
    double stiffness{100.0};
    double damping{10.0};
    bool allow_over_damping{false};
  };

  struct Settling {
    double duration{1.0};
    double damping_ratio{1.0};
    double epsilon{default_epsilon};
  };

  struct State {
    double displacement{0.0};
    double velocity{0.0};
  };

  template <Animatable V>
  struct Motion {
    V value;
    V velocity;
  };

  Spring() noexcept;

  explicit Spring(Duration parameters) noexcept;

  explicit Spring(Response parameters) noexcept;

  explicit Spring(Physical parameters) noexcept;

  explicit Spring(Settling parameters) noexcept;

  static auto smooth(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Spring;

  static auto snappy(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Spring;

  static auto bouncy(double duration = 0.5, double extra_bounce = 0.0) noexcept
      -> Spring;

  auto response() const noexcept -> double { return response_; }

  auto duration() const noexcept -> double { return response_; }

  auto damping_ratio() const noexcept -> double { return damping_ratio_; }

  auto mass() const noexcept -> double { return mass_; }

  auto stiffness() const noexcept -> double;

  auto damping() const noexcept -> double;

  auto bounce() const noexcept -> double;

  auto angular_frequency() const noexcept -> double;

  auto state(double displacement, double velocity,
             double elapsed) const noexcept -> State;

  auto blended_state(const Spring& from, double blend, double displacement,
                     double velocity, double elapsed) const noexcept -> State;

  auto blended_toward(const Spring& from, double blend,
                      double elapsed) const noexcept -> Spring;

  auto settled(const State& state, double precision) const noexcept -> bool;

  auto settling_duration(double precision = default_epsilon) const noexcept
      -> double;

  template <Animatable V>
  auto settling_duration(const V& target, const V& initial_velocity,
                         double epsilon) const noexcept -> double {
    const AnimatableVector goal = AnimatableTraits<V>::vector(target);
    const AnimatableVector speed =
        AnimatableTraits<V>::vector(initial_velocity);
    double longest = 0.0;
    for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
      if (goal[index] != 0.0 || speed[index] != 0.0) {
        const double settling =
            settle_time(-goal[index], speed[index], epsilon);
        longest = settling > longest ? settling : longest;
      }
    }
    return longest;
  }

  template <Animatable V>
  auto settling_duration(const V& from, const V& to, const V& initial_velocity,
                         double epsilon) const noexcept -> double {
    return settling_duration(difference(to, from), initial_velocity, epsilon);
  }

  template <Animatable V>
  auto value(const V& target, const V& initial_velocity,
             double time) const noexcept -> V {
    return moved(target, initial_velocity, time).value;
  }

  template <Animatable V>
  auto velocity(const V& target, const V& initial_velocity,
                double time) const noexcept -> V {
    return moved(target, initial_velocity, time).velocity;
  }

  template <Animatable V>
  auto value(const V& from, const V& to, const V& initial_velocity,
             double time) const noexcept -> V {
    return sum(from, value(difference(to, from), initial_velocity, time));
  }

  template <Animatable V>
  auto velocity(const V& from, const V& to, const V& initial_velocity,
                double time) const noexcept -> V {
    return velocity(difference(to, from), initial_velocity, time);
  }

  template <Animatable V>
  auto update(const V& value, const V& velocity, const V& target,
              double delta_time) const noexcept -> Motion<V> {
    const AnimatableVector at = AnimatableTraits<V>::vector(value);
    const AnimatableVector goal = AnimatableTraits<V>::vector(target);
    const AnimatableVector speed = AnimatableTraits<V>::vector(velocity);
    std::array<double, AnimatableVector::capacity> values{};
    std::array<double, AnimatableVector::capacity> speeds{};
    for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
      const State next =
          state(at[index] - goal[index], speed[index], delta_time);
      values[index] = goal[index] + next.displacement;
      speeds[index] = next.velocity;
    }
    return Motion<V>{.value = AnimatableTraits<V>::value(AnimatableVector{
                         values[0], values[1], values[2], values[3]}),
                     .velocity = AnimatableTraits<V>::value(AnimatableVector{
                         speeds[0], speeds[1], speeds[2], speeds[3]})};
  }

  template <Animatable V>
  auto force(const V& target, const V& position,
             const V& velocity) const noexcept -> V {
    const AnimatableVector stretch = AnimatableTraits<V>::vector(position) -
                                     AnimatableTraits<V>::vector(target);
    return AnimatableTraits<V>::value(
        (stretch * -stiffness()) -
        (AnimatableTraits<V>::vector(velocity) * damping()));
  }

  template <Animatable V>
  auto force(const V& from, const V& to, const V& position,
             const V& velocity) const noexcept -> V {
    return force(difference(to, from), position, velocity);
  }

  friend auto operator==(const Spring&, const Spring&) -> bool = default;

 private:
  template <Animatable V>
  static auto difference(const V& left, const V& right) noexcept -> V {
    return AnimatableTraits<V>::value(AnimatableTraits<V>::vector(left) -
                                      AnimatableTraits<V>::vector(right));
  }

  template <Animatable V>
  static auto sum(const V& left, const V& right) noexcept -> V {
    return AnimatableTraits<V>::value(AnimatableTraits<V>::vector(left) +
                                      AnimatableTraits<V>::vector(right));
  }

  template <Animatable V>
  auto moved(const V& target, const V& initial_velocity,
             double time) const noexcept -> Motion<V> {
    const AnimatableVector goal = AnimatableTraits<V>::vector(target);
    const AnimatableVector speed =
        AnimatableTraits<V>::vector(initial_velocity);
    std::array<double, AnimatableVector::capacity> values{};
    std::array<double, AnimatableVector::capacity> speeds{};
    for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
      const State at = state(-goal[index], speed[index], time);
      values[index] = goal[index] + at.displacement;
      speeds[index] = at.velocity;
    }
    return Motion<V>{.value = AnimatableTraits<V>::value(AnimatableVector{
                         values[0], values[1], values[2], values[3]}),
                     .velocity = AnimatableTraits<V>::value(AnimatableVector{
                         speeds[0], speeds[1], speeds[2], speeds[3]})};
  }

  auto settle_time(double displacement, double velocity,
                   double epsilon) const noexcept -> double;

  static auto settled_frequency(const Settling& parameters) noexcept -> double;

  double response_;
  double damping_ratio_;
  double mass_{1.0};
};

}
