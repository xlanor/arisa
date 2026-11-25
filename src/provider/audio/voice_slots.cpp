#include <arisa/provider/audio/voice_slots.hpp>

#include <algorithm>
#include <ranges>

namespace arisa::provider {

VoiceSlots::VoiceSlots(std::uint32_t count) : states_(count, State::free) {}

auto VoiceSlots::take(std::optional<std::chrono::nanoseconds> timeout)
    -> std::optional<std::uint32_t> {
  std::unique_lock lock{mutex_};
  const auto available = [this] {
    return closed_ || std::ranges::contains(states_, State::free);
  };
  if (timeout.has_value() && !available()) {
    static_cast<void>(freed_.wait_for(lock, *timeout, available));
  }
  if (closed_) {
    return std::nullopt;
  }
  const auto found = std::ranges::find(states_, State::free);
  if (found == states_.end()) {
    return std::nullopt;
  }
  *found = State::writing;
  return static_cast<std::uint32_t>(found - states_.begin());
}

void VoiceSlots::give_back(std::uint32_t slot) noexcept {
  {
    const std::scoped_lock lock{mutex_};
    if (slot >= states_.size() || states_[slot] != State::writing) {
      return;
    }
    states_[slot] = State::free;
  }
  freed_.notify_all();
}

void VoiceSlots::mark_queued(std::uint32_t slot, std::uint32_t frames) {
  states_[slot] = State::queued;
  order_.push_back(Queued{.slot = slot, .frames = frames});
  ++submitted_;
}

void VoiceSlots::count_played() noexcept {
  ++played_;
  if (order_.empty()) {
    ++underruns_;
  }
}

void VoiceSlots::played(std::uint32_t slot) {
  {
    const std::scoped_lock lock{mutex_};
    const auto found = std::ranges::find(order_, slot, &Queued::slot);
    if (found == order_.end()) {
      return;
    }
    order_.erase(found);
    states_[slot] = State::free;
    count_played();
  }
  freed_.notify_all();
}

auto VoiceSlots::queued() const -> std::vector<Queued> {
  const std::scoped_lock lock{mutex_};
  return {order_.begin(), order_.end()};
}

auto VoiceSlots::state(std::uint32_t slot) const -> std::optional<State> {
  const std::scoped_lock lock{mutex_};
  if (slot >= states_.size()) {
    return std::nullopt;
  }
  return states_[slot];
}

auto VoiceSlots::statistics() const -> AppVoiceStatistics {
  const std::scoped_lock lock{mutex_};
  std::uint64_t frames = 0;
  for (const Queued& each : order_) {
    frames += each.frames;
  }
  return AppVoiceStatistics{
      .submitted = submitted_,
      .played = played_,
      .underruns = underruns_,
      .queued = static_cast<std::uint32_t>(order_.size()),
      .latency = std::chrono::nanoseconds{static_cast<std::int64_t>(
          frames * 1'000'000'000ULL / AppVoice::sample_rate)}};
}

void VoiceSlots::close() {
  {
    const std::scoped_lock lock{mutex_};
    closed_ = true;
  }
  freed_.notify_all();
}

}
