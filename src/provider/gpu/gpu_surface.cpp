#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/surface_directory.hpp>
#include <arisa/provider/gpu/surface_presence.hpp>

#include <utility>

namespace arisa::provider {

GpuSurface::GpuSurface(std::shared_ptr<SurfaceDirectory> directory,
                       SurfaceId id, SurfaceMode mode) noexcept
    : directory_(std::move(directory)),
      id_(id),
      mode_(mode),
      release_(
          [directory = directory_, id]() noexcept { directory->remove(id); }) {}

void GpuSurface::request_frame() const {
  if (active()) {
    directory_->request_frame(id_);
  }
}

void GpuSurface::set_drive(FrameDrive drive) const {
  if (active()) {
    directory_->set_drive(id_, drive);
  }
}

auto GpuSurface::drive() const -> FrameDrive {
  return active() ? directory_->drive(id_) : FrameDrive::on_demand;
}

auto GpuSurface::active() const noexcept -> bool {
  return directory_ != nullptr && release_.valid();
}

auto GpuSurface::presence() const -> std::shared_ptr<const SurfacePresence> {
  return active() ? directory_->presence(id_) : nullptr;
}

}
