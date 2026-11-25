#include <runtime/frame_pacer.hpp>
#include <runtime/render_thread.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::runtime {

class RenderThread::State {
 public:
  State(provider::GpuProvider& gpu, provider::ClockProvider& clock) noexcept
      : gpu_(gpu), clock_(clock), stepped_(clock.stepped()) {}

  auto scene() -> rendering::ReplayScene& {
    const std::scoped_lock lock{mutex_};
    if (!building_.has_value()) {
      for (std::size_t index = 0; index < scene_count; ++index) {
        if (index != current_ && index != pending_scene_ &&
            index != replaying_ && index != delivering_) {
          building_ = index;
          break;
        }
      }
    }
    return scenes_.at(building_.value_or(0));
  }

  auto replayed_since() noexcept -> bool {
    return replayed_since_.exchange(false, std::memory_order_acq_rel);
  }

  auto submit(provider::FramePacket packet, std::chrono::nanoseconds presented)
      -> std::expected<Handoff, ApplicationError> {
    std::optional<std::size_t> made;
    {
      const std::scoped_lock lock{mutex_};
      made = std::exchange(building_, std::nullopt);
    }
    bool live = false;
    std::chrono::nanoseconds period = display_period;
    if (made.has_value()) {
      const rendering::ReplayScene& scene = scenes_.at(*made);
      live = scene.replayable && scene.generation != 0 &&
             !scene.bindings.empty() && !settled(scene, presented);
      period = period_of(scene);
    }
    const provider::ClockProvider& clock = clock_;
    const auto started = clock.now();
    std::uint64_t ticket = 0;
    {
      std::unique_lock lock{mutex_};
      space_.wait(lock, [this] { return !pending_.has_value(); });
      shown_ = presented;
      last_replay_.reset();
      period_ = period;
      live_ = live;
      pending_.emplace(std::move(packet));
      pending_scene_ = made;
      ticket = ++queued_;
    }
    wake_.notify_all();
    Handoff handoff;
    std::optional<ApplicationError> failed;
    {
      std::unique_lock lock{mutex_};
      if (stepped_) {
        while (!done_.wait_for(
            lock, park_poll, [this, ticket] { return processed_ >= ticket; })) {
          if (parked()) {
            break;
          }
        }
      }
      handoff.receipt = receipt_;
      failed = std::exchange(error_, std::nullopt);
    }
    handoff.stall = clock.now() - started;
    if (failed.has_value()) {
      return std::unexpected(*failed);
    }
    return handoff;
  }

  void abandon() noexcept {
    const std::scoped_lock lock{mutex_};
    building_.reset();
  }

  void step() {
    std::unique_lock lock{mutex_};
    const std::uint64_t wanted = ++requested_;
    wake_.notify_all();
    done_.wait(lock, [this, wanted] { return ticked_ >= wanted; });
  }

  void drain() {
    std::unique_lock lock{mutex_};
    done_.wait(lock, [this] { return processed_ >= queued_; });
  }

  auto take_capture() -> std::optional<CapturedFrame> {
    const std::scoped_lock lock{mutex_};
    return std::exchange(captured_, std::nullopt);
  }

  auto replayed() const noexcept -> std::uint64_t {
    return replays_.load(std::memory_order_relaxed);
  }

  auto wakes() const noexcept -> std::uint64_t {
    return wakes_.load(std::memory_order_relaxed);
  }

  auto submitted() const noexcept -> std::uint64_t {
    return submissions_.load(std::memory_order_relaxed);
  }

  auto lockstep() const noexcept -> bool { return stepped_; }

