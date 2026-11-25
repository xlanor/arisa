#pragma once

#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_video.h>

#include <deque>
#include <expected>
#include <functional>
#include <optional>
#include <string_view>

namespace arisa::provider::sdl3 {

class Sdl3WindowSurface final : public SurfaceProvider {
 public:
  [[nodiscard]] static auto create(Sdl3EventPump& pump, std::string_view title,
                                   SurfaceSize size)
      -> std::expected<Sdl3WindowSurface, ProviderError>;

  auto size() const noexcept -> SurfaceSize override;

  auto scale() const noexcept -> SurfaceScale override;

  auto display_mode() const noexcept -> DisplayMode override;

  auto poll() -> std::optional<SurfaceEvent> override;

  auto window() const noexcept -> SDL_Window* { return window_.get(); }

 private:
  struct WindowRelease {
    using handle_type = SDL_Window*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type window) noexcept -> bool {
      return window != nullptr;
    }

    static void release(handle_type& window) noexcept;
  };

  using Window = support::UniqueHandle<WindowRelease>;

  Sdl3WindowSurface(Sdl3Context video, Window window, Sdl3EventPump& pump);

  void refresh();

  static auto measure(SDL_Window* window) noexcept -> SurfaceSize;

  Sdl3Context video_;
  Window window_;
  std::reference_wrapper<Sdl3EventPump> pump_;
  Sdl3EventPump::Subscription events_;
  SurfaceSize size_;
  SurfaceScale scale_;
  std::deque<SurfaceEvent> pending_;
};

}
