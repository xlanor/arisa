#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <arisa/support/unique_handle.hpp>

#include <switch.h>

#include <deque>
#include <expected>
#include <optional>

namespace arisa::provider::horizon {

class HorizonSurface final : public SurfaceProvider {
 public:
  [[nodiscard]] static auto create(
      std::optional<SurfaceSize> docked_cap = std::nullopt)
      -> std::expected<HorizonSurface, ProviderError>;

  auto size() const noexcept -> SurfaceSize override;

  auto scale() const noexcept -> SurfaceScale override;

  auto display_mode() const noexcept -> DisplayMode override;

  auto poll() -> std::optional<SurfaceEvent> override;

  auto window() const noexcept -> NWindow*;

  void release_layer_on_close() noexcept { releases_layer_ = true; }

  HorizonSurface(const HorizonSurface&) = delete;
  auto operator=(const HorizonSurface&) -> HorizonSurface& = delete;
  HorizonSurface(HorizonSurface&& other) noexcept = default;
  auto operator=(HorizonSurface&& other) noexcept -> HorizonSurface& = default;
  ~HorizonSurface() override;

 private:
  struct WindowRelease {
    using handle_type = NWindow*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type window) noexcept -> bool {
      return window != nullptr;
    }
    static void release(handle_type& window) noexcept;
  };

  using Window = support::UniqueHandle<WindowRelease>;

  HorizonSurface(NWindow* window, AppletOperationMode mode) noexcept;

  Window window_;
  AppletOperationMode reported_mode_;
  SurfaceSize reported_size_;
  std::deque<SurfaceEvent> pending_;
  bool releases_layer_{false};
};

}
