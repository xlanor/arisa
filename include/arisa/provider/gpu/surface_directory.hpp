#pragma once

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::provider {

class SurfacePresence;

class SurfaceDirectory {
 public:
  struct Placement {
    TextureId texture{0};
    SurfaceMode mode{SurfaceMode::texture};

    friend constexpr auto operator==(const Placement&, const Placement&)
        -> bool = default;
  };

  struct Listing {
    SurfaceId surface{0};
    SurfaceMode mode{SurfaceMode::texture};
    FrameDrive drive{FrameDrive::on_demand};
    std::optional<double> frame_rate{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t generation{0};

    friend constexpr auto operator==(const Listing&, const Listing&)
        -> bool = default;
  };

  SurfaceDirectory() = default;
  SurfaceDirectory(const SurfaceDirectory&) = delete;
  auto operator=(const SurfaceDirectory&) -> SurfaceDirectory& = delete;
  SurfaceDirectory(SurfaceDirectory&&) = delete;
  auto operator=(SurfaceDirectory&&) -> SurfaceDirectory& = delete;
  virtual ~SurfaceDirectory() = default;

  virtual auto place(SurfaceId surface, PassRegion area)
      -> std::optional<Placement> = 0;

  virtual void advance() = 0;

  virtual void request_frame(SurfaceId surface) = 0;

  virtual void set_drive(SurfaceId surface, FrameDrive drive) = 0;

  virtual auto drive(SurfaceId surface) const -> FrameDrive = 0;

  virtual auto presence(SurfaceId surface)
      -> std::shared_ptr<const SurfacePresence> = 0;

  virtual void vacate(SurfaceId surface) noexcept = 0;

  virtual void remove(SurfaceId surface) noexcept = 0;

  virtual auto listing() const -> std::vector<Listing> = 0;
};

}
