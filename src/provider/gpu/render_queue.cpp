#include <arisa/provider/gpu/render_queue.hpp>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace arisa::provider {

class RenderQueue::State : public std::enable_shared_from_this<State> {
 public:
  enum class Holder : std::uint8_t { app, park, renderer };

  auto take(Holder holder) -> Lease {
    std::unique_lock lock{mutex_};
    if (!free_for(holder)) {
      ++counters_.lease_waits;
      changed_.wait(lock, [this, holder] { return free_for(holder); });
    }
    return grant(holder);
  }

  auto submission() -> std::optional<Lease> {
    std::unique_lock lock{mutex_};
    if (!free_for(Holder::renderer)) {
      if (withdrawn_ > 0) {
        return std::nullopt;
      }
      ++counters_.submission_waits;
      changed_.wait(lock, [this] {
        return withdrawn_ > 0 || free_for(Holder::renderer);
      });
      if (!free_for(Holder::renderer)) {
        return std::nullopt;
      }
    }
    return grant(Holder::renderer);
  }

  void withdraw(bool refusing) noexcept {
    {
      const std::scoped_lock lock{mutex_};
      if (refusing) {
        ++withdrawn_;
      } else {
        --withdrawn_;
      }
    }
    changed_.notify_all();
  }

  auto attempt(Holder holder) -> std::optional<Lease> {
    const std::scoped_lock lock{mutex_};
    if (!free_for(holder)) {
      return std::nullopt;
    }
    return grant(holder);
  }

  auto parked() const -> bool {
    const std::scoped_lock lock{mutex_};
    return parked_ > 0;
  }

  auto counters() const -> Counters {
    const std::scoped_lock lock{mutex_};
    return counters_;
  }

 private:
  auto free_for(Holder holder) const noexcept -> bool {
    switch (holder) {
      case Holder::app:
        return !app_ && !renderer_;
      case Holder::park:
        return !renderer_;
      case Holder::renderer:
        return !app_ && !renderer_ && parked_ == 0;
    }
    return false;
  }

  auto grant(Holder holder) -> Lease {
    switch (holder) {
      case Holder::app:
        app_ = true;
        ++counters_.leases;
        break;
      case Holder::park:
        ++parked_;
        ++counters_.parks;
        break;
      case Holder::renderer:
        renderer_ = true;
        ++counters_.submissions;
        break;
    }
    if (app_ && renderer_) {
      ++counters_.overlaps;
    }
    return Lease{shared_from_this(), static_cast<std::uint8_t>(holder)};
  }

 public:
  void give_back(Holder holder) noexcept {
    {
      const std::scoped_lock lock{mutex_};
      switch (holder) {
        case Holder::app:
          app_ = false;
          break;
        case Holder::park:
          --parked_;
          break;
        case Holder::renderer:
          renderer_ = false;
          break;
      }
    }
    changed_.notify_all();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  bool app_{false};
  bool renderer_{false};
  std::uint32_t parked_{0};
  std::uint32_t withdrawn_{0};
  Counters counters_{};
};

RenderQueue::Lease::Lease(Lease&& other) noexcept
    : state_(std::move(other.state_)), holder_(other.holder_) {}

auto RenderQueue::Lease::operator=(Lease&& other) noexcept -> Lease& {
  if (this != &other) {
    release();
    state_ = std::move(other.state_);
    holder_ = other.holder_;
  }
  return *this;
}

RenderQueue::Lease::~Lease() { release(); }

void RenderQueue::Lease::release() noexcept {
  if (const std::shared_ptr<State> state = std::exchange(state_, nullptr)) {
    state->give_back(static_cast<State::Holder>(holder_));
  }
}

RenderQueue::RenderQueue() : state_(std::make_shared<State>()) {}

auto RenderQueue::lease() -> Lease { return state_->take(State::Holder::app); }

auto RenderQueue::try_lease() -> std::optional<Lease> {
  return state_->attempt(State::Holder::app);
}

auto RenderQueue::park() -> Lease { return state_->take(State::Holder::park); }

auto RenderQueue::submit() -> std::optional<Lease> {
  return state_->submission();
}

auto RenderQueue::withdraw() -> support::ScopedCallback {
  state_->withdraw(true);
  return support::ScopedCallback{
      [state = state_]() noexcept { state->withdraw(false); }};
}

auto RenderQueue::parked() const -> bool { return state_->parked(); }

auto RenderQueue::counters() const -> Counters { return state_->counters(); }

}
