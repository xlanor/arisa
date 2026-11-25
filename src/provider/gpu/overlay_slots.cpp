#include <arisa/provider/gpu/overlay_slots.hpp>

#include <algorithm>
#include <span>
#include <utility>

namespace arisa::provider {

OverlaySlots::OverlaySlots(std::uint32_t count, Leftovers leftovers) noexcept
    : count_(std::clamp<std::uint32_t>(count, 1, maximum)),
      leftovers_(leftovers) {}

void OverlaySlots::retire_unused(Slot& slot) const noexcept {
  slot.fresh = false;
  if (uses_ == 0) {
    return;
  }
  slot.last_use = std::max(slot.last_use, uses_);
  slot.presented = false;
}

void OverlaySlots::abandon(std::uint32_t slot) noexcept {
  const std::scoped_lock lock{mutex_};
  if (slot >= count_ || building_ != slot) {
    return;
  }
  slots_[slot].state = State::free;
  retire_unused(slots_[slot]);
  building_.reset();
}

auto OverlaySlots::publish(std::uint32_t slot, bool empty,
                           SurfaceSize extent) noexcept -> bool {
  bool published = false;
  {
    const std::scoped_lock lock{mutex_};
    published = settle(slot, empty, extent);
  }
  if (published) {
    changed_.notify_all();
  }
  return published;
}

auto OverlaySlots::settle(std::uint32_t slot, bool empty,
                          SurfaceSize extent) noexcept -> bool {
  if (slot >= count_ || building_ != slot) {
    return false;
  }
  building_.reset();
  Slot& published = slots_[slot];
  if (closed_) {
    published.state = State::free;
    retire_unused(published);
    return false;
  }
  if (current_.has_value()) {
    Slot& superseded = slots_[*current_];
    if (!superseded.fresh) {
      superseded.state = State::free;
    } else if (leftovers_ == Leftovers::carried) {
      superseded.state = State::left;
    } else {
      superseded.state = State::free;
      retire_unused(superseded);
    }
    current_.reset();
  }
  published.sequence = ++published_;
  published.extent = extent;
  const bool carrying = std::ranges::any_of(
      std::span{slots_}.first(count_),
      [](const Slot& each) { return each.state == State::left; });
  if (empty && !carrying) {
    published.state = State::free;
    retire_unused(published);
    blank_ = true;
    return true;
  }
  blank_ = false;
  published.state = State::current;
  published.fresh = true;
  current_ = slot;
  return true;
}

auto OverlaySlots::use(std::optional<SurfaceSize> target)
    -> std::expected<std::optional<Use>, ProviderError> {
  std::optional<Use> use;
  {
    const std::scoped_lock lock{mutex_};
    if (closed_) {
      return std::nullopt;
    }
    if (!current_.has_value()) {
      if (!std::exchange(blank_, false)) {
        return std::nullopt;
      }
      ++uses_;
    } else {
      Slot& slot = slots_[*current_];
      if (target.has_value() && !slot.extent.empty() &&
          *target != slot.extent) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::invalid_argument,
                          ProviderErrorDetail::overlay_extent});
      }
      slot.last_use = ++uses_;
      slot.presented = false;
      use = Use{.slot = *current_,
                .value = slot.last_use,
                .fresh = std::exchange(slot.fresh, false),
                .carried = {},
                .carrying = 0};
      if (use->fresh) {
        carry(*use);
      }
    }
  }
  changed_.notify_all();
  return use;
}

void OverlaySlots::carry(Use& use) {
  for (std::uint32_t index = 0; index < count_; ++index) {
    Slot& left = slots_[index];
    if (left.state != State::left) {
      continue;
    }
    left.state = State::free;
    left.fresh = false;
    left.last_use = use.value;
    left.presented = false;
    use.carried[use.carrying++] = index;
  }
  std::ranges::sort(std::span{use.carried}.first(use.carrying),
                    [this](std::uint32_t a, std::uint32_t b) {
                      return slots_[a].sequence < slots_[b].sequence;
                    });
}

auto OverlaySlots::await(std::uint64_t after,
                         std::chrono::nanoseconds limit) const
    -> std::uint64_t {
  std::unique_lock lock{mutex_};
  changed_.wait_for(lock, limit,
                    [this, after] { return closed_ || uses_ > after; });
  return uses_;
}

auto OverlaySlots::await_scene(std::uint64_t after,
                               std::chrono::nanoseconds limit) const
    -> std::uint64_t {
  std::unique_lock lock{mutex_};
  changed_.wait_for(lock, limit,
                    [this, after] { return closed_ || published_ > after; });
  return published_;
}

void OverlaySlots::close() noexcept {
  {
    const std::scoped_lock lock{mutex_};
    closed_ = true;
    blank_ = false;
    current_.reset();
    for (Slot& slot : slots_) {
      if (slot.state == State::current || slot.state == State::left) {
        slot.state = State::free;
      }
    }
  }
  changed_.notify_all();
}

auto OverlaySlots::closed() const -> bool {
  const std::scoped_lock lock{mutex_};
  return closed_;
}

auto OverlaySlots::building() const -> std::optional<std::uint32_t> {
  const std::scoped_lock lock{mutex_};
  return building_;
}

auto OverlaySlots::current() const -> std::optional<std::uint32_t> {
  const std::scoped_lock lock{mutex_};
  return current_;
}

auto OverlaySlots::standing(std::uint32_t slot) const -> Standing {
  const std::scoped_lock lock{mutex_};
  if (slot >= count_) {
    return {};
  }
  return Standing{.last_use = slots_[slot].last_use,
                  .presented = slots_[slot].presented};
}

auto OverlaySlots::scenes() const -> std::uint64_t {
  const std::scoped_lock lock{mutex_};
  return published_;
}

auto OverlaySlots::uses() const -> std::uint64_t {
  const std::scoped_lock lock{mutex_};
  return uses_;
}

}
