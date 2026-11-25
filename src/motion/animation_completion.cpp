#include <arisa/motion/animation_completion.hpp>

#include <utility>

namespace arisa::motion {

AnimationCompletion::~AnimationCompletion() {
  if (const auto queue = queue_.lock()) {
    for (CompletionQueue::Completion& completion : completions_) {
      queue->push(std::move(completion));
    }
    return;
  }
  for (CompletionQueue::Completion& completion : completions_) {
    completion();
  }
}

void AnimationCompletion::add(CompletionQueue::Completion completion) {
  completions_.push_back(std::move(completion));
}

void AnimationCompletion::attach(
    const std::shared_ptr<CompletionQueue>& queue) noexcept {
  queue_ = queue;
}

}
