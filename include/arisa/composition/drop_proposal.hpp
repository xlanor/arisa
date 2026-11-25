#pragma once

#include <arisa/composition/drop_operation.hpp>

#include <optional>

namespace arisa::composition {

class DropProposal {
 public:
  constexpr explicit DropProposal(DropOperation operation) noexcept
      : operation_(operation) {}

  constexpr DropProposal(DropOperation within_application,
                         DropOperation outside_application) noexcept
      : operation_(within_application), outside_(outside_application) {}

  constexpr auto operation() const noexcept -> DropOperation {
    return operation_;
  }

  constexpr auto operation_outside_application() const noexcept
      -> std::optional<DropOperation> {
    return outside_;
  }

  friend constexpr auto operator==(const DropProposal&, const DropProposal&)
      -> bool = default;

 private:
  DropOperation operation_;
  std::optional<DropOperation> outside_;
};

}
