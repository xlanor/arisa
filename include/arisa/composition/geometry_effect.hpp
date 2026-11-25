#pragma once

#include <arisa/composition/animatable_data.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/view_modifier.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/graphics/projection_transform.hpp>

#include <concepts>
#include <utility>

namespace arisa::composition {

template <class T>
concept GeometryEffect =
    std::equality_comparable<T> && std::copy_constructible<T> &&
    requires(const T& effect, Size size) {
      {
        effect.effect_value(size)
      } -> std::convertible_to<graphics::ProjectionTransform>;
    };

template <GeometryEffect Effect>
class GeometryEffectModifier : public ViewModifier {
 public:
  explicit GeometryEffectModifier(Effect effect) : effect_(std::move(effect)) {}

  auto operator==(const GeometryEffectModifier&) const -> bool = default;

  auto body(Context&, Content content) const -> Element override {
    Element shown = Element{std::move(content)}.visual_effect(
        [effect = effect_](VisualEffect seen, GeometryProxy proxy) {
          return seen.projection_effect(effect.effect_value(proxy.size()));
        });
    if constexpr (requires { Effect::ignored_by_layout; }) {
      return shown;
    } else {
      return std::move(shown).geometry_projection(
          [effect = effect_](Size size) -> graphics::ProjectionTransform {
            return effect.effect_value(size);
          });
    }
  }

 private:
  Effect effect_;
};

template <GeometryEffect Effect>
  requires AnimatableView<Effect>
class GeometryEffectModifier<Effect> : public ViewModifier {
 public:
  using AnimatableData = typename Effect::AnimatableData;

  explicit GeometryEffectModifier(Effect effect) : effect_(std::move(effect)) {}

  auto operator==(const GeometryEffectModifier&) const -> bool = default;

  auto animatable_data() const -> AnimatableData {
    return effect_.animatable_data();
  }

  void set_animatable_data(AnimatableData data) {
    effect_.set_animatable_data(std::move(data));
  }

  auto body(Context&, Content content) const -> Element override {
    Element shown = Element{std::move(content)}.visual_effect(
        [effect = effect_](VisualEffect seen, GeometryProxy proxy) {
          return seen.projection_effect(effect.effect_value(proxy.size()));
        });
    if constexpr (requires { Effect::ignored_by_layout; }) {
      return shown;
    } else {
      return std::move(shown).geometry_projection(
          [effect = effect_](Size size) -> graphics::ProjectionTransform {
            return effect.effect_value(size);
          });
    }
  }

 private:
  Effect effect_;
};

template <GeometryEffect Effect>
class IgnoredByLayoutEffect {
 public:
  static constexpr bool ignored_by_layout = true;

  explicit IgnoredByLayoutEffect(Effect effect) : effect_(std::move(effect)) {}

  auto effect_value(Size size) const -> graphics::ProjectionTransform {
    return effect_.effect_value(size);
  }

  auto effect() const noexcept -> const Effect& { return effect_; }

  auto operator==(const IgnoredByLayoutEffect&) const -> bool = default;

 private:
  Effect effect_;
};

template <GeometryEffect Effect>
  requires AnimatableView<Effect>
class IgnoredByLayoutEffect<Effect> {
 public:
  using AnimatableData = typename Effect::AnimatableData;

  static constexpr bool ignored_by_layout = true;

  explicit IgnoredByLayoutEffect(Effect effect) : effect_(std::move(effect)) {}

  auto effect_value(Size size) const -> graphics::ProjectionTransform {
    return effect_.effect_value(size);
  }

  auto effect() const noexcept -> const Effect& { return effect_; }

  auto animatable_data() const -> AnimatableData {
    return effect_.animatable_data();
  }

  void set_animatable_data(AnimatableData data) {
    effect_.set_animatable_data(std::move(data));
  }

  auto operator==(const IgnoredByLayoutEffect&) const -> bool = default;

 private:
  Effect effect_;
};

template <class Effect>
auto Element::geometry_effect(Effect effect) && -> Element {
  static_assert(GeometryEffect<Effect>,
                "a geometry effect compares equal and returns a projection "
                "from effect_value(Size)");
  return std::move(*this).modifier(
      GeometryEffectModifier<Effect>{std::move(effect)});
}

}
