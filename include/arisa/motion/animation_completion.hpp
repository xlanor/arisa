#pragma once

#include <arisa/motion/completion_queue.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace arisa::motion {

class AnimationCompletion {
 public:
  AnimationCompletion() noexcept = default;

  AnimationCompletion(const AnimationCompletion&) = delete;
  auto operator=(const AnimationCompletion&) -> AnimationCompletion& = delete;
  AnimationCompletion(AnimationCompletion&&) = delete;
  auto operator=(AnimationCompletion&&) -> AnimationCompletion& = delete;
  ~AnimationCompletion();

  void add(CompletionQueue::Completion completion);

  void attach(const std::shared_ptr<CompletionQueue>& queue) noexcept;

 private:
  std::vector<CompletionQueue::Completion> completions_;
  std::weak_ptr<CompletionQueue> queue_;
};

}
