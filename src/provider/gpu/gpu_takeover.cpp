#include <arisa/provider/gpu/gpu_takeover.hpp>

#include <mutex>
#include <utility>

namespace arisa::provider {

class GpuTakeover::State {
 public:
  auto enter() -> bool {
    const std::scoped_lock lock{mutex_};
    if (held_) {
      ++counters_.refused;
      return false;
    }
    held_ = true;
    return true;
  }

  void granted() {
    const std::scoped_lock lock{mutex_};
    ++counters_.granted;
  }

  void leave(bool returned) noexcept {
    const std::scoped_lock lock{mutex_};
    held_ = false;
    if (returned) {
      ++counters_.returned;
    }
  }

  auto held() const -> bool {
    const std::scoped_lock lock{mutex_};
    return held_;
  }

  auto counters() const -> Counters {
    const std::scoped_lock lock{mutex_};
    return counters_;
  }

 private:
  mutable std::mutex mutex_;
  bool held_{false};
  Counters counters_{};
};

GpuTakeover::Seat::Seat() : state_(std::make_shared<State>()) {}

auto GpuTakeover::Seat::claim(RenderQueue& queue,
                              std::weak_ptr<FrameSchedule> wake,
                              HandBack prepare)
    -> std::expected<GpuTakeover, ProviderError> {
  if (!state_->enter()) {
    return std::unexpected(ProviderError{ProviderErrorCode::resource_exhausted,
                                         ProviderErrorDetail::taken_over});
  }
  RenderQueue::Lease park = queue.park();
  GpuTakeover taken{state_, std::move(park), queue.withdraw(),
                    std::move(wake)};
  if (prepare) {
    taken.hand_back_ = prepare();
  }
  state_->granted();
  return taken;
}

auto GpuTakeover::Seat::held() const -> bool { return state_->held(); }

auto GpuTakeover::Seat::counters() const -> Counters {
  return state_->counters();
}

GpuTakeover::GpuTakeover(std::shared_ptr<State> state, RenderQueue::Lease park,
                         support::ScopedCallback refusing,
                         std::weak_ptr<FrameSchedule> wake) noexcept
    : state_(std::move(state)),
      park_(std::move(park)),
      refusing_(std::move(refusing)),
      wake_(std::move(wake)) {}

auto GpuTakeover::operator=(GpuTakeover&& other) noexcept -> GpuTakeover& {
  if (this != &other) {
    release();
    state_ = std::move(other.state_);
    park_ = std::move(other.park_);
    refusing_ = std::move(other.refusing_);
    wake_ = std::move(other.wake_);
    hand_back_ = std::move(other.hand_back_);
  }
  return *this;
}

GpuTakeover::~GpuTakeover() { release(); }

void GpuTakeover::release() noexcept {
  const std::shared_ptr<State> state = std::exchange(state_, nullptr);
  if (state == nullptr) {
    return;
  }
  hand_back_.reset();
  state->leave(true);
  park_.release();
  refusing_.reset();
  if (const std::shared_ptr<FrameSchedule> schedule = wake_.lock()) {
    schedule->request_frame();
  }
  wake_.reset();
}

}
