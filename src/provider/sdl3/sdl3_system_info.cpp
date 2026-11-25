#include <arisa/provider/sdl3/sdl3_system_info.hpp>

#include <arisa/support/scoped_callback.hpp>

#include <SDL3/SDL_locale.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace arisa::provider::sdl3 {

Sdl3SystemInfo::Sdl3SystemInfo(Sdl3Context video, bool controls_screen_saver)
    : video_(std::move(video)),
      controls_screen_saver_(controls_screen_saver),
      screen_saver_(
          []() -> std::expected<void, ProviderError> {
            if (!SDL_DisableScreenSaver()) {
              return std::unexpected(
                  ProviderError{ProviderErrorCode::platform_failure});
            }
            return {};
          },
          []() noexcept { static_cast<void>(SDL_EnableScreenSaver()); }) {}

auto Sdl3SystemInfo::create() -> std::expected<Sdl3SystemInfo, ProviderError> {
  auto video = Sdl3Context::create(SDL_INIT_VIDEO);
  if (!video.has_value()) {
    return std::unexpected(video.error());
  }
  static_cast<void>(SDL_EnableScreenSaver());
  const bool controls_screen_saver = SDL_DisableScreenSaver();
  static_cast<void>(SDL_EnableScreenSaver());
  return Sdl3SystemInfo{std::move(*video), controls_screen_saver};
}

auto Sdl3SystemInfo::preferred_languages() const
    -> std::expected<std::vector<std::string>, ProviderError> {
  int count = 0;
  SDL_Locale** locales = SDL_GetPreferredLocales(&count);
  if (locales == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  const support::ScopedCallback release{
      [locales]() noexcept { SDL_free(static_cast<void*>(locales)); }};
  std::vector<std::string> tags;
  for (int index = 0; index < count; ++index) {
    const SDL_Locale* locale = locales[index];
    if (locale == nullptr || locale->language == nullptr) {
      continue;
    }
    std::string tag{locale->language};
    if (locale->country != nullptr) {
      tag += '-';
      tag += locale->country;
    }
    tags.push_back(std::move(tag));
  }
  if (tags.empty()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return tags;
}

auto Sdl3SystemInfo::battery() const
    -> std::expected<BatteryState, ProviderError> {
  int percent = -1;
  const SDL_PowerState state = SDL_GetPowerInfo(nullptr, &percent);
  return interpret(state, percent);
}

auto Sdl3SystemInfo::network() const
    -> std::expected<NetworkState, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto Sdl3SystemInfo::inhibit_dimming()
    -> std::expected<DimmingInhibit, ProviderError> {
  if (!controls_screen_saver_) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return screen_saver_.acquire().transform([](support::ScopedCallback release) {
    return DimmingInhibit{std::move(release)};
  });
}

auto Sdl3SystemInfo::interpret(SDL_PowerState state, int percent)
    -> std::expected<BatteryState, ProviderError> {
  switch (state) {
    case SDL_POWERSTATE_ON_BATTERY:
    case SDL_POWERSTATE_CHARGING:
    case SDL_POWERSTATE_CHARGED:
      break;
    case SDL_POWERSTATE_NO_BATTERY:
    case SDL_POWERSTATE_UNKNOWN:
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    case SDL_POWERSTATE_ERROR:
    default:
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
  }
  if (percent < 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  const auto clamped = BatteryPercent::create(static_cast<std::uint8_t>(
      std::min<int>(percent, BatteryPercent::maximum())));
  return BatteryState{.percent = clamped.value_or(BatteryPercent::of<0>()),
                      .charging = state != SDL_POWERSTATE_ON_BATTERY};
}

}
