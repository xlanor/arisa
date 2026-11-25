#include <arisa/provider/gpu/frame_schedule.hpp>

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::provider {

auto FrameSchedule::demand() const -> Demand {
  const std::scoped_lock lock{mutex_};
  Demand found;
  bool unhinted = false;
  for (const Lease& held : hooks_ | std::views::values) {
    const FrameHookOptions& options = held.options;
    if (options.drive != FrameDrive::continuous) {
      continue;
    }
    found.continuous = true;
    if (!options.frame_rate.has_value()) {
      unhinted = true;
      continue;
    }
    found.frame_rate =
        std::max(found.frame_rate.value_or(0.0), *options.frame_rate);
  }
  if (unhinted) {
    found.frame_rate.reset();
  }
  return found;
}

auto FrameSchedule::listing() const -> std::vector<Listing> {
  const std::scoped_lock lock{mutex_};
  std::vector<Listing> found;
  found.reserve(hooks_.size());
  for (const auto& [id, held] : hooks_) {
    found.push_back(Listing{.id = FrameHookId{id},
                            .drive = held.options.drive,
                            .frame_rate = held.options.frame_rate,
                            .surface = !held.hooked});
  }
  return found;
}

auto FrameSchedule::installed() const noexcept -> bool {
  return installed_.load(std::memory_order_acquire) > 0;
}

auto FrameSchedule::take_request() noexcept -> bool {
  return requested_.exchange(false, std::memory_order_acq_rel);
}

void FrameSchedule::request_frame() {
  requested_.store(true, std::memory_order_release);
  wake();
}

auto FrameSchedule::wake_with(Waker waker) -> support::ScopedCallback {
  {
    const std::scoped_lock lock{waker_mutex_};
    waker_ = std::move(waker);
  }
  return support::ScopedCallback{[weak = weak_from_this()]() noexcept {
    if (const std::shared_ptr<FrameSchedule> schedule = weak.lock()) {
      const std::scoped_lock lock{schedule->waker_mutex_};
      schedule->waker_ = nullptr;
    }
  }};
}

auto FrameSchedule::next_serial() noexcept -> GpuSerial {
  return GpuSerial{serial_.fetch_add(1, std::memory_order_acq_rel) + 1};
}

auto FrameSchedule::next_serial_after(GpuSerial last) noexcept
    -> std::optional<GpuSerial> {
  std::uint64_t expected = last.value();
  if (!serial_.compare_exchange_strong(expected, expected + 1,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
    return std::nullopt;
  }
  return GpuSerial{expected + 1};
}

auto FrameSchedule::serial() const noexcept -> GpuSerial {
  return GpuSerial{serial_.load(std::memory_order_acquire)};
}

auto FrameSchedule::adopt(FrameHookOptions options) -> FrameHookId {
  return lease(options, true);
}

auto FrameSchedule::lease(FrameHookOptions options, bool hooked)
    -> FrameHookId {
  std::uint64_t id = 0;
  {
    const std::scoped_lock lock{mutex_};
    id = next_id_++;
    hooks_.insert_or_assign(id, Lease{.options = options, .hooked = hooked});
    if (hooked) {
      ++hooked_;
    }
    installed_.store(hooked_, std::memory_order_release);
  }
  wake();
  return FrameHookId{id};
}

void FrameSchedule::retire(FrameHookId id) {
  {
    const std::scoped_lock lock{mutex_};
    const auto found = hooks_.find(id.value());
    if (found != hooks_.end()) {
      if (found->second.hooked) {
        --hooked_;
      }
      hooks_.erase(found);
    }
    installed_.store(hooked_, std::memory_order_release);
  }
  wake();
}

void FrameSchedule::set_drive(FrameHookId id, FrameDrive drive) {
  {
    const std::scoped_lock lock{mutex_};
    const auto found = hooks_.find(id.value());
    if (found == hooks_.end() || found->second.options.drive == drive) {
      return;
    }
    found->second.options.drive = drive;
  }
  wake();
}

auto FrameSchedule::drive(FrameHookId id) const -> FrameDrive {
  const std::scoped_lock lock{mutex_};
  const auto found = hooks_.find(id.value());
  return found == hooks_.end() ? FrameDrive::on_demand
                               : found->second.options.drive;
}

void FrameSchedule::wake() {
  const std::scoped_lock lock{waker_mutex_};
  if (waker_) {
    waker_();
  }
}

}
