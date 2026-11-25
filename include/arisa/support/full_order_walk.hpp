#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::support {

template <class Access>
concept TreeAccess = requires(typename Access::Node& node) {
  typename Access::Node;
  { Access::children(node) } -> std::ranges::random_access_range;
  { Access::children(node) } -> std::ranges::sized_range;
};

template <TreeAccess Access, std::movable State>
class FullOrderWalk {
 public:
  using Node = typename Access::Node;

  struct Step {
    State state;
    bool descend{true};
  };

  template <class Visitor>
    requires requires(Visitor& visitor, Node& node, const State& parent,
                      State& own, State* above) {
      { visitor.enter(node, parent) } -> std::same_as<Step>;
      requires requires { visitor.leave(node, own); } ||
                   requires { visitor.leave(node, own, above); };
    }
  void run(Node& root, const State& parent, Visitor& visitor) {
    levels_.clear();
    open(root, parent, visitor);
    while (!levels_.empty()) {
      Level& top = levels_.back();
      if (Node* child = top.descend ? following(top, visitor) : nullptr) {
        open(*child, top.state, visitor);
        continue;
      }
      close(top, visitor);
      levels_.pop_back();
    }
  }

  auto depth() const noexcept -> std::size_t { return levels_.size(); }

  auto capacity() const noexcept -> std::size_t { return levels_.capacity(); }

 private:
  struct Level {
    Node* node;
    State state;
    std::size_t next;
    bool descend;
  };

  template <class Visitor>
  void open(Node& node, const State& parent, Visitor& visitor) {
    Step step = visitor.enter(node, parent);
    levels_.emplace_back(&node, std::move(step.state), 0, step.descend);
  }

  template <class Visitor>
  void close(Level& level, Visitor& visitor) {
    if constexpr (requires(State* above) {
                    visitor.leave(*level.node, level.state, above);
                  }) {
      State* above =
          levels_.size() > 1 ? &levels_[levels_.size() - 2].state : nullptr;
      visitor.leave(*level.node, level.state, above);
    } else {
      visitor.leave(*level.node, level.state);
    }
  }

  template <class Visitor>
  static auto following(Level& level, Visitor& visitor) -> Node* {
    if constexpr (requires(Node& node, State& own) {
                    { visitor.next(node, own) } -> std::same_as<Node*>;
                  }) {
      return visitor.next(*level.node, level.state);
    } else {
      if (level.next >= std::ranges::size(Access::children(*level.node))) {
        return nullptr;
      }
      return &at(*level.node, level.next++);
    }
  }

  static auto at(Node& node, std::size_t index) -> Node& {
    auto&& children = Access::children(node);
    using Children = decltype(children);
    const auto offset =
        static_cast<std::ranges::range_difference_t<Children>>(index);
    if constexpr (std::convertible_to<std::ranges::range_reference_t<Children>,
                                      Node&>) {
      return std::ranges::begin(children)[offset];
    } else {
      return *std::ranges::begin(children)[offset];
    }
  }

  std::vector<Level> levels_;
};

}
