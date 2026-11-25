#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <array>
#include <bitset>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>

namespace arisa::provider {

class OverlaySlots {
 public:
  static constexpr std::uint32_t maximum = 8;

  using Presented = std::bitset<maximum>;

  enum class Leftovers : std::uint8_t { dropped, carried };

  struct Use {
    std::uint32_t slot{0};
    std::uint64_t value{0};
    bool fresh{false};
    std::array<std::uint32_t, maximum> carried{};
    std::uint32_t carrying{0};
  };

  struct Standing {
    std::uint64_t last_use{0};
    bool presented{false};
  };

  OverlaySlots(std::uint32_t count, Leftovers leftovers) noexcept;

  OverlaySlots(const OverlaySlots&) = delete;
  auto operator=(const OverlaySlots&) -> OverlaySlots& = delete;
  OverlaySlots(OverlaySlots&&) = delete;
  auto operator=(OverlaySlots&&) -> OverlaySlots& = delete;
  ~OverlaySlots() = default;

  auto count() const noexcept -> std::uint32_t { return count_; }

  template <class Idle>
  [[nodiscard]] auto claim(Idle&& idle) -> std::optional<std::uint32_t> {
    const std::scoped_lock lock{mutex_};
    if (closed_ || building_.has_value()) {
      return std::nullopt;
    }
    for (std::uint32_t step = 0; step < count_; ++step) {
      const std::uint32_t index = (next_ + step) % count_;
      Slot& slot = slots_[index];
      if (slot.state != State::free) {
        continue;
      }
      if (!idle(index, Standing{.last_use = slot.last_use,
                                .presented = slot.presented})) {
        continue;
      }
      slot.state = State::building;
      building_ = index;
      next_ = (index + 1) % count_;
      return index;
    }
    return std::nullopt;
  }

  void abandon(std::uint32_t slot) noexcept;

  [[nodiscard]] auto publish(std::uint32_t slot, bool empty,
                             SurfaceSize extent) noexcept -> bool;

  [[nodiscard]] auto use(std::optional<SurfaceSize> target)
      -> std::expected<std::optional<Use>, ProviderError>;

  template <class Signal>
  auto present(Signal&& signal) -> Presented {
    Presented presented;
    {
      const std::scoped_lock lock{mutex_};
      for (std::uint32_t index = 0; index < count_; ++index) {
        Slot& slot = slots_[index];
        if (slot.last_use != 0 && !slot.presented) {
          signal(index);
          slot.presented = true;
          presented.set(index);
        }
      }
    }
    return presented;
  }

  auto await(std::uint64_t after, std::chrono::nanoseconds limit) const
      -> std::uint64_t;

  auto await_scene(std::uint64_t after, std::chrono::nanoseconds limit) const
      -> std::uint64_t;

  void close() noexcept;

  auto closed() const -> bool;

  auto building() const -> std::optional<std::uint32_t>;

  auto current() const -> std::optional<std::uint32_t>;

  auto standing(std::uint32_t slot) const -> Standing;

  auto scenes() const -> std::uint64_t;

  auto uses() const -> std::uint64_t;

 private:
  enum class State : std::uint8_t { free, building, current, left };

  struct Slot {
    State state{State::free};
    std::uint64_t last_use{0};
    std::uint64_t sequence{0};
    SurfaceSize extent{};
    bool presented{true};
    bool fresh{false};
  };

  auto settle(std::uint32_t slot, bool empty, SurfaceSize extent) noexcept
      -> bool;

  void carry(Use& use);

  void retire_unused(Slot& slot) const noexcept;

  mutable std::mutex mutex_;
  mutable std::condition_variable changed_;
  std::array<Slot, maximum> slots_{};
  std::uint32_t count_;
  Leftovers leftovers_;
  std::uint32_t next_{0};
  std::optional<std::uint32_t> building_;
  std::optional<std::uint32_t> current_;
  std::uint64_t uses_{0};
  std::uint64_t published_{0};
  bool blank_{false};
  bool closed_{false};
};

}
