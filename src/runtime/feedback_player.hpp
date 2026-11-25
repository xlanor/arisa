#pragma once

#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/haptics/sensory_feedback.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace arisa::runtime {

class FeedbackPlayer {
 public:
  explicit FeedbackPlayer(provider::HapticsProvider& haptics) noexcept
      : haptics_(haptics) {}

  static auto gap(provider::SensoryFeedback::Kind kind) noexcept
      -> std::chrono::milliseconds;

  void play(std::span<const provider::SensoryFeedback> requested,
            std::chrono::nanoseconds now, provider::HapticStrength strength);

  auto played() const noexcept -> std::uint64_t { return played_; }

  auto coalesced() const noexcept -> std::uint64_t { return coalesced_; }

 private:
  std::reference_wrapper<provider::HapticsProvider> haptics_;
  std::array<std::optional<std::chrono::nanoseconds>,
             provider::SensoryFeedback::kind_count>
      last_{};
  std::uint64_t played_{0};
  std::uint64_t coalesced_{0};
};

}
