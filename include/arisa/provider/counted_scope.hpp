#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>

namespace arisa::provider {

class CountedScope {
 public:
  using Enter = std::move_only_function<std::expected<void, ProviderError>()>;
  using Leave = std::move_only_function<void() noexcept>;

  CountedScope(Enter enter, Leave leave);

  [[nodiscard]] auto acquire()
      -> std::expected<support::ScopedCallback, ProviderError>;

  auto holders() const -> std::size_t;

 private:
  class State;

  std::shared_ptr<State> state_;
};

}
