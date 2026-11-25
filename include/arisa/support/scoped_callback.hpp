#pragma once

#include <arisa/support/unique_handle.hpp>

#include <functional>

namespace arisa::support {

struct CallbackRelease {
  using handle_type = std::move_only_function<void() noexcept>;

  static auto empty() noexcept -> handle_type { return {}; }

  static auto is_valid(const handle_type& handle) noexcept -> bool {
    return static_cast<bool>(handle);
  }

  static void release(handle_type& handle) noexcept { handle(); }
};

using ScopedCallback = UniqueHandle<CallbackRelease>;

}
