#pragma once

#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class ScrollTransitionConfiguration {
 public:
  enum class Mode : std::uint8_t {
    interactive,
    animated,
    identity,
  };

  class Threshold {
   public:
    static constexpr auto visible(double amount = 1.0) noexcept -> Threshold {
      return Threshold{1.0, amount, 0.0};
    }

    static constexpr auto hidden() noexcept -> Threshold {
      return visible(0.0);
    }

    static constexpr auto centered() noexcept -> Threshold {
      return Threshold{0.0, 0.5, 0.0};
    }

    constexpr auto interpolated(Threshold towards, double amount) const noexcept
        -> Threshold {
      return Threshold{edge_ + ((towards.edge_ - edge_) * amount),
                       share_ + ((towards.share_ - share_) * amount),
                       inset_ + ((towards.inset_ - inset_) * amount)};
    }

    constexpr auto inset(double distance) const noexcept -> Threshold {
      return Threshold{edge_, share_, inset_ + (edge_ * distance)};
    }

    constexpr auto leading_start(double container, double length) const noexcept
        -> double {
      return inset_ + ((1.0 - edge_) * container * 0.5) + (share_ * length);
    }

    constexpr auto trailing_start(double container,
                                  double length) const noexcept -> double {
      return container - leading_start(container, length);
    }

    friend constexpr auto operator==(Threshold, Threshold) -> bool = default;

   private:
    constexpr Threshold(double edge, double share, double inset) noexcept
        : edge_(edge), share_(share), inset_(inset) {}

    double edge_;
    double share_;
    double inset_;
  };

  static auto interactive(
      motion::UnitCurve timing_curve = motion::UnitCurve::ease_in_out) noexcept
      -> ScrollTransitionConfiguration {
    return ScrollTransitionConfiguration{Mode::interactive, std::nullopt,
                                         timing_curve};
  }

  static auto animated(
      motion::Animation animation = motion::Animation::smooth()) noexcept
      -> ScrollTransitionConfiguration {
    return ScrollTransitionConfiguration{Mode::animated, animation,
                                         motion::UnitCurve::linear};
  }

  static auto identity() noexcept -> ScrollTransitionConfiguration {
    return ScrollTransitionConfiguration{Mode::identity, std::nullopt,
                                         motion::UnitCurve::linear};
  }

  auto animation(motion::Animation value) const noexcept
      -> ScrollTransitionConfiguration {
    ScrollTransitionConfiguration changed = *this;
    changed.animation_ = value;
    return changed;
  }

  auto threshold(Threshold value) const noexcept
      -> ScrollTransitionConfiguration {
    ScrollTransitionConfiguration changed = *this;
    changed.threshold_ = value;
    return changed;
  }

  auto mode() const noexcept -> Mode { return mode_; }

  auto animation() const noexcept -> const std::optional<motion::Animation>& {
    return animation_;
  }

  auto threshold() const noexcept -> Threshold {
    if (threshold_.has_value()) {
      return *threshold_;
    }
    return mode_ == Mode::animated ? Threshold::visible(animated_share)
                                   : Threshold::visible();
  }

  auto timing_curve() const noexcept -> const motion::UnitCurve& {
    return curve_;
  }

  friend auto operator==(const ScrollTransitionConfiguration&,
                         const ScrollTransitionConfiguration&)
      -> bool = default;

 private:
  static constexpr double animated_share = 0.5;

  ScrollTransitionConfiguration(Mode mode,
                                std::optional<motion::Animation> animation,
                                motion::UnitCurve curve) noexcept
      : mode_(mode), animation_(animation), curve_(curve) {}

  Mode mode_;
  std::optional<motion::Animation> animation_;
  motion::UnitCurve curve_;
  std::optional<Threshold> threshold_;
};

}
