#pragma once

#include <arisa/async/job.hpp>
#include <arisa/async/job_priority.hpp>

#include <utility>

namespace arisa::async {

class Executor {
 public:
  virtual ~Executor() = default;

  auto submit(Work work, JobPriority priority = JobPriority::normal) -> Job {
    return schedule(std::move(work), priority);
  }

 private:
  virtual auto schedule(Work work, JobPriority priority) -> Job = 0;
};

}
