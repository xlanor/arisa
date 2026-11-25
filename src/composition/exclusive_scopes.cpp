#include <arisa/composition/exclusive_scopes.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace arisa::composition {

auto ExclusiveScopes::acquire(ExclusiveFrame::Options options,
                              std::optional<NodeId> owner,
                              std::optional<NodeId> focused) -> std::uint64_t {
  const std::uint64_t id = next_++;
  scopes_.push_back(Scope{.id = id,
                          .options = std::move(options),
                          .owner = owner,
                          .saved = focused});
  ++revision_;
  return id;
}

void ExclusiveScopes::release(std::uint64_t id) noexcept {
  const auto found = std::ranges::find(scopes_, id, &Scope::id);
  if (found == scopes_.end()) {
    return;
  }
  const auto next = std::next(found);
  if (next == scopes_.end()) {
    restore_ = found->saved;
  } else {
    next->saved = found->saved;
  }
  scopes_.erase(found);
  ++revision_;
}

auto ExclusiveScopes::keeps(OverlayLayer layer) const noexcept -> bool {
  return std::ranges::all_of(scopes_, [layer](const Scope& scope) {
    return std::ranges::contains(scope.options.keep, layer);
  });
}

auto ExclusiveScopes::leased() const noexcept -> bool {
  return std::ranges::any_of(
      scopes_, [](const Scope& scope) { return scope.options.lease; });
}

auto ExclusiveScopes::kept() const -> std::vector<OverlayLayer> {
  std::vector<OverlayLayer> layers;
  if (scopes_.empty()) {
    return layers;
  }
  for (const OverlayLayer layer : scopes_.front().options.keep) {
    if (keeps(layer) && !std::ranges::contains(layers, layer)) {
      layers.push_back(layer);
    }
  }
  return layers;
}

auto ExclusiveScopes::owner() const noexcept -> std::optional<NodeId> {
  for (const Scope& scope : std::views::reverse(scopes_)) {
    if (scope.owner.has_value()) {
      return scope.owner;
    }
  }
  return std::nullopt;
}

auto ExclusiveScopes::take_restore() noexcept -> std::optional<NodeId> {
  return std::exchange(restore_, std::nullopt);
}

}
