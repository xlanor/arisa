#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/transfer_item.hpp>

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class DragContainer {
 public:
  using Payload =
      std::function<std::vector<TransferItem>(std::span<const ElementKey>)>;

  DragContainer(std::optional<Namespace::ID> space, Payload payload) noexcept
      : space_(space), payload_(std::move(payload)) {}

  auto space() const noexcept -> const std::optional<Namespace::ID>& {
    return space_;
  }

  auto serves(const std::optional<Namespace::ID>& wanted) const noexcept
      -> bool {
    return !wanted.has_value() || wanted == space_;
  }

  auto payload(std::span<const ElementKey> ids) const
      -> std::vector<TransferItem> {
    return payload_ ? payload_(ids) : std::vector<TransferItem>{};
  }

 private:
  std::optional<Namespace::ID> space_;
  Payload payload_;
};

}
