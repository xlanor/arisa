#include <arisa/provider/gpu/surface_directory.hpp>
#include <arisa/provider/gpu/surface_presence.hpp>

#include <utility>

namespace arisa::provider {

SurfacePresence::SurfacePresence(std::weak_ptr<SurfaceDirectory> directory,
                                 SurfaceId surface) noexcept
    : directory_(std::move(directory)), surface_(surface) {}

SurfacePresence::~SurfacePresence() {
  if (const std::shared_ptr<SurfaceDirectory> directory = directory_.lock()) {
    directory->vacate(surface_);
  }
}

}
