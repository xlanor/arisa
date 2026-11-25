#pragma once

#include <arisa/provider/counted_scope.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/system_info/provider.hpp>

#include <SDL3/SDL_power.h>

#include <expected>

namespace arisa::provider::sdl3 {

class Sdl3SystemInfo final : public SystemInfoProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<Sdl3SystemInfo, ProviderError>;

  [[nodiscard]] auto battery() const
      -> std::expected<BatteryState, ProviderError> override;

  [[nodiscard]] auto network() const
      -> std::expected<NetworkState, ProviderError> override;

  [[nodiscard]] auto preferred_languages() const
      -> std::expected<std::vector<std::string>, ProviderError> override;

  [[nodiscard]] auto inhibit_dimming()
      -> std::expected<DimmingInhibit, ProviderError> override;

  [[nodiscard]] static auto interpret(SDL_PowerState state, int percent)
      -> std::expected<BatteryState, ProviderError>;

 private:
  Sdl3SystemInfo(Sdl3Context video, bool controls_screen_saver);

  Sdl3Context video_;
  bool controls_screen_saver_;
  CountedScope screen_saver_;
};

}
