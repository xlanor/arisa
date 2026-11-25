#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace arisa::motion {

class CompletionQueue {
 public:
  using Completion = std::move_only_function<void()>;

  void push(Completion completion);

  auto drain() -> std::size_t;

  auto pending() const noexcept -> bool { return !waiting_.empty(); }

 private:
  std::vector<Completion> waiting_;
};

}
