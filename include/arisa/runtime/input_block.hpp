#pragma once

#include <arisa/support/scoped_callback.hpp>

#include <utility>

namespace arisa::runtime {

class InputGate;

class InputBlock {
 public:
  auto active() const noexcept -> bool { return release_.valid(); }

  void release() noexcept { release_.reset(); }

 private:
  friend class InputGate;

  explicit InputBlock(support::ScopedCallback release) noexcept
      : release_(std::move(release)) {}

  support::ScopedCallback release_;
};

}
