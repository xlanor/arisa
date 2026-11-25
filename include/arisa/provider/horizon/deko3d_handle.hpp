#pragma once

#include <arisa/support/unique_handle.hpp>

namespace arisa::provider::horizon {

template <class Handle, void (*Destroy)(Handle)>
struct Deko3dRelease {
  using handle_type = Handle;

  static auto empty() noexcept -> handle_type { return nullptr; }

  static auto is_valid(handle_type handle) noexcept -> bool {
    return handle != nullptr;
  }

  static void release(handle_type& handle) noexcept { Destroy(handle); }
};

template <class Handle, void (*Destroy)(Handle)>
using Deko3dHandle = support::UniqueHandle<Deko3dRelease<Handle, Destroy>>;

}