  void run(const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    while (true) {
      if (pending_.has_value()) {
        provider::FramePacket packet = std::move(*pending_);
        const std::optional<std::size_t> scene =
            std::exchange(pending_scene_, std::nullopt);
        delivering_ = scene;
        pending_.reset();
        space_.notify_all();
        lock.unlock();
        deliver(std::move(packet), scene);
        lock.lock();
        delivering_.reset();
        ++processed_;
        done_.notify_all();
        continue;
      }
      if (stop.stop_requested()) {
        break;
      }
      if (stepped_) {
        if (!wake_.wait(lock, stop, [this] {
              return pending_.has_value() || requested_ != ticked_;
            })) {
          continue;
        }
        if (pending_.has_value()) {
          continue;
        }
        const std::uint64_t wanted = requested_;
        lock.unlock();
        static_cast<void>(tick(clock_.get().now().since_origin()));
        lock.lock();
        ticked_ = wanted;
        done_.notify_all();
        continue;
      }
      if (!live_ || !shown_.has_value()) {
        idle_ = true;
        const bool woken = wake_.wait(
            lock, stop, [this] { return live_ || pending_.has_value(); });
        idle_ = false;
        if (woken && !pending_.has_value()) {
          wakes_.fetch_add(1, std::memory_order_relaxed);
        }
        continue;
      }
      const std::chrono::nanoseconds deadline = due_at();
      const std::chrono::nanoseconds remaining =
          deadline - clock_.get().now().since_origin();
      if (remaining > std::chrono::nanoseconds{0}) {
        static_cast<void>(wake_.wait_for(lock, stop, remaining, [this] {
          return !live_ || pending_.has_value();
        }));
        if (!pending_.has_value() && !stop.stop_requested()) {
          wakes_.fetch_add(1, std::memory_order_relaxed);
        }
        continue;
      }
      lock.unlock();
      const bool ticked = tick(clock_.get().now().since_origin());
      lock.lock();
      if (!ticked) {
        static_cast<void>(wake_.wait_for(
            lock, stop, slack, [this] { return pending_.has_value(); }));
      }
    }
  }

 private:
  struct Effect {
    std::array<float, 2> shift{};
    float alpha{1.0F};
    std::optional<std::array<float, 2>> trim{};
  };

  static auto interval_of(const rendering::ReplayScene& scene) noexcept
      -> std::uint32_t {
    return FramePacer::swap_interval(scene.frame_rate, scene.driven);
  }

