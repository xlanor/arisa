#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace arisa::provider {

class SensoryFeedback {
 public:
  enum class Kind : std::uint8_t {
    selection,
    impact,
    increase,
    decrease,
    success,
    warning,
    error,
    start,
    stop,
    alignment,
    level_change,
    path_complete,
    press,
    release,
  };

  enum class PressFeedback : std::uint8_t {
    button,
    button_icon_only,
    slider,
    toggle,
    tab,
  };

  enum class ReleaseFeedback : std::uint8_t {
    slider,
  };

  enum class SelectionFeedback : std::uint8_t {
    on,
    off,
    minimum,
    maximum,
  };

  enum class Weight : std::uint8_t {
    light,
    medium,
    heavy,
  };

  enum class Flexibility : std::uint8_t {
    rigid,
    solid,
    soft,
  };

  class Intensity {
   public:
    constexpr Intensity() noexcept : value_(1.0F) {}

    constexpr explicit Intensity(float value) noexcept
        : value_(!(value >= 0.0F) ? 0.0F : (value > 1.0F ? 1.0F : value)) {}

    constexpr auto value() const noexcept -> float { return value_; }

    friend constexpr auto operator==(Intensity, Intensity) -> bool = default;

   private:
    float value_;
  };

  static constexpr std::size_t kind_count = 14;

  static const SensoryFeedback selection;
  static const SensoryFeedback increase;
  static const SensoryFeedback decrease;
  static const SensoryFeedback success;
  static const SensoryFeedback warning;
  static const SensoryFeedback error;
  static const SensoryFeedback start;
  static const SensoryFeedback stop;
  static const SensoryFeedback alignment;
  static const SensoryFeedback level_change;
  static const SensoryFeedback path_complete;

  static constexpr auto impact(Weight weight = Weight::medium,
                               Intensity intensity = Intensity{}) noexcept
      -> SensoryFeedback {
    return SensoryFeedback{Kind::impact, weight, std::nullopt, intensity};
  }

  static constexpr auto impact(Flexibility flexibility,
                               Intensity intensity = Intensity{}) noexcept
      -> SensoryFeedback {
    return SensoryFeedback{Kind::impact, std::nullopt, flexibility, intensity};
  }

  static constexpr auto press(PressFeedback control) noexcept
      -> SensoryFeedback {
    return SensoryFeedback{Kind::press, std::to_underlying(control)};
  }

  static constexpr auto release(ReleaseFeedback control) noexcept
      -> SensoryFeedback {
    return SensoryFeedback{Kind::release, std::to_underlying(control)};
  }

  static constexpr auto selection_feedback(SelectionFeedback variant) noexcept
      -> SensoryFeedback {
    return SensoryFeedback{Kind::selection, std::to_underlying(variant) + 1U};
  }

  static auto catalogue() noexcept -> std::span<const SensoryFeedback>;

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto weight() const noexcept -> std::optional<Weight> {
    return weight_;
  }

  constexpr auto flexibility() const noexcept -> std::optional<Flexibility> {
    return flexibility_;
  }

  constexpr auto intensity() const noexcept -> Intensity { return intensity_; }

  constexpr auto press_feedback() const noexcept
      -> std::optional<PressFeedback> {
    if (kind_ != Kind::press) {
      return std::nullopt;
    }
    return static_cast<PressFeedback>(detail_);
  }

  constexpr auto release_feedback() const noexcept
      -> std::optional<ReleaseFeedback> {
    if (kind_ != Kind::release) {
      return std::nullopt;
    }
    return static_cast<ReleaseFeedback>(detail_);
  }

  constexpr auto selection_variant() const noexcept
      -> std::optional<SelectionFeedback> {
    if (kind_ != Kind::selection || detail_ == 0) {
      return std::nullopt;
    }
    return static_cast<SelectionFeedback>(detail_ - 1U);
  }

  auto name() const noexcept -> std::string_view;

  friend constexpr auto operator==(const SensoryFeedback&,
                                   const SensoryFeedback&) -> bool = default;

 private:
  constexpr SensoryFeedback(Kind kind, std::optional<Weight> weight,
                            std::optional<Flexibility> flexibility,
                            Intensity intensity) noexcept
      : kind_(kind),
        weight_(weight),
        flexibility_(flexibility),
        intensity_(intensity) {}

  constexpr explicit SensoryFeedback(Kind kind) noexcept
      : SensoryFeedback(kind, std::nullopt, std::nullopt, Intensity{}) {}

  constexpr SensoryFeedback(Kind kind, unsigned detail) noexcept
      : SensoryFeedback(kind, std::nullopt, std::nullopt, Intensity{}) {
    detail_ = static_cast<std::uint8_t>(detail);
  }

  Kind kind_;
  std::optional<Weight> weight_;
  std::optional<Flexibility> flexibility_;
  Intensity intensity_;
  std::uint8_t detail_{0};
};

inline constexpr SensoryFeedback SensoryFeedback::selection{Kind::selection};
inline constexpr SensoryFeedback SensoryFeedback::increase{Kind::increase};
inline constexpr SensoryFeedback SensoryFeedback::decrease{Kind::decrease};
inline constexpr SensoryFeedback SensoryFeedback::success{Kind::success};
inline constexpr SensoryFeedback SensoryFeedback::warning{Kind::warning};
inline constexpr SensoryFeedback SensoryFeedback::error{Kind::error};
inline constexpr SensoryFeedback SensoryFeedback::start{Kind::start};
inline constexpr SensoryFeedback SensoryFeedback::stop{Kind::stop};
inline constexpr SensoryFeedback SensoryFeedback::alignment{Kind::alignment};
inline constexpr SensoryFeedback SensoryFeedback::level_change{
    Kind::level_change};
inline constexpr SensoryFeedback SensoryFeedback::path_complete{
    Kind::path_complete};

}
