#pragma once

#include <arisa/provider/gpu/swap_interval.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider {

template <class Payload = std::monostate>
class LentSwapchain {
 public:
  struct Outstanding {
    std::uint32_t index{0};
    Payload payload{};
  };

  struct Settlement {
    std::vector<Outstanding> outstanding;
    std::optional<SwapInterval> interval;
  };

  void lend(std::uint32_t images) {
    const std::scoped_lock lock{mutex_};
    acquired_.assign(images, std::nullopt);
    deferred_.reset();
    lent_ = true;
  }

  [[nodiscard]] auto resize(std::uint32_t images) -> bool {
    const std::scoped_lock lock{mutex_};
    if (std::ranges::any_of(acquired_, [](const std::optional<Payload>& each) {
          return each.has_value();
        })) {
      return false;
    }
    acquired_.assign(images, std::nullopt);
    return true;
  }

  auto lent() const -> bool {
    const std::scoped_lock lock{mutex_};
    return lent_;
  }

  [[nodiscard]] auto acquire(std::uint32_t index, Payload payload = {})
      -> bool {
    const std::scoped_lock lock{mutex_};
    if (!lent_ || index >= acquired_.size() || acquired_[index].has_value()) {
      return false;
    }
    acquired_[index] = std::move(payload);
    return true;
  }

  [[nodiscard]] auto present(std::uint32_t index) -> bool {
    const std::scoped_lock lock{mutex_};
    if (!lent_ || index >= acquired_.size() || !acquired_[index].has_value()) {
      return false;
    }
    acquired_[index].reset();
    return true;
  }

  auto acquired(std::uint32_t index) const -> bool {
    const std::scoped_lock lock{mutex_};
    return index < acquired_.size() && acquired_[index].has_value();
  }

  auto outstanding() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return static_cast<std::size_t>(std::ranges::count_if(
        acquired_,
        [](const std::optional<Payload>& each) { return each.has_value(); }));
  }

  [[nodiscard]] auto request(SwapInterval wanted)
      -> std::optional<SwapInterval> {
    const std::scoped_lock lock{mutex_};
    if (lent_) {
      deferred_ = wanted;
      return std::nullopt;
    }
    return wanted;
  }

  [[nodiscard]] auto settle() -> Settlement {
    const std::scoped_lock lock{mutex_};
    Settlement settled{.outstanding = {}, .interval = deferred_};
    for (std::uint32_t index = 0; index < acquired_.size(); ++index) {
      if (acquired_[index].has_value()) {
        settled.outstanding.push_back(Outstanding{
            .index = index, .payload = std::move(*acquired_[index])});
      }
    }
    acquired_.clear();
    deferred_.reset();
    lent_ = false;
    return settled;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::optional<Payload>> acquired_;
  std::optional<SwapInterval> deferred_;
  bool lent_{false};
};

}
