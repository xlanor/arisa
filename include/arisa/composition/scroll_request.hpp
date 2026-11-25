#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/unit_point.hpp>

#include <optional>

namespace arisa::composition {

struct ScrollRequest {
  ElementKey target;
  std::optional<motion::UnitPoint> anchor;
  motion::Transaction transaction;
  NodeId within{0};
};

}
