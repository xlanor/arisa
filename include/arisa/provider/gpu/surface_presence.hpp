#pragma once

#include <arisa/provider/gpu/gpu_surface.hpp>

#include <memory>

namespace arisa::provider {

class SurfaceDirectory;

class SurfacePresence {
 public:
  SurfacePresence(std::weak_ptr<SurfaceDirectory> directory,
                  SurfaceId surface) noexcept;

  SurfacePresence(const SurfacePresence&) = delete;
  auto operator=(const SurfacePresence&) -> SurfacePresence& = delete;
  SurfacePresence(SurfacePresence&&) = delete;
  auto operator=(SurfacePresence&&) -> SurfacePresence& = delete;
  ~SurfacePresence();

  auto surface() const noexcept -> SurfaceId { return surface_; }

 private:
  std::weak_ptr<SurfaceDirectory> directory_;
  SurfaceId surface_;
};

}
