#include <runtime/feedback_player.hpp>

#include <cstddef>
#include <utility>

namespace arisa::runtime {

auto FeedbackPlayer::gap(provider::SensoryFeedback::Kind kind) noexcept
    -> std::chrono::milliseconds {
  using Kind = provider::SensoryFeedback::Kind;
  switch (kind) {
    case Kind::selection:
    case Kind::alignment:
      return std::chrono::milliseconds{90};
    case Kind::increase:
    case Kind::decrease:
      return std::chrono::milliseconds{70};
    case Kind::press:
    case Kind::release:
      return std::chrono::milliseconds{40};
    case Kind::impact:
      return std::chrono::milliseconds{100};
    case Kind::success:
    case Kind::warning:
    case Kind::error:
    case Kind::start:
    case Kind::stop:
    case Kind::level_change:
    case Kind::path_complete:
      break;
  }
  return std::chrono::milliseconds{250};
}

void FeedbackPlayer::play(std::span<const provider::SensoryFeedback> requested,
                          std::chrono::nanoseconds now,
                          provider::HapticStrength strength) {
  if (requested.empty() || strength.silent()) {
    return;
  }
  coalesced_ += requested.size() - 1;
  const provider::SensoryFeedback& latest = requested.back();
  std::optional<std::chrono::nanoseconds>& last =
      last_[static_cast<std::size_t>(std::to_underlying(latest.kind()))];
  if (last.has_value() && now - *last < gap(latest.kind())) {
    ++coalesced_;
    return;
  }
  last = now;
  ++played_;
  static_cast<void>(haptics_.get().play(latest, strength));
}

}
