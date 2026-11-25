#pragma once

#include <memory>
#include <utility>

namespace arisa::composition {

class DepartureHold {
 public:
  class Token {};

  explicit DepartureHold(std::shared_ptr<const Token> token) noexcept
      : token_(std::move(token)) {}

 private:
  std::shared_ptr<const Token> token_;
};

}
