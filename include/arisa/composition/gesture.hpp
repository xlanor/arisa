#pragma once

#include <arisa/composition/gesture_combinators.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/gesture_protocol.hpp>

#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::composition {

class Gesture : public GestureCombinators<Gesture> {
 public:
  using Value = std::monostate;

  template <GestureProtocol G>
    requires(!std::same_as<std::remove_cvref_t<G>, Gesture>)
  Gesture(G gesture) {
    GestureLayout::lower(std::move(gesture), layout_, {});
  }

  void lower(GestureLayout& into,
             GestureLayout::Observer<std::monostate> observer) && {
    const std::size_t begin = into.size();
    into.absorb(std::move(layout_));
    into.watch(begin, std::move(observer));
  }

  static constexpr std::string_view controller_label = "Select";

  auto size() const noexcept -> std::size_t { return layout_.size(); }

  void controller_tap(Size size) {
    for (std::size_t index = 0; index < layout_.size(); ++index) {
      std::visit(
          [size](auto& leaf) {
            using Leaf = std::remove_cvref_t<decltype(leaf)>;
            if (!leaf.input_kinds().contains(GestureInputKinds::controller)) {
              return;
            }
            if constexpr (std::same_as<Leaf, TapGesture>) {
              leaf.handlers().ended(std::monostate{});
            } else if constexpr (std::same_as<Leaf, SpatialTapGesture>) {
              leaf.handlers().ended(SpatialTapGesture::Value{
                  .location =
                      Point{.x = size.width * 0.5F, .y = size.height * 0.5F}});
            }
          },
          layout_.leaf(index));
    }
  }

  void controller_hold() {
    for (std::size_t index = 0; index < layout_.size(); ++index) {
      if (auto* held = std::get_if<LongPressGesture>(&layout_.leaf(index));
          held != nullptr &&
          held->input_kinds().contains(GestureInputKinds::controller)) {
        held->pressing(true);
        held->handlers().changed(true);
        held->handlers().ended(true);
        held->pressing(false);
      }
    }
  }

  auto layout() noexcept -> GestureLayout& { return layout_; }

  auto layout() const noexcept -> const GestureLayout& { return layout_; }

 private:
  GestureLayout layout_;
};

}

#include <arisa/composition/any_gesture.hpp>
#include <arisa/composition/exclusive_gesture.hpp>
#include <arisa/composition/gesture_state_gesture.hpp>
#include <arisa/composition/mapped_gesture.hpp>
#include <arisa/composition/sequence_gesture.hpp>
#include <arisa/composition/simultaneous_gesture.hpp>
