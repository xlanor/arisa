#pragma once

#include <arisa/provider/surface/provider.hpp>

#include <deque>
#include <mutex>

namespace arisa::provider::headless {

class ScriptedSurface final : public SurfaceProvider {
 public:
  static constexpr SurfaceSize default_size{.width = SurfaceWidth{1280},
                                            .height = SurfaceHeight{720}};

  auto size() const noexcept -> SurfaceSize override;

  auto scale() const noexcept -> SurfaceScale override;

  auto display_mode() const noexcept -> DisplayMode override;

  auto poll() -> std::optional<SurfaceEvent> override;

  void resize(SurfaceSize size);

  void set_scale(SurfaceScale scale);

  void set_display_mode(DisplayMode mode);

  void push(SurfaceEvent event);

 private:
  mutable std::mutex mutex_;
  SurfaceSize size_{default_size};
  SurfaceScale scale_{SurfaceScale::unit()};
  DisplayMode mode_{DisplayMode::desktop};
  std::deque<SurfaceEvent> pending_;
};

}
