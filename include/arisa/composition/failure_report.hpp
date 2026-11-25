#pragma once

#include <arisa/composition/node_id.hpp>
#include <arisa/composition/ui_error.hpp>

#include <optional>

namespace arisa::composition {

struct FailureReport {
  NodeId node{0};
  std::optional<NodeId> boundary;
  UiError error{""};

  friend auto operator==(const FailureReport&, const FailureReport&)
      -> bool = default;
};

}
