#include <arisa/async/job.hpp>

#include <atomic>
#include <cstdint>

namespace arisa::async {

class Job::State {
 public:
  explicit State(Work work) noexcept : work_(std::move(work)) {}

  void run() {
    Phase expected = Phase::pending;
    if (!phase_.compare_exchange_strong(expected, Phase::running,
                                        std::memory_order_acq_rel)) {
      return;
    }
    work_(stop_.get_token());
    work_ = nullptr;
    phase_.store(Phase::finished, std::memory_order_release);
  }

  void cancel() noexcept {
    stop_.request_stop();
    Phase expected = Phase::pending;
    if (phase_.compare_exchange_strong(expected, Phase::dropped,
                                       std::memory_order_acq_rel)) {
      work_ = nullptr;
    }
  }

  auto started() const noexcept -> bool {
    const Phase current = phase_.load(std::memory_order_acquire);
    return current == Phase::running || current == Phase::finished;
  }

  auto finished() const noexcept -> bool {
    return phase_.load(std::memory_order_acquire) == Phase::finished;
  }

  auto dropped() const noexcept -> bool {
    return phase_.load(std::memory_order_acquire) == Phase::dropped;
  }

 private:
  enum class Phase : std::uint8_t {
    pending,
    running,
    finished,
    dropped,
  };

  std::atomic<Phase> phase_{Phase::pending};
  std::stop_source stop_;
  Work work_;
};

auto Job::create(Work work) -> std::pair<Job, Ticket> {
  auto state = std::make_shared<State>(std::move(work));
  return {Job{state}, Ticket{state}};
}

Job::Job(std::shared_ptr<State> state) noexcept : state_(std::move(state)) {}

Job::Job(Job&& other) noexcept = default;

auto Job::operator=(Job&& other) noexcept -> Job& {
  if (this != &other) {
    cancel();
    state_ = std::move(other.state_);
  }
  return *this;
}

Job::~Job() { cancel(); }

void Job::cancel() noexcept {
  if (state_ != nullptr) {
    state_->cancel();
  }
}

auto Job::started() const noexcept -> bool {
  return state_ != nullptr && state_->started();
}

auto Job::finished() const noexcept -> bool {
  return state_ != nullptr && state_->finished();
}

auto Job::dropped() const noexcept -> bool {
  return state_ == nullptr || state_->dropped();
}

Job::Ticket::Ticket(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}

void Job::Ticket::run() {
  if (const std::shared_ptr<State> state = std::exchange(state_, nullptr)) {
    state->run();
  }
}

auto Job::Ticket::dropped() const noexcept -> bool {
  return state_ == nullptr || state_->dropped();
}

}
