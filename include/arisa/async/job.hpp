#pragma once

#include <functional>
#include <memory>
#include <stop_token>
#include <utility>

namespace arisa::async {

using Work = std::move_only_function<void(std::stop_token)>;

class [[nodiscard]] Job {
 public:
  class Ticket;

  static auto create(Work work) -> std::pair<Job, Ticket>;

  Job(const Job&) = delete;
  auto operator=(const Job&) -> Job& = delete;
  Job(Job&& other) noexcept;
  auto operator=(Job&& other) noexcept -> Job&;
  ~Job();

  void cancel() noexcept;

  auto started() const noexcept -> bool;

  auto finished() const noexcept -> bool;

  auto dropped() const noexcept -> bool;

 private:
  class State;

  explicit Job(std::shared_ptr<State> state) noexcept;

  std::shared_ptr<State> state_;
};

class Job::Ticket {
 public:
  void run();

  auto dropped() const noexcept -> bool;

 private:
  friend class Job;

  explicit Ticket(std::shared_ptr<State> state) noexcept;

  std::shared_ptr<State> state_;
};

}
