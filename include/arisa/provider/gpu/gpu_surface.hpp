#pragma once

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::provider {

enum class SurfaceMode : std::uint8_t {
  texture,
  underlay,
};

struct SurfaceIdTag;

using SurfaceId = support::StrongValue<std::uint64_t, SurfaceIdTag>;

struct SurfaceOptions {
  SurfaceMode mode{SurfaceMode::texture};
  FrameDrive drive{FrameDrive::on_demand};
  std::optional<double> frame_rate{};
  SamplerFilter filter{SamplerFilter::linear};
  bool premultiplied{false};

  friend constexpr auto operator==(const SurfaceOptions&, const SurfaceOptions&)
      -> bool = default;
};

class SurfaceDirectory;
class SurfacePresence;

template <class... Args>
class GpuSurfaces;

class GpuSurface {
 public:
  GpuSurface(GpuSurface&& other) noexcept = default;
  auto operator=(GpuSurface&& other) noexcept -> GpuSurface& = default;
  GpuSurface(const GpuSurface&) = delete;
  auto operator=(const GpuSurface&) -> GpuSurface& = delete;
  ~GpuSurface() = default;

  auto id() const noexcept -> SurfaceId { return id_; }

  auto mode() const noexcept -> SurfaceMode { return mode_; }

  void request_frame() const;

  void set_drive(FrameDrive drive) const;

  auto drive() const -> FrameDrive;

  auto active() const noexcept -> bool;

  auto presence() const -> std::shared_ptr<const SurfacePresence>;

 private:
  template <class... Args>
  friend class GpuSurfaces;

  GpuSurface(std::shared_ptr<SurfaceDirectory> directory, SurfaceId id,
             SurfaceMode mode) noexcept;

  std::shared_ptr<SurfaceDirectory> directory_;
  SurfaceId id_;
  SurfaceMode mode_;
  support::ScopedCallback release_;
};

}
