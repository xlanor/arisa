#include <arisa/provider/haptics/rumble_queue.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace arisa::provider {

class RumbleQueue::State {
 public:
  explicit State(std::unique_ptr<RumbleMotor> motor) noexcept
      : motor_(std::move(motor)) {}

  void request(const RumbleShape& shape) {
    {
      std::unique_lock lock{mutex_, std::try_to_lock};
      if (!lock.owns_lock()) {
        contended_.fetch_add(1, std::memory_order_relaxed);
        lock.lock();
      }
      pending_ = shape;
    }
    requested_.fetch_add(1, std::memory_order_relaxed);
    wake_.notify_one();
  }

  auto counters() const noexcept -> Counters {
    return Counters{
        .requested = requested_.load(std::memory_order_relaxed),
        .contended = contended_.load(std::memory_order_relaxed),
        .started = started_.load(std::memory_order_relaxed),
        .interrupted = interrupted_.load(std::memory_order_relaxed)};
  }

  void run(const std::stop_token& token) {
    while (!token.stop_requested()) {
      std::optional<RumbleShape> next = take(token);
      if (!next.has_value()) {
        break;
      }
      started_.fetch_add(1, std::memory_order_relaxed);
      motor_->prepare();
      if (perform(*next, token)) {
        motor_->vibrate(RumblePulse{});
      } else {
        interrupted_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    motor_->vibrate(RumblePulse{});
  }

 private:
  auto take(const std::stop_token& token) -> std::optional<RumbleShape> {
    std::unique_lock lock{mutex_};
    if (!wake_.wait(lock, token, [this] { return pending_.has_value(); })) {
      return std::nullopt;
    }
    return std::exchange(pending_, std::nullopt);
  }

  auto perform(const RumbleShape& shape, const std::stop_token& token) -> bool {
    for (const RumblePulse& pulse : shape.pulses()) {
      motor_->vibrate(pulse);
      std::unique_lock lock{mutex_};
      if (wake_.wait_for(lock, token, pulse.duration,
                         [this] { return pending_.has_value(); }) ||
          token.stop_requested()) {
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<RumbleMotor> motor_;
  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::optional<RumbleShape> pending_;
  std::atomic<std::uint64_t> requested_{0};
  std::atomic<std::uint64_t> contended_{0};
  std::atomic<std::uint64_t> started_{0};
  std::atomic<std::uint64_t> interrupted_{0};
};

RumbleQueue::RumbleQueue(std::unique_ptr<State> state, Thread thread) noexcept
    : state_(std::move(state)), thread_(std::move(thread)) {}

RumbleQueue::RumbleQueue(RumbleQueue&& other) noexcept = default;

auto RumbleQueue::operator=(RumbleQueue&& other) noexcept -> RumbleQueue& {
  thread_ = std::move(other.thread_);
  state_ = std::move(other.state_);
  return *this;
}

RumbleQueue::~RumbleQueue() = default;

auto RumbleQueue::create(ThreadingProvider& threading,
                         std::unique_ptr<RumbleMotor> motor)
    -> std::expected<RumbleQueue, ProviderError> {
  auto state = std::make_unique<State>(std::move(motor));
  auto thread = threading.start(
      ThreadSpec{.name = "arisa-haptics", .role = ThreadRole::background},
      [&driven = *state](const std::stop_token& token) { driven.run(token); });
  if (!thread.has_value()) {
    return std::unexpected(thread.error());
  }
  return RumbleQueue{std::move(state), std::move(*thread)};
}

void RumbleQueue::play(const RumbleShape& shape) {
  if (shape.silent()) {
    return;
  }
  state_->request(shape);
}

auto RumbleQueue::counters() const noexcept -> Counters {
  return state_->counters();
}

}
