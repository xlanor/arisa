#pragma once

#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/tree.hpp>

#include <cstddef>
#include <vector>

namespace arisa::composition {

class PaintOrder {
 public:
  static auto reorders(const Node& parent) -> bool;

  static void arrange(const Node& parent, std::vector<const Node*>& order);

  static auto above(const Tree& tree, NodeId upper, NodeId lower) -> bool;

 private:
  static auto chain(const Tree& tree, NodeId id) -> std::vector<const Node*>;

  static auto position(const Node& parent, const Node& child) -> std::size_t;

  static auto carries(const Node& node) -> bool;

  static void flatten(const Node& parent, std::vector<const Node*>& order);

  static auto layout_chain(const Tree& tree, NodeId id)
      -> std::vector<const Node*>;
};

}
