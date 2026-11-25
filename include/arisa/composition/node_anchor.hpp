#pragma once

#include <arisa/composition/node_id.hpp>

#include <functional>

namespace arisa::composition {

class Tree;

class NodeAnchor {
 public:
  NodeAnchor(Tree& tree, NodeId node) noexcept : tree_(tree), node_(node) {}

  void mark_dirty() const;

  auto node() const noexcept -> NodeId { return node_; }

  void rebind(NodeId node) noexcept { node_ = node; }

 private:
  std::reference_wrapper<Tree> tree_;
  NodeId node_;
};

}
