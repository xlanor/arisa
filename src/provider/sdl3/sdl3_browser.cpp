#include <arisa/provider/sdl3/sdl3_browser.hpp>

#include <SDL3/SDL_misc.h>

#include <string>

namespace arisa::provider::sdl3 {

auto Sdl3Browser::create() -> std::expected<Sdl3Browser, ProviderError> {
  return Sdl3Browser{};
}

auto Sdl3Browser::open(const Url& url) -> std::expected<void, ProviderError> {
  const std::string address{url.value()};
  if (!SDL_OpenURL(address.c_str())) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return {};
}

}
