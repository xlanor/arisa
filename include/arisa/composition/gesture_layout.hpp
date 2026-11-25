#pragma once

#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/gesture_handlers.hpp>
#include <arisa/composition/gesture_protocol.hpp>
#include <arisa/composition/long_press_gesture.hpp>
#include <arisa/composition/magnify_gesture.hpp>
#include <arisa/composition/rotate_gesture.hpp>
#include <arisa/composition/spatial_tap_gesture.hpp>
#include <arisa/composition/tap_gesture.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::composition {

class GestureLayout {
 public:
  enum class Relation : std::uint8_t {
    exclusive,
    simultaneous,
    sequenced,
  };

  using Leaf = std::variant<TapGesture, SpatialTapGesture, LongPressGesture,
                            DragGesture, MagnifyGesture, RotateGesture>;

  struct Branch {
    Relation relation{Relation::exclusive};
    std::size_t begin{0};
    std::size_t middle{0};
    std::size_t end{0};

    friend constexpr auto operator==(const Branch&, const Branch&)
        -> bool = default;
  };

  template <class V>
  using Observer = typename GestureHandlers<V>::Observer;

  template <class G>
  static constexpr bool leaf_type =
      std::same_as<G, TapGesture> || std::same_as<G, SpatialTapGesture> ||
      std::same_as<G, LongPressGesture> || std::same_as<G, DragGesture> ||
      std::same_as<G, MagnifyGesture> || std::same_as<G, RotateGesture>;

  template <GestureProtocol G>
  static void lower(G gesture, GestureLayout& into,
                    Observer<GestureValue<G>> observer) {
    if constexpr (leaf_type<G>) {
      if (observer.changed || observer.ended || observer.cancelled) {
        gesture.handlers().observe(std::move(observer));
      }
      into.leaves_.emplace_back(std::in_place_type<G>, std::move(gesture));
    } else if constexpr (requires {
                           std::move(gesture).lower(into, std::move(observer));
                         }) {
      std::move(gesture).lower(into, std::move(observer));
    } else {
      lower(gesture.body(), into, std::move(observer));
    }
  }

  void branch(Relation relation, std::size_t begin, std::size_t middle) {
    branches_.push_back(Branch{.relation = relation,
                               .begin = begin,
                               .middle = middle,
                               .end = leaves_.size()});
  }

  void absorb(GestureLayout&& other) {
    const std::size_t base = leaves_.size();
    for (Leaf& leaf : other.leaves_) {
      leaves_.push_back(std::move(leaf));
    }
    for (const Branch& each : other.branches_) {
      branches_.push_back(Branch{.relation = each.relation,
                                 .begin = each.begin + base,
                                 .middle = each.middle + base,
                                 .end = each.end + base});
    }
    other.leaves_.clear();
    other.branches_.clear();
  }

  void watch(std::size_t begin, Observer<std::monostate> observer);

  auto size() const noexcept -> std::size_t { return leaves_.size(); }

  auto leaf(std::size_t index) noexcept -> Leaf& { return leaves_[index]; }

  auto leaf(std::size_t index) const noexcept -> const Leaf& {
    return leaves_[index];
  }

  auto branches() const noexcept -> const std::vector<Branch>& {
    return branches_;
  }

  auto relation(std::size_t first, std::size_t second) const
      -> std::optional<Relation> {
    const Branch* chosen = nullptr;
    for (const Branch& each : branches_) {
      const bool holds = each.begin <= first && first < each.end &&
                         each.begin <= second && second < each.end;
      if (holds && (chosen == nullptr ||
                    each.end - each.begin < chosen->end - chosen->begin)) {
        chosen = &each;
      }
    }
    if (chosen == nullptr) {
      return std::nullopt;
    }
    return chosen->relation;
  }

 private:
  std::vector<Leaf> leaves_;
  std::vector<Branch> branches_;
};

template <class V>
class GestureRelay {
 public:
  using Observer = GestureLayout::Observer<V>;

  GestureRelay(GestureHandlers<V> handlers, Observer outer) noexcept
      : handlers_(std::move(handlers)), outer_(std::move(outer)) {}

  void changed(const V& value) {
    handlers_.changed(value);
    if (outer_.changed) {
      outer_.changed(value);
    }
  }

  void ended(const V& value) {
    handlers_.ended(value);
    if (outer_.ended) {
      outer_.ended(value);
    }
  }

  void cancelled() {
    handlers_.cancelled();
    if (outer_.cancelled) {
      outer_.cancelled();
    }
  }

  static auto observer(std::shared_ptr<GestureRelay> relay) -> Observer {
    return Observer{
        .changed = [relay](const V& value) { relay->changed(value); },
        .ended = [relay](const V& value) { relay->ended(value); },
        .cancelled = [relay] { relay->cancelled(); }};
  }

 private:
  GestureHandlers<V> handlers_;
  Observer outer_;
};

inline void GestureLayout::watch(std::size_t begin,
                                 Observer<std::monostate> observer) {
  if (!observer.changed && !observer.ended && !observer.cancelled) {
    return;
  }
  auto relay = std::make_shared<GestureRelay<std::monostate>>(
      GestureHandlers<std::monostate>{}, std::move(observer));
  for (std::size_t index = begin; index < leaves_.size(); ++index) {
    std::visit(
        [&relay](auto& each) {
          using Value = typename std::remove_cvref_t<decltype(each)>::Value;
          each.handlers().observe(typename GestureHandlers<Value>::Observer{
              .changed =
                  [relay](const Value&) { relay->changed(std::monostate{}); },
              .ended =
                  [relay](const Value&) { relay->ended(std::monostate{}); },
              .cancelled = [relay] { relay->cancelled(); }});
        },
        leaves_[index]);
  }
}

}
