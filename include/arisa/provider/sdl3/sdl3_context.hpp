#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_init.h>

#include <expected>

namespace arisa::provider::sdl3 {

class Sdl3Context {
 public:
  [[nodiscard]] static auto create(SDL_InitFlags subsystems)
      -> std::expected<Sdl3Context, ProviderError>;

  auto subsystems() const noexcept -> SDL_InitFlags {
    return subsystems_.get();
  }

 private:
  struct Release {
    using handle_type = SDL_InitFlags;

    static auto empty() noexcept -> handle_type { return 0; }

    static auto is_valid(handle_type subsystems) noexcept -> bool {
      return subsystems != 0;
    }

    static void release(handle_type& subsystems) noexcept;
  };

  explicit Sdl3Context(SDL_InitFlags subsystems) noexcept;

  support::UniqueHandle<Release> subsystems_;
};

}
