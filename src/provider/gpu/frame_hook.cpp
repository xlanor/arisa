#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>

#include <utility>

namespace arisa::provider {

FrameHook::FrameHook(std::shared_ptr<FrameSchedule> schedule, FrameHookId id,
                     support::ScopedCallback release) noexcept
    : schedule_(std::move(schedule)), id_(id), release_(std::move(release)) {}

void FrameHook::request_frame() const {
  if (schedule_ != nullptr) {
    schedule_->request_frame();
  }
}

void FrameHook::set_drive(FrameDrive drive) const {
  if (schedule_ != nullptr) {
    schedule_->set_drive(id_, drive);
  }
}

auto FrameHook::drive() const -> FrameDrive {
  return schedule_ != nullptr ? schedule_->drive(id_) : FrameDrive::on_demand;
}

auto FrameHook::active() const noexcept -> bool {
  return schedule_ != nullptr && release_.valid();
}

}
