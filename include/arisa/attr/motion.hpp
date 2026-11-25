#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/keyframe_animator.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/motion_path.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::attr {

class ScaleEffect {
 public:
  constexpr explicit ScaleEffect(
      float scale,
      motion::UnitPoint anchor = motion::UnitPoint::center) noexcept
      : x_(scale), y_(scale), anchor_(anchor) {}

  constexpr ScaleEffect(
      float x, float y,
      motion::UnitPoint anchor = motion::UnitPoint::center) noexcept
      : x_(x), y_(y), anchor_(anchor) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).scale_effect(x_, y_, anchor_);
  }

 private:
  float x_;
  float y_;
  motion::UnitPoint anchor_;
};

class Offset {
 public:
  constexpr explicit Offset(float x, float y = 0.0F) noexcept : x_(x), y_(y) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).offset(x_, y_);
  }

 private:
  float x_;
  float y_;
};

class RotationEffect {
 public:
  constexpr explicit RotationEffect(
      motion::Angle angle,
      motion::UnitPoint anchor = motion::UnitPoint::center) noexcept
      : angle_(angle), anchor_(anchor) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).rotation_effect(angle_, anchor_);
  }

 private:
  motion::Angle angle_;
  motion::UnitPoint anchor_;
};

class TransitionAttribute {
 public:
  explicit TransitionAttribute(motion::Transition transition) noexcept
      : transition_(transition) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).transition(transition_);
  }

 private:
  motion::Transition transition_;
};

class AnimationAttribute {
 public:
  template <std::equality_comparable T>
  AnimationAttribute(std::optional<motion::Animation> animation, T value)
      : write_([animation = std::move(animation),
                held = std::move(value)](composition::Element element) mutable {
          return std::move(element).animation(std::move(animation),
                                              std::move(held));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

class TransactionAttribute {
 public:
  explicit TransactionAttribute(composition::Element::TransactionEdit edit)
      : edit_(std::move(edit)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).transaction(std::move(edit_));
  }

 private:
  composition::Element::TransactionEdit edit_;
};

class OnChange {
 public:
  template <class T, class Action>
  OnChange(T value, Action perform)
      : OnChange(std::move(value), false, std::move(perform)) {}

  template <class T, class Action>
  OnChange(T value, bool initial, Action perform)
      : write_([held = std::move(value), initial,
                performed =
                    std::move(perform)](composition::Element element) mutable {
          return std::move(element).on_change(std::move(held), initial,
                                              std::move(performed));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

class MatchedGeometryEffect {
 public:
  MatchedGeometryEffect(composition::ElementKey key,
                        composition::Namespace::ID space,
                        motion::MatchedGeometryProperties properties =
                            motion::MatchedGeometryProperties::frame,
                        motion::UnitPoint anchor = motion::UnitPoint::center,
                        bool is_source = true)
      : key_(std::move(key)),
        space_(space),
        properties_(properties),
        anchor_(anchor),
        is_source_(is_source) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).matched_geometry_effect(
        std::move(key_), space_, properties_, anchor_, is_source_);
  }

 private:
  composition::ElementKey key_;
  composition::Namespace::ID space_;
  motion::MatchedGeometryProperties properties_;
  motion::UnitPoint anchor_;
  bool is_source_;
};

class ContentTransitionAttribute {
 public:
  constexpr explicit ContentTransitionAttribute(
      motion::ContentTransition chosen) noexcept
      : transition_(chosen) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).content_transition(transition_);
  }

 private:
  motion::ContentTransition transition_;
};

class HoverEffectAttribute {
 public:
  constexpr explicit HoverEffectAttribute(
      motion::HoverEffect effect = motion::HoverEffect::automatic,
      bool is_enabled = true) noexcept
      : effect_(effect), enabled_(is_enabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).hover_effect(effect_, enabled_);
  }

 private:
  motion::HoverEffect effect_;
  bool enabled_;
};

class MotionPathAttribute {
 public:
  MotionPathAttribute(motion::MotionPath path, double progress)
      : path_(std::move(path)), progress_(progress) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).motion_path(std::move(path_), progress_);
  }

 private:
  motion::MotionPath path_;
  double progress_;
};

class KeyframeAnimatorAttribute {
 public:
  template <class Value, std::equality_comparable Trigger, class Content,
            class Builder>
  KeyframeAnimatorAttribute(Value initial_value, Trigger trigger,
                            Content content, Builder keyframes)
      : write_([initial = std::move(initial_value), fired = std::move(trigger),
                shown = std::move(content), built = std::move(keyframes)](
                   composition::Element element) mutable {
          return std::move(element).keyframe_animator(
              std::move(initial), std::move(fired), std::move(shown),
              std::move(built));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

using scale_effect = ScaleEffect;
using offset = Offset;
using rotation_effect = RotationEffect;
using transition = TransitionAttribute;
using animation = AnimationAttribute;
using transaction = TransactionAttribute;
using on_change = OnChange;
using matched_geometry_effect = MatchedGeometryEffect;
using content_transition = ContentTransitionAttribute;
using hover_effect = HoverEffectAttribute;
using motion_path = MotionPathAttribute;
using keyframe_animator = KeyframeAnimatorAttribute;

}
