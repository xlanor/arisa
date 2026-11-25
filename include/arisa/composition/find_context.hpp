#pragma once

#include <arisa/composition/binding.hpp>

#include <optional>

namespace arisa::composition {

struct FindContext {
  std::optional<Binding<bool>> is_presented;
  bool supports_replace{false};

  friend auto operator==(const FindContext& left, const FindContext& right)
      -> bool {
    return left.supports_replace == right.supports_replace &&
           left.is_presented.has_value() == right.is_presented.has_value() &&
           (!left.is_presented.has_value() ||
            left.is_presented->identity() == right.is_presented->identity());
  }
};

}
