#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/job_priority.hpp>

#include <functional>
#include <optional>
#include <string>

namespace arisa::composition {

struct TaskOptions {
  std::optional<std::string> name{};
  async::JobPriority priority{async::JobPriority::normal};
  std::optional<std::reference_wrapper<async::Executor>> executor_preference{};
};

}
