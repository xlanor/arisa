#pragma once

#include <arisa/support/scoped_callback.hpp>

namespace arisa::provider {

class DimmingInhibit {
 public:
  explicit DimmingInhibit(support::ScopedCallback release) noexcept;

  auto active() const noexcept -> bool;

 private:
  support::ScopedCallback release_;
};

}
