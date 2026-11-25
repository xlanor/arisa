#include <arisa/composition/paint_order.hpp>

#include <algorithm>
#include <ranges>

namespace arisa::composition {

auto PaintOrder::carries(const Node& node) -> bool {
  return node.passes_through() &&
         std::ranges::any_of(node.children(), [](const Node& child) {
           return child.modifiers().z_index != 0.0 || carries(child);
         });
}

auto PaintOrder::reorders(const Node& parent) -> bool {
  return std::ranges::any_of(parent.children(), [](const Node& child) {
    return child.modifiers().z_index != 0.0 || carries(child);
  });
}

void PaintOrder::flatten(const Node& parent, std::vector<const Node*>& order) {
  for (const Node& child : parent.children()) {
    if (carries(child)) {
      flatten(child, order);
    } else {
      order.push_back(&child);
    }
  }
}

void PaintOrder::arrange(const Node& parent, std::vector<const Node*>& order) {
  const std::size_t first = order.size();
  flatten(parent, order);
  std::ranges::stable_sort(
      std::ranges::subrange(order.begin() + static_cast<std::ptrdiff_t>(first),
                            order.end()),
      {}, [](const Node* child) { return child->modifiers().z_index; });
}

auto PaintOrder::chain(const Tree& tree, NodeId id)
    -> std::vector<const Node*> {
  std::vector<const Node*> found;
  std::optional<NodeId> current = id;
  while (current.has_value()) {
    const auto node = tree.node(*current);
    if (!node.has_value()) {
      break;
    }
    found.push_back(&node->get());
    current = node->get().parent();
  }
  std::ranges::reverse(found);
  return found;
}

auto PaintOrder::position(const Node& parent, const Node& child)
    -> std::size_t {
  std::vector<const Node*> order;
  flatten(parent, order);
  const auto found = std::ranges::find(order, &child);
  return static_cast<std::size_t>(found - order.begin());
}

auto PaintOrder::layout_chain(const Tree& tree, NodeId id)
    -> std::vector<const Node*> {
  std::vector<const Node*> found = chain(tree, id);
  if (found.size() < 2) {
    return found;
  }
  std::vector<const Node*> kept;
  kept.reserve(found.size());
  for (std::size_t index = 0; index < found.size(); ++index) {
    const bool last = index + 1 == found.size();
    if (!last && index > 0 && carries(*found[index])) {
      continue;
    }
    kept.push_back(found[index]);
  }
  return kept;
}

auto PaintOrder::above(const Tree& tree, NodeId upper, NodeId lower) -> bool {
  const std::vector<const Node*> top = layout_chain(tree, upper);
  const std::vector<const Node*> bottom = layout_chain(tree, lower);
  std::size_t shared = 0;
  while (shared < top.size() && shared < bottom.size() &&
         top[shared] == bottom[shared]) {
    ++shared;
  }
  if (shared == 0) {
    return false;
  }
  if (shared == top.size() || shared == bottom.size()) {
    return top.size() > bottom.size();
  }
  const Node& parent = *top[shared - 1];
  const Node& mine = *top[shared];
  const Node& theirs = *bottom[shared];
  if (mine.modifiers().z_index != theirs.modifiers().z_index) {
    return mine.modifiers().z_index > theirs.modifiers().z_index;
  }
  return position(parent, mine) > position(parent, theirs);
}

}
