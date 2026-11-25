#pragma once

#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition_phase.hpp>
#include <arisa/motion/transition_properties.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <cstdint>
#include <optional>

namespace arisa::motion {

class TransitionContent;

class Transition {
 public:
  struct Effect {
    float opacity{1.0F};
    float scale_x{1.0F};
    float scale_y{1.0F};
    UnitPoint anchor{UnitPoint::center};
    float offset_x{0.0F};
    float offset_y{0.0F};
    float relative_x{0.0F};
    float relative_y{0.0F};
    float blur{0.0F};
    float rotation{0.0F};
    float brightness{0.0F};
    float contrast{1.0F};
    float saturation{1.0F};
    float grayscale{0.0F};
    float hue_rotation{0.0F};
    theme::Colour colour_multiply{1.0F, 1.0F, 1.0F, 1.0F};
    float tilt{0.0F};
    float tilt_x{0.0F};
    float tilt_y{1.0F};
    float tilt_z{0.0F};
    float perspective{1.0F};

    auto combined(const Effect& other) const noexcept -> Effect;

    auto moves() const noexcept -> bool;

    auto recolours() const noexcept -> bool;

    auto reduced() const noexcept -> Effect;

    auto at(double progress) const noexcept -> Effect;

    friend auto operator==(const Effect&, const Effect&) -> bool = default;
  };

  enum class BlurReplace : std::uint8_t { down_up, up_up };

  static constexpr float blur_replace_radius = 10.0F;
  static constexpr float blur_replace_shrink = 0.92F;
  static constexpr float blur_replace_grow = 1.12F;
  static constexpr float symbol_effect_scale = 0.5F;
  static constexpr double symbol_effect_duration = 0.35;

  template <class T>
    requires requires(const T& custom, TransitionContent content,
                      TransitionPhase phase) {
      { custom.body(content, phase) } -> std::convertible_to<TransitionContent>;
    }
  static auto custom(const T& transition) -> Transition;

  static auto modifier(const TransitionContent& active,
                       const TransitionContent& identity) noexcept
      -> Transition;

  static auto symbol_effect() noexcept -> Transition;

  static auto identity() noexcept -> Transition;

  static auto effect(Effect symmetric) noexcept -> Transition;

  static auto blur_replace(
      BlurReplace configuration = BlurReplace::down_up) noexcept -> Transition;

  static auto opacity() noexcept -> Transition;

  static auto scale(float scale = 0.0F,
                    UnitPoint anchor = UnitPoint::center) noexcept
      -> Transition;

  static auto move(theme::Edge edge) noexcept -> Transition;

  static auto slide() noexcept -> Transition;

  static auto push(theme::Edge edge) noexcept -> Transition;

  static auto offset(float x, float y = 0.0F) noexcept -> Transition;

  static auto asymmetric(Transition insertion, Transition removal) noexcept
      -> Transition;

  auto combined(const Transition& other) const noexcept -> Transition;

  auto animation(std::optional<Animation> animation) const -> Transition;

  auto with_properties(TransitionProperties properties) const noexcept
      -> Transition;

  auto insertion() const noexcept -> const Effect& { return insertion_; }

  auto removal() const noexcept -> const Effect& { return removal_; }

  auto attached_animation() const noexcept -> const std::optional<Animation>& {
    return animation_;
  }

  auto properties() const noexcept -> TransitionProperties;

  auto edge() const noexcept -> std::optional<theme::Edge> { return edge_; }

  auto reduced() const noexcept -> Transition;

  friend auto operator==(const Transition&, const Transition&)
      -> bool = default;

 private:
  Transition(Effect insertion, Effect removal) noexcept
      : insertion_(insertion), removal_(removal) {}

  static auto displacement(theme::Edge edge) noexcept -> Effect;

  static auto effect_of(const TransitionContent& content) noexcept -> Effect;

  static auto opposite(theme::Edge edge) noexcept -> theme::Edge;

  Effect insertion_;
  Effect removal_;
  std::optional<Animation> animation_;
  std::optional<TransitionProperties> declared_;
  std::optional<theme::Edge> edge_;
};

}
