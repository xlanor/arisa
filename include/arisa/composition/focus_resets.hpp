#pragma once

#include <arisa/composition/namespace.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace arisa::composition {

class FocusResets {
 public:
  void push(Namespace::ID scope) { pending_.push_back(scope); }

  auto take() -> std::vector<Namespace::ID> {
    return std::exchange(pending_, {});
  }

  auto empty() const noexcept -> bool { return pending_.empty(); }

 private:
  std::vector<Namespace::ID> pending_;
};

}
