#pragma once

#include <arisa/attr/focus.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/long_press_gesture.hpp>
#include <arisa/theme/shape.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::attr {

class OnTapGesture : public Deferred {
 public:
  explicit OnTapGesture(std::move_only_function<void()> perform)
      : OnTapGesture(1, std::move(perform)) {}

  OnTapGesture(int count, std::move_only_function<void()> perform)
      : Deferred([count, held = std::move(perform)](
                     composition::Element element) mutable {
          return std::move(element).on_tap_gesture(count, std::move(held));
        }) {}
};

class OnLongPressGesture : public Deferred {
 public:
  explicit OnLongPressGesture(std::move_only_function<void()> perform)
      : OnLongPressGesture(composition::LongPressGesture::default_duration,
                           composition::LongPressGesture::default_distance,
                           std::move(perform)) {}

  OnLongPressGesture(
      double minimum_duration, std::move_only_function<void()> perform,
      composition::LongPressGesture::Pressing on_pressing_changed = {})
      : OnLongPressGesture(minimum_duration,
                           composition::LongPressGesture::default_distance,
                           std::move(perform), std::move(on_pressing_changed)) {
  }

  OnLongPressGesture(
      double minimum_duration, float maximum_distance,
      std::move_only_function<void()> perform,
      composition::LongPressGesture::Pressing on_pressing_changed = {})
      : Deferred([minimum_duration, maximum_distance,
                  perform = std::move(perform),
                  pressing = std::move(on_pressing_changed)](
                     composition::Element element) mutable {
          return std::move(element).on_long_press_gesture(
              minimum_duration, maximum_distance, std::move(perform),
              std::move(pressing));
        }) {}
};

class GestureAttribute : public Deferred {
 public:
  explicit GestureAttribute(composition::Gesture gesture)
      : Deferred([held = std::make_shared<composition::Gesture>(
                      std::move(gesture))](composition::Element element) {
          return std::move(element).gesture(std::move(*held));
        }) {}
};

class SimultaneousGesture : public Deferred {
 public:
  explicit SimultaneousGesture(composition::Gesture gesture)
      : Deferred([held = std::make_shared<composition::Gesture>(
                      std::move(gesture))](composition::Element element) {
          return std::move(element).simultaneous_gesture(std::move(*held));
        }) {}
};

class HighPriorityGesture : public Deferred {
 public:
  explicit HighPriorityGesture(composition::Gesture gesture)
      : Deferred([held = std::make_shared<composition::Gesture>(
                      std::move(gesture))](composition::Element element) {
          return std::move(element).high_priority_gesture(std::move(*held));
        }) {}
};

class ContentShape {
 public:
  explicit ContentShape(theme::Shape shape) noexcept
      : kinds_(composition::ContentShapeKinds::interaction), shape_(shape) {}

  ContentShape(composition::ContentShapeKinds kinds,
               theme::Shape shape) noexcept
      : kinds_(kinds), shape_(shape) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).content_shape(kinds_, shape_);
  }

 private:
  composition::ContentShapeKinds kinds_;
  theme::Shape shape_;
};

using on_tap_gesture = OnTapGesture;
using on_long_press_gesture = OnLongPressGesture;
using gesture = GestureAttribute;
using simultaneous_gesture = SimultaneousGesture;
using high_priority_gesture = HighPriorityGesture;
using content_shape = ContentShape;

}
