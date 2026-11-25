#pragma once

#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/overlay_layer.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace arisa::composition {

class ExclusiveScopes {
 public:
  auto acquire(ExclusiveFrame::Options options, std::optional<NodeId> owner,
               std::optional<NodeId> focused) -> std::uint64_t;

  void release(std::uint64_t id) noexcept;

  auto active() const noexcept -> bool { return !scopes_.empty(); }

  auto depth() const noexcept -> std::size_t { return scopes_.size(); }

  auto keeps(OverlayLayer layer) const noexcept -> bool;

  auto leased() const noexcept -> bool;

  auto kept() const -> std::vector<OverlayLayer>;

  auto owner() const noexcept -> std::optional<NodeId>;

  auto revision() const noexcept -> std::uint64_t { return revision_; }

  auto take_restore() noexcept -> std::optional<NodeId>;

 private:
  struct Scope {
    std::uint64_t id{0};
    ExclusiveFrame::Options options;
    std::optional<NodeId> owner;
    std::optional<NodeId> saved;
  };

  std::vector<Scope> scopes_;
  std::uint64_t next_{1};
  std::uint64_t revision_{0};
  std::optional<NodeId> restore_;
};

}
