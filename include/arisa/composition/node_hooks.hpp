#pragma once

#include <arisa/composition/departure_hold.hpp>

#include <functional>

namespace arisa::composition {

struct NodeHooks {
  std::move_only_function<void()> appear;
  std::move_only_function<void()> disappear;
  std::move_only_function<void(DepartureHold)> depart;
};

}