  static auto period_of(const rendering::ReplayScene& scene)
      -> std::chrono::nanoseconds {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(
            1.0 / std::max(scene.frame_rate.value_or(default_rate), 1.0)));
  }

  auto due_at() const -> std::chrono::nanoseconds {
    if (last_replay_.has_value()) {
      return *last_replay_ + period_ - slack;
    }
    return shown_.value_or(std::chrono::nanoseconds{0}) + (2 * period_) - slack;
  }

  void deliver(provider::FramePacket packet, std::optional<std::size_t> scene) {
    const std::uint64_t frame = packet.frame();
    const bool capturing = packet.plan().capture;
    auto receipt = packet.submit(std::cref(clock_.get()));
    submissions_.fetch_add(1, std::memory_order_relaxed);
    const std::scoped_lock lock{mutex_};
    if (!receipt.has_value()) {
      if (receipt.error().detail() != provider::ProviderErrorDetail::taken_over) {
        error_ = ApplicationError{ApplicationErrorCode::presentation_failed,
                                  receipt.error()};
      }
      return;
    }
    if (scene.has_value()) {
      current_ = scene;
    }
    if (capturing && receipt->image.has_value()) {
      captured_.emplace(
          CapturedFrame{.frame = frame, .image = std::move(*receipt->image)});
      receipt->image.reset();
    }
    receipt_ = std::move(*receipt);
  }

  auto tick(std::chrono::nanoseconds now) -> bool {
    std::optional<std::size_t> picked;
    std::optional<std::chrono::nanoseconds> shown;
    std::optional<std::chrono::nanoseconds> last;
    {
      const std::scoped_lock lock{mutex_};
      if (pending_.has_value()) {
        return true;
      }
      if (parked()) {
        live_ = false;
        return true;
      }
      picked = current_;
      replaying_ = current_;
      shown = shown_;
      last = last_replay_;
    }
    std::optional<provider::FramePacket> built;
    bool live = false;
    if (picked.has_value() && shown.has_value()) {
      const rendering::ReplayScene& current = scenes_.at(*picked);
      if (current.replayable) {
        live = !settled(current, last.value_or(*shown));
        if (live && due(current, now, *shown, last)) {
          built = build(current, now);
          live = built.has_value() && !settled(current, now);
        }
      }
    }
    {
      const std::scoped_lock lock{mutex_};
      replaying_.reset();
    }
    bool replayed = false;
    if (built.has_value()) {
      replayed_since_.store(true, std::memory_order_release);
      replayed = built->submit(std::cref(clock_.get())).has_value();
      submissions_.fetch_add(1, std::memory_order_relaxed);
      built.reset();
      if (replayed) {
        replays_.fetch_add(1, std::memory_order_relaxed);
      } else {
        live = false;
      }
    }
    {
      const std::scoped_lock lock{mutex_};
      if (shown_ == shown) {
        if (replayed) {
          last_replay_ = now;
        }
        live_ = live;
      }
    }
    return true;
  }

  auto parked() -> bool {
    const auto queue = gpu_.get().render_queue();
    return queue.has_value() && queue->get().parked();
  }

  static auto due(const rendering::ReplayScene& scene,
                  std::chrono::nanoseconds now, std::chrono::nanoseconds shown,
                  std::optional<std::chrono::nanoseconds> last) -> bool {
    const std::chrono::nanoseconds period = period_of(scene);
    if (last.has_value()) {
      return now - *last >= period - slack;
    }
    return now - shown >= (2 * period) - slack;
  }

  static auto settled(const rendering::ReplayScene& scene,
                      std::chrono::nanoseconds at) -> bool {
    const auto finished =
        [at](const std::optional<motion::AnimationTrack>& track) {
          return !track.has_value() || track->sample(at).finished;
        };
    return std::ranges::all_of(
        scene.bindings, [&finished](const rendering::ReplayBinding& binding) {
          return finished(binding.offset) && finished(binding.opacity) &&
                 finished(binding.trim_start) && finished(binding.trim_end);
        });
  }

  auto build(const rendering::ReplayScene& scene, std::chrono::nanoseconds now)
      -> std::optional<provider::FramePacket> {
    if (scene.generation == 0) {
      return std::nullopt;
    }
    patch(scene, now);
    auto frame = gpu_.get().begin_replay();
    if (!frame.has_value()) {
      return std::nullopt;
    }
    frame->plan(provider::FramePlan{.predicted_present = now + display_period,
                                    .replay = true,
                                    .exclusive = false,
                                    .extent = {},
                                    .swap_interval = interval_of(scene),
                                    .capture = false});
    frame->clear(scene.clear);
    if (!frame
             ->replay(
                 provider::ReplayDrawData{.indices = scene.indices,
                                          .commands = patched_,
                                          .vertex_count = scene.vertex_count,
                                          .generation = scene.generation})
             .has_value()) {
      return std::nullopt;
    }
    auto sealed = frame->seal();
    if (!sealed.has_value()) {
      return std::nullopt;
    }
    return std::move(*sealed);
  }

  void patch(const rendering::ReplayScene& scene,
             std::chrono::nanoseconds now) {
    effects_.clear();
    for (const rendering::ReplayBinding& binding : scene.bindings) {
      Effect effect = binding.parent != 0 && binding.parent <= effects_.size()
                          ? effects_[binding.parent - 1]
                          : Effect{};
      effect.trim.reset();
      if (binding.offset.has_value()) {
        const motion::AnimatableVector moved =
            binding.offset->sample(now).value;
        const auto dx =
            static_cast<float>(moved[0] - binding.painted_offset[0]) *
            binding.scale;
        const auto dy =
            static_cast<float>(moved[1] - binding.painted_offset[1]) *
            binding.scale;
        effect.shift[0] += (binding.linear[0] * dx) + (binding.linear[2] * dy);
        effect.shift[1] += (binding.linear[1] * dx) + (binding.linear[3] * dy);
      }
      if (binding.opacity.has_value()) {
        const double sampled =
            std::clamp(binding.opacity->sample(now).value[0], 0.0, 1.0);
        effect.alpha *= static_cast<float>(
            sampled / std::clamp(binding.painted_opacity, 1.0e-3, 1.0));
      }
      if (binding.trims) {
        const double from = binding.trim_start.has_value()
                                ? binding.trim_start->sample(now).value[0]
                                : binding.painted_trim_start;
        const double to = binding.trim_end.has_value()
                              ? binding.trim_end->sample(now).value[0]
                              : binding.painted_trim_end;
        std::array<float, 2> window = provider::DrawCommand::untrimmed;
        if (from > 0.0) {
          window[0] = static_cast<float>(std::clamp(from, 0.0, 1.0)) *
                      binding.trim_length;
        }
        if (to < 1.0) {
          window[1] = static_cast<float>(std::clamp(to, 0.0, 1.0)) *
                      binding.trim_length;
        }
        effect.trim = window;
      }
      effects_.push_back(effect);
    }
    patched_.assign(scene.commands.begin(), scene.commands.end());
    for (std::size_t index = 0;
         index < patched_.size() && index < scene.owners.size(); ++index) {
      const std::uint32_t owner = scene.owners[index];
      if (owner == 0 || owner > effects_.size()) {
        continue;
      }
      const Effect& effect = effects_[owner - 1];
      provider::DrawCommand& command = patched_[index];
      command.translation[0] += effect.shift[0];
      command.translation[1] += effect.shift[1];
      command.opacity *= effect.alpha;
      if (effect.trim.has_value()) {
        command.trim = *effect.trim;
      }
    }
  }

  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::reference_wrapper<provider::ClockProvider> clock_;
  bool stepped_;
  std::array<rendering::ReplayScene, scene_count> scenes_{};
  std::atomic<bool> replayed_since_{false};
  std::atomic<std::uint64_t> replays_{0};
  std::atomic<std::uint64_t> wakes_{0};
  std::atomic<std::uint64_t> submissions_{0};
  std::vector<Effect> effects_;
  std::vector<provider::DrawCommand> patched_;
  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::condition_variable_any space_;
  std::condition_variable_any done_;
  std::optional<provider::FramePacket> pending_;
  std::optional<std::size_t> pending_scene_;
  std::optional<std::size_t> delivering_;
  std::optional<std::size_t> current_;
  std::optional<std::size_t> replaying_;
  std::optional<std::size_t> building_;
  std::optional<CapturedFrame> captured_;
  std::optional<provider::FrameReceipt> receipt_;
  std::optional<ApplicationError> error_;
  std::optional<std::chrono::nanoseconds> shown_;
  std::optional<std::chrono::nanoseconds> last_replay_;
  std::chrono::nanoseconds period_{0};
  bool live_{false};
  bool idle_{false};
  std::uint64_t requested_{0};
  std::uint64_t ticked_{0};
  std::uint64_t queued_{0};
  std::uint64_t processed_{0};
};

