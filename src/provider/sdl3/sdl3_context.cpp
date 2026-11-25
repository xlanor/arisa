#include <arisa/provider/sdl3/sdl3_context.hpp>

namespace arisa::provider::sdl3 {

Sdl3Context::Sdl3Context(SDL_InitFlags subsystems) noexcept
    : subsystems_(subsystems) {}

auto Sdl3Context::create(SDL_InitFlags subsystems)
    -> std::expected<Sdl3Context, ProviderError> {
  if (subsystems == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  if (!SDL_InitSubSystem(subsystems)) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return Sdl3Context{subsystems};
}

void Sdl3Context::Release::release(handle_type& subsystems) noexcept {
  SDL_QuitSubSystem(subsystems);
  if (SDL_WasInit(0) == 0) {
    SDL_Quit();
  }
}

}
