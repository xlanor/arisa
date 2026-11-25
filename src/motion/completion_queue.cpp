#include <arisa/motion/completion_queue.hpp>

#include <utility>

namespace arisa::motion {

void CompletionQueue::push(Completion completion) {
  waiting_.push_back(std::move(completion));
}

auto CompletionQueue::drain() -> std::size_t {
  std::size_t ran = 0;
  while (!waiting_.empty()) {
    std::vector<Completion> ready = std::exchange(waiting_, {});
    for (Completion& completion : ready) {
      completion();
      ++ran;
    }
  }
  return ran;
}

}
