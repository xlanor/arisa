#include <arisa/composition/drop_delegate.hpp>

namespace arisa::composition {

auto DropDelegate::validate_drop(const DropInfo&) -> bool { return true; }

void DropDelegate::drop_entered(const DropInfo&) {}

auto DropDelegate::drop_updated(const DropInfo&)
    -> std::optional<DropProposal> {
  return std::nullopt;
}

void DropDelegate::drop_exited(const DropInfo&) {}

}
