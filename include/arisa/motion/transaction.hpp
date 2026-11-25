#pragma once

#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_completion.hpp>
#include <arisa/motion/animation_completion_criteria.hpp>
#include <arisa/motion/completion_queue.hpp>
#include <arisa/motion/scroll_content_offset_adjustment_behavior.hpp>
#include <arisa/motion/transaction_key.hpp>
#include <arisa/motion/unit_point.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::motion {

class Transaction {
 public:
  Transaction() noexcept = default;

  explicit Transaction(std::optional<Animation> animation) noexcept;

  auto animation() const noexcept -> const std::optional<Animation>& {
    return animation_;
  }

  void set_animation(std::optional<Animation> animation) noexcept;

  auto disables_animations() const noexcept -> bool {
    return disables_animations_;
  }

  void set_disables_animations(bool disables) noexcept;

  auto is_continuous() const noexcept -> bool { return continuous_; }

  void set_is_continuous(bool continuous) noexcept { continuous_ = continuous; }

  auto tracks_velocity() const noexcept -> bool { return tracks_velocity_; }

  void set_tracks_velocity(bool tracks) noexcept { tracks_velocity_ = tracks; }

  auto scroll_target_anchor() const noexcept -> std::optional<UnitPoint> {
    return scroll_target_anchor_;
  }

  void set_scroll_target_anchor(std::optional<UnitPoint> anchor) noexcept {
    scroll_target_anchor_ = anchor;
  }

  auto scroll_position_update_preserves_velocity() const noexcept -> bool {
    return preserves_velocity_;
  }

  void set_scroll_position_update_preserves_velocity(bool preserves) noexcept {
    preserves_velocity_ = preserves;
  }

  auto scroll_content_offset_adjustment_behavior() const noexcept
      -> ScrollContentOffsetAdjustmentBehavior {
    return offset_adjustment_;
  }

  void set_scroll_content_offset_adjustment_behavior(
      ScrollContentOffsetAdjustmentBehavior behavior) noexcept {
    offset_adjustment_ = behavior;
  }

  void add_animation_completion(CompletionQueue::Completion completion);

  void add_animation_completion(AnimationCompletionCriteria criteria,
                                CompletionQueue::Completion completion);

  auto completion() const noexcept
      -> const std::shared_ptr<AnimationCompletion>& {
    return completion_;
  }

  auto logical_completion() const noexcept
      -> const std::shared_ptr<AnimationCompletion>& {
    return logical_;
  }

  template <class T>
  auto operator[](const TransactionKey<T>& key) const -> const T& {
    if (const void* found = find(key.identity()); found != nullptr) {
      return *static_cast<const T*>(found);
    }
    return key.default_value();
  }

  template <class T>
  void set(const TransactionKey<T>& key, T value) {
    store(key.identity(), std::make_shared<const T>(std::move(value)));
  }

  void attach(const std::shared_ptr<CompletionQueue>& queue) const noexcept;

 private:
  struct Entry {
    const void* key{nullptr};
    std::shared_ptr<const void> value;
  };

  auto find(const void* key) const noexcept -> const void*;

  void store(const void* key, std::shared_ptr<const void> value);

  std::optional<Animation> animation_;
  bool disables_animations_{false};
  bool continuous_{false};
  bool tracks_velocity_{false};
  std::optional<UnitPoint> scroll_target_anchor_;
  bool preserves_velocity_{false};
  ScrollContentOffsetAdjustmentBehavior offset_adjustment_{
      ScrollContentOffsetAdjustmentBehavior::automatic};
  std::shared_ptr<AnimationCompletion> completion_;
  std::shared_ptr<AnimationCompletion> logical_;
  std::shared_ptr<const std::vector<Entry>> values_;
};

}
