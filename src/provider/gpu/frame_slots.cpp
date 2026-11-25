#include <arisa/provider/gpu/frame_slots.hpp>

#include <algorithm>

namespace arisa::provider {

FrameSlots::FrameSlots(std::uint32_t count) noexcept
    : count_(std::max<std::uint32_t>(count, 1)) {}

auto FrameSlots::claim() -> std::optional<Claim> {
  std::unique_lock lock{mutex_};
  if (claimed_) {
    return std::nullopt;
  }
  ready_.wait(lock, [this] { return built_ < allowed_; });
  claimed_ = true;
  return Claim{.slot = static_cast<std::uint32_t>(built_ % count_),
               .frame = built_};
}

auto FrameSlots::await() -> std::optional<std::uint32_t> {
  std::unique_lock lock{mutex_};
  const std::uint64_t seen = interrupts_;
  ready_.wait(lock,
              [this, seen] { return built_ < allowed_ || interrupts_ != seen; });
  if (built_ >= allowed_) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(built_ % count_);
}

void FrameSlots::interrupt() noexcept {
  {
    const std::scoped_lock lock{mutex_};
    ++interrupts_;
  }
  ready_.notify_all();
}

auto FrameSlots::next() const -> Claim {
  const std::scoped_lock lock{mutex_};
  return Claim{.slot = static_cast<std::uint32_t>(built_ % count_),
               .frame = built_};
}

void FrameSlots::abandon() noexcept {
  const std::scoped_lock lock{mutex_};
  claimed_ = false;
}

void FrameSlots::seal() noexcept {
  {
    const std::scoped_lock lock{mutex_};
    claimed_ = false;
    ++built_;
  }
  ready_.notify_all();
}

void FrameSlots::release(std::uint64_t frame) noexcept {
  {
    const std::scoped_lock lock{mutex_};
    allowed_ = std::max(allowed_, frame + 2);
  }
  ready_.notify_all();
}

auto FrameSlots::claimed() const -> bool {
  const std::scoped_lock lock{mutex_};
  return claimed_;
}

}
