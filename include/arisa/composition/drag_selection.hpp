#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class DragSelection {
 public:
  using Selected = std::function<std::vector<ElementKey>()>;

  DragSelection(std::optional<Namespace::ID> space, Selected selected) noexcept
      : space_(space), selected_(std::move(selected)) {}

  auto serves(const std::optional<Namespace::ID>& wanted) const noexcept
      -> bool {
    return !wanted.has_value() || !space_.has_value() || wanted == space_;
  }

  auto selected() const -> std::vector<ElementKey> {
    return selected_ ? selected_() : std::vector<ElementKey>{};
  }

 private:
  std::optional<Namespace::ID> space_;
  Selected selected_;
};

}
