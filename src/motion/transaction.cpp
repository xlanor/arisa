#include <arisa/motion/transaction.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace arisa::motion {

Transaction::Transaction(std::optional<Animation> animation) noexcept
    : animation_(std::move(animation)) {}

void Transaction::set_animation(std::optional<Animation> animation) noexcept {
  animation_ = std::move(animation);
}

void Transaction::set_disables_animations(bool disables) noexcept {
  disables_animations_ = disables;
}

void Transaction::add_animation_completion(
    CompletionQueue::Completion completion) {
  if (completion_ == nullptr) {
    completion_ = std::make_shared<AnimationCompletion>();
  }
  completion_->add(std::move(completion));
}

void Transaction::add_animation_completion(
    AnimationCompletionCriteria criteria,
    CompletionQueue::Completion completion) {
  if (criteria == AnimationCompletionCriteria::removed) {
    add_animation_completion(std::move(completion));
    return;
  }
  if (logical_ == nullptr) {
    logical_ = std::make_shared<AnimationCompletion>();
  }
  logical_->add(std::move(completion));
}

void Transaction::attach(
    const std::shared_ptr<CompletionQueue>& queue) const noexcept {
  if (completion_ != nullptr) {
    completion_->attach(queue);
  }
  if (logical_ != nullptr) {
    logical_->attach(queue);
  }
}

auto Transaction::find(const void* key) const noexcept -> const void* {
  if (values_ == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find(*values_, key, &Entry::key);
  return found == values_->end() ? nullptr : found->value.get();
}

void Transaction::store(const void* key, std::shared_ptr<const void> value) {
  std::vector<Entry> next;
  if (values_ != nullptr) {
    next = *values_;
  }
  std::erase_if(next, [key](const Entry& entry) { return entry.key == key; });
  next.push_back(Entry{.key = key, .value = std::move(value)});
  values_ = std::make_shared<const std::vector<Entry>>(std::move(next));
}

}
