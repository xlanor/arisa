#include <arisa/motion/spring.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>

namespace arisa::motion {

Spring::Spring() noexcept : Spring(Duration{}) {}

Spring::Spring(Duration parameters) noexcept
    : response_(std::max(parameters.duration, minimum_response)),
      damping_ratio_(parameters.bounce >= 0.0
                         ? 1.0 - std::min(parameters.bounce, 1.0)
                         : 1.0 / (1.0 + std::max(parameters.bounce, -0.99))) {}

Spring::Spring(Response parameters) noexcept
    : response_(std::max(parameters.response, minimum_response)),
      damping_ratio_(std::max(parameters.damping_ratio, 0.0)) {}

Spring::Spring(Physical parameters) noexcept
    : response_(std::max(2.0 * std::numbers::pi *
                             std::sqrt(std::max(parameters.mass, 1.0e-9) /
                                       std::max(parameters.stiffness, 1.0e-9)),
                         minimum_response)),
      damping_ratio_(std::max(parameters.damping, 0.0) /
                     (2.0 * std::sqrt(std::max(parameters.stiffness, 1.0e-9) *
                                      std::max(parameters.mass, 1.0e-9)))),
      mass_(std::max(parameters.mass, 1.0e-9)) {
  if (!parameters.allow_over_damping) {
    damping_ratio_ = std::min(damping_ratio_, 1.0);
  }
}

Spring::Spring(Settling parameters) noexcept
    : response_(std::max(2.0 * std::numbers::pi / settled_frequency(parameters),
                         minimum_response)),
      damping_ratio_(std::max(parameters.damping_ratio, 0.0)) {}

auto Spring::settled_frequency(const Settling& parameters) noexcept -> double {
  const double duration = std::max(parameters.duration, 1.0e-6);
  const double epsilon = std::clamp(parameters.epsilon, 1.0e-12, 0.5);
  const double ratio = std::max(parameters.damping_ratio, 1.0e-6);
  if (ratio < 1.0) {
    const double floor = epsilon * std::sqrt(1.0 - (ratio * ratio)) / ratio;
    return -std::log(std::min(floor, 0.5)) / (ratio * duration);
  }
  double low = 1.0e-3;
  double high = 1.0e4;
  constexpr int halvings = 80;
  for (int step = 0; step < halvings; ++step) {
    const double natural = std::sqrt(low * high);
    const Spring probe{Response{.response = 2.0 * std::numbers::pi / natural,
                                .damping_ratio = ratio}};
    if (std::abs(probe.state(1.0, 0.0, duration).displacement) > epsilon) {
      low = natural;
    } else {
      high = natural;
    }
  }
  return std::sqrt(low * high);
}

auto Spring::stiffness() const noexcept -> double {
  const double natural = angular_frequency();
  const double base = mass_ * natural * natural;
  if (damping_ratio_ <= 1.0) {
    return base;
  }
  return base * ((2.0 * damping_ratio_ * damping_ratio_) - 1.0);
}

auto Spring::damping() const noexcept -> double {
  return 2.0 * damping_ratio_ * mass_ * angular_frequency();
}

auto Spring::smooth(double duration, double extra_bounce) noexcept -> Spring {
  return Spring{Duration{.duration = duration, .bounce = extra_bounce}};
}

auto Spring::snappy(double duration, double extra_bounce) noexcept -> Spring {
  constexpr double bounce = 0.15;
  return Spring{
      Duration{.duration = duration, .bounce = bounce + extra_bounce}};
}

auto Spring::bouncy(double duration, double extra_bounce) noexcept -> Spring {
  constexpr double bounce = 0.3;
  return Spring{
      Duration{.duration = duration, .bounce = bounce + extra_bounce}};
}

auto Spring::bounce() const noexcept -> double {
  if (damping_ratio_ <= 1.0) {
    return 1.0 - damping_ratio_;
  }
  return (1.0 / damping_ratio_) - 1.0;
}

auto Spring::angular_frequency() const noexcept -> double {
  return 2.0 * std::numbers::pi / response_;
}

auto Spring::state(double displacement, double velocity,
                   double elapsed) const noexcept -> State {
  constexpr double critical_band = 1.0e-9;
  const double natural = angular_frequency();
  const double zeta = damping_ratio_;
  const double time = std::max(elapsed, 0.0);
  if (zeta < 1.0 - critical_band) {
    const double damped = natural * std::sqrt(1.0 - (zeta * zeta));
    const double envelope = std::exp(-zeta * natural * time);
    const double first = displacement;
    const double second = (velocity + (zeta * natural * displacement)) / damped;
    const double cosine = std::cos(damped * time);
    const double sine = std::sin(damped * time);
    return State{
        .displacement = envelope * ((first * cosine) + (second * sine)),
        .velocity = envelope *
                    ((((second * damped) - (zeta * natural * first)) * cosine) -
                     (((first * damped) + (zeta * natural * second)) * sine))};
  }
  if (zeta <= 1.0 + critical_band) {
    const double envelope = std::exp(-natural * time);
    const double slope = velocity + (natural * displacement);
    const double position = displacement + (slope * time);
    return State{.displacement = envelope * position,
                 .velocity = envelope * (slope - (natural * position))};
  }
  const double spread = std::sqrt((zeta * zeta) - 1.0);
  const double slow = -natural * (zeta - spread);
  const double fast = -natural * (zeta + spread);
  const double fast_weight = (velocity - (slow * displacement)) / (fast - slow);
  const double slow_weight = displacement - fast_weight;
  const double slow_term = std::exp(slow * time);
  const double fast_term = std::exp(fast * time);
  return State{
      .displacement = (slow_weight * slow_term) + (fast_weight * fast_term),
      .velocity =
          (slow_weight * slow * slow_term) + (fast_weight * fast * fast_term)};
}

auto Spring::blended_toward(const Spring& from, double blend,
                            double elapsed) const noexcept -> Spring {
  if (blend <= 0.0 || elapsed >= blend) {
    return *this;
  }
  const double share = std::clamp(elapsed / blend, 0.0, 1.0);
  return Spring{Response{
      .response = from.response_ + ((response_ - from.response_) * share),
      .damping_ratio = damping_ratio_}};
}

auto Spring::blended_state(const Spring& from, double blend,
                           double displacement, double velocity,
                           double elapsed) const noexcept -> State {
  const double time = std::max(elapsed, 0.0);
  if (blend <= 0.0) {
    return state(displacement, velocity, time);
  }
  const double blended = std::min(time, blend);
  const auto acceleration = [this, &from, blend](double at, double position,
                                                 double speed) {
    const double natural = blended_toward(from, blend, at).angular_frequency();
    return (-natural * natural * position) -
           (2.0 * damping_ratio_ * natural * speed);
  };
  State current{.displacement = displacement, .velocity = velocity};
  const auto steps =
      static_cast<std::size_t>(std::ceil(blended / blend_step - 1.0e-9));
  const double step = steps == 0 ? 0.0 : blended / static_cast<double>(steps);
  double at = 0.0;
  for (std::size_t index = 0; index < steps; ++index) {
    const double half = step * 0.5;
    const double x = current.displacement;
    const double v = current.velocity;
    const double k1x = v;
    const double k1v = acceleration(at, x, v);
    const double k2x = v + (half * k1v);
    const double k2v = acceleration(at + half, x + (half * k1x), k2x);
    const double k3x = v + (half * k2v);
    const double k3v = acceleration(at + half, x + (half * k2x), k3x);
    const double k4x = v + (step * k3v);
    const double k4v = acceleration(at + step, x + (step * k3x), k4x);
    current.displacement =
        x + (step / 6.0) * (k1x + (2.0 * k2x) + (2.0 * k3x) + k4x);
    current.velocity =
        v + (step / 6.0) * (k1v + (2.0 * k2v) + (2.0 * k3v) + k4v);
    at += step;
  }
  if (time <= blend) {
    return current;
  }
  return state(current.displacement, current.velocity, time - blend);
}

auto Spring::settled(const State& state, double precision) const noexcept
    -> bool {
  return std::abs(state.displacement) < precision &&
         std::abs(state.velocity) < precision * angular_frequency();
}

auto Spring::settling_duration(double precision) const noexcept -> double {
  return settle_time(-1.0, 0.0, precision);
}

auto Spring::settle_time(double displacement, double velocity,
                         double epsilon) const noexcept -> double {
  const double natural = angular_frequency();
  const double decay = damping_ratio_ * natural;
  const double threshold = std::max(epsilon, 1.0e-12);
  if (decay <= 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  if (damping_ratio_ < 1.0) {
    const double reach =
        std::abs(displacement) + std::abs(velocity + (decay * displacement));
    return std::max(std::log(reach / threshold) / decay, 0.0);
  }
  constexpr int hold = 10;
  std::optional<int> last_unsettled;
  int quiet = 0;
  const auto steps = static_cast<int>(longest_scan / settle_step);
  for (int index = 0; index <= steps && quiet < hold; ++index) {
    const double time = static_cast<double>(index) * settle_step;
    if (std::abs(state(displacement, velocity, time).displacement) >=
        threshold) {
      last_unsettled = index;
      quiet = 0;
    } else {
      ++quiet;
    }
  }
  if (!last_unsettled.has_value()) {
    return 0.0;
  }
  return static_cast<double>(*last_unsettled + 1) * settle_step;
}

}