RenderThread::RenderThread(std::shared_ptr<State> state,
                           provider::Thread thread) noexcept
    : state_(std::move(state)), thread_(std::move(thread)) {}

auto RenderThread::create(provider::ThreadingProvider& threading,
                          provider::GpuProvider& gpu,
                          provider::ClockProvider& clock)
    -> std::expected<RenderThread, ApplicationError> {
  auto state = std::make_shared<State>(gpu, clock);
  auto started = threading.start(
      provider::ThreadSpec{.name = "arisa-render",
                           .role = provider::ThreadRole::render},
      [state](const std::stop_token& stop) { state->run(stop); });
  if (!started.has_value()) {
    return std::unexpected(ApplicationError{
        ApplicationErrorCode::executors_unavailable, started.error()});
  }
  return RenderThread{std::move(state), std::move(*started)};
}

auto RenderThread::scene() -> rendering::ReplayScene& {
  return state_->scene();
}

auto RenderThread::replayed_since() noexcept -> bool {
  return state_->replayed_since();
}

auto RenderThread::submit(provider::FramePacket packet,
                          std::chrono::nanoseconds presented)
    -> std::expected<Handoff, ApplicationError> {
  return state_->submit(std::move(packet), presented);
}

void RenderThread::abandon() noexcept { state_->abandon(); }

void RenderThread::step() { state_->step(); }

void RenderThread::drain() { state_->drain(); }

auto RenderThread::take_capture() -> std::optional<CapturedFrame> {
  return state_->take_capture();
}

auto RenderThread::replayed() const noexcept -> std::uint64_t {
  return state_->replayed();
}

auto RenderThread::wakes() const noexcept -> std::uint64_t {
  return state_->wakes();
}

auto RenderThread::submitted() const noexcept -> std::uint64_t {
  return state_->submitted();
}

auto RenderThread::lockstep() const noexcept -> bool {
  return state_->lockstep();
}

}
