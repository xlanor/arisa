#pragma once

#include <arisa/provider/audio/app_voice.hpp>
#include <arisa/provider/error.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <mutex>
#include <optional>
#include <vector>

namespace arisa::provider {

class VoiceSlots {
 public:
  enum class State : std::uint8_t {
    free,
    writing,
    queued,
  };

  struct Queued {
    std::uint32_t slot{0};
    std::uint32_t frames{0};

    friend constexpr auto operator==(const Queued&, const Queued&)
        -> bool = default;
  };

  explicit VoiceSlots(std::uint32_t count);

  [[nodiscard]] auto take(std::optional<std::chrono::nanoseconds> timeout)
      -> std::optional<std::uint32_t>;

  void give_back(std::uint32_t slot) noexcept;

  template <class Enqueue>
  [[nodiscard]] auto queue(std::uint32_t slot, std::uint32_t frames,
                           Enqueue&& enqueue)
      -> std::expected<void, ProviderError> {
    {
      const std::scoped_lock lock{mutex_};
      if (slot >= states_.size() || states_[slot] != State::writing) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::invalid_argument});
      }
      if (enqueue()) {
        mark_queued(slot, frames);
        return {};
      }
      states_[slot] = State::free;
    }
    freed_.notify_all();
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }

  template <class Finished>
  void reap(Finished&& finished) {
    bool freed = false;
    {
      const std::scoped_lock lock{mutex_};
      for (auto each = order_.begin(); each != order_.end();) {
        if (finished(each->slot)) {
          states_[each->slot] = State::free;
          each = order_.erase(each);
          count_played();
          freed = true;
        } else {
          ++each;
        }
      }
    }
    if (freed) {
      freed_.notify_all();
    }
  }

  void played(std::uint32_t slot);

  auto queued() const -> std::vector<Queued>;

  auto state(std::uint32_t slot) const -> std::optional<State>;

  auto statistics() const -> AppVoiceStatistics;

  void close();

 private:
  void mark_queued(std::uint32_t slot, std::uint32_t frames);

  void count_played() noexcept;

  mutable std::mutex mutex_;
  std::condition_variable freed_;
  std::vector<State> states_;
  std::deque<Queued> order_;
  std::uint64_t submitted_{0};
  std::uint64_t played_{0};
  std::uint64_t underruns_{0};
  bool closed_{false};
};

}
