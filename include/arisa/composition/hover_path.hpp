#pragma once

#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/node_id.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class HoverPath {
 public:
  struct Group {
    std::optional<NodeId> container;
    std::optional<HoverEffectGroup> named;

    friend auto operator==(const Group&, const Group&) -> bool = default;
  };

  HoverPath(std::vector<NodeId> chain, std::vector<Group> groups)
      : chain_(std::move(chain)), groups_(std::move(groups)) {}

  auto contains(NodeId node) const noexcept -> bool {
    return std::ranges::contains(chain_, node);
  }

  auto active(NodeId container) const noexcept -> bool {
    return std::ranges::any_of(groups_, [container](const Group& group) {
      return group.container == container;
    });
  }

  auto active(const HoverEffectGroup& named) const -> bool {
    return std::ranges::any_of(groups_, [&named](const Group& group) {
      return group.named.has_value() && group.named->same_group(named);
    });
  }

  auto chain() const noexcept -> const std::vector<NodeId>& { return chain_; }

  auto groups() const noexcept -> const std::vector<Group>& { return groups_; }

  friend auto operator==(const HoverPath&, const HoverPath&) -> bool = default;

 private:
  std::vector<NodeId> chain_;
  std::vector<Group> groups_;
};

}
