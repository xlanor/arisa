#pragma once

#include <arisa/async/executor.hpp>

#include <functional>
#include <memory>

namespace arisa::runtime {

class DispatchQueue;

class Dispatcher final : public async::Executor {
 public:
  using Task = std::move_only_function<void()>;

  explicit Dispatcher(std::shared_ptr<DispatchQueue> queue) noexcept;

  auto post(Task task) const -> bool;

  friend auto operator==(const Dispatcher& left,
                         const Dispatcher& right) noexcept -> bool {
    return left.queue_ == right.queue_;
  }

 private:
  auto schedule(async::Work work, async::JobPriority priority)
      -> async::Job override;

  std::shared_ptr<DispatchQueue> queue_;
};

}
