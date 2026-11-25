#pragma once

#include <arisa/async/job.hpp>
#include <arisa/composition/context.hpp>

#include <chrono>
#include <memory>
#include <optional>

namespace arisa::components {

class TimedWake {
 public:
  using Date = std::chrono::system_clock::time_point;

  static auto now(composition::Context& context) -> Date;

  void schedule(composition::Context& context, Date at, Date current);

  void cancel() noexcept;

  auto due() const noexcept -> const std::optional<Date>& { return due_; }

 private:
  std::shared_ptr<async::Job> job_;
  std::optional<Date> due_;
};

}
