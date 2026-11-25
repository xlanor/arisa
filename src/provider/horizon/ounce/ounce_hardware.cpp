#include <arisa/provider/horizon/horizon_threading.hpp>
#include <arisa/provider/horizon/ounce/ounce_hardware.hpp>

#include <cstdint>
#include <limits>

namespace arisa::provider::horizon::ounce {

auto OunceHardware::configure_window(NWindow* window,
                                     std::optional<SurfaceSize> docked_cap)
    -> std::expected<void, ProviderError> {
  const SurfaceSize docked = docked_cap.has_value() && !docked_cap->empty()
                                 ? *docked_cap
                                 : SurfaceSize{};
  if (const Result configured = nwindowSetModeDimensions(
          window, 0, 0, docked.width.value(), docked.height.value());
      R_FAILED(configured)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, configured});
  }
  return {};
}

auto OunceHardware::framebuffer(NWindow* window, AppletOperationMode) noexcept
    -> SurfaceSize {
  u32 width = 0;
  u32 height = 0;
  if (window == nullptr ||
      R_FAILED(nwindowGetDimensions(window, &width, &height)) || width == 0 ||
      height == 0) {
    return handheld_size;
  }
  return SurfaceSize{.width = SurfaceWidth{width},
                     .height = SurfaceHeight{height}};
}

auto OunceHardware::placement() noexcept -> ThreadPlacementPolicy {
  return ThreadPlacementPolicy{}.restricted_to(
      CoreMask{HorizonThreadingProvider::usable_cores(
          CoreMask{std::numeric_limits<std::uint64_t>::max()})});
}

}
