#include <arisa/provider/standard/std_threading.hpp>

#include <arisa/provider/diagnostics/thread_cpu_meter.hpp>

#include <atomic>
#include <cstddef>
#include <expected>
#include <functional>
#include <latch>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <pthread.h>

namespace arisa::provider::standard {

class StdThreadingProvider::Worker {
  struct Start;

 public:
  static auto launch(std::string name, ThreadPlacement placement,
                     ThreadSetup setup, ThreadBody body)
      -> std::expected<std::unique_ptr<Worker>, ProviderError> {
    auto start = std::make_shared<Start>(std::move(name), placement,
                                         std::move(setup), std::move(body));
    auto worker = std::make_unique<Worker>(Key{}, start);
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    const int sized =
        pthread_attr_setstacksize(&attributes, placement.stack().value());
    auto handed = std::make_unique<std::shared_ptr<Start>>(start);
    const int created = sized == 0
                            ? pthread_create(&worker->handle_, &attributes,
                                             &Worker::run, handed.get())
                            : sized;
    pthread_attr_destroy(&attributes);
    if (created != 0) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    static_cast<void>(handed.release());
    worker->joinable_ = true;
    start->ready.wait();
    worker->id_ = ThreadId{start->id.load(std::memory_order_acquire)};
    return worker;
  }

  struct Key {
    explicit Key() = default;
  };

  Worker(Key, std::shared_ptr<Start> start) noexcept
      : start_(std::move(start)) {}

  Worker(const Worker&) = delete;
  auto operator=(const Worker&) -> Worker& = delete;
  Worker(Worker&&) = delete;
  auto operator=(Worker&&) -> Worker& = delete;

  ~Worker() {
    request_stop();
    if (joinable_) {
      pthread_join(handle_, nullptr);
    }
  }

  void request_stop() noexcept { start_->stop.request_stop(); }

  auto id() const noexcept -> ThreadId { return id_; }

  auto finished() const noexcept -> bool {
    return start_->finished.load(std::memory_order_acquire);
  }

  auto running_here() const noexcept -> bool {
    return joinable_ && pthread_equal(handle_, pthread_self()) != 0;
  }

  static void abandon(std::unique_ptr<Worker> worker) noexcept {
    if (worker->joinable_) {
      pthread_detach(worker->handle_);
      worker->joinable_ = false;
    }
  }

 private:
  struct Start {
    Start(std::string thread_name, ThreadPlacement thread_placement,
          ThreadSetup thread_setup, ThreadBody thread_body)
        : name(std::move(thread_name)),
          placement(thread_placement),
          setup(std::move(thread_setup)),
          body(std::move(thread_body)) {}

    std::string name;
    ThreadPlacement placement;
    ThreadSetup setup;
    ThreadBody body;
    std::stop_source stop;
    std::atomic<bool> finished{false};
    std::atomic<std::size_t> id{0};
    std::latch ready{1};
  };

  static auto run(void* argument) -> void* {
    const std::unique_ptr<std::shared_ptr<Start>> handed{
        static_cast<std::shared_ptr<Start>*>(argument)};
    Start& start = **handed;
    start.id.store(
        StdThreadingProvider::identify(std::this_thread::get_id()).value(),
        std::memory_order_release);
    start.ready.count_down();
    if (start.setup) {
      start.setup(start.name, start.placement);
    }
    const ThreadCpuMeter::Enrolment enrolled =
        ThreadCpuMeter::enrol(start.name);
    start.body(start.stop.get_token());
    start.finished.store(true, std::memory_order_release);
    return nullptr;
  }

  std::shared_ptr<Start> start_;
  pthread_t handle_{};
  bool joinable_{false};
  ThreadId id_{0};
};

class StdThreadingProvider::JoiningThread final
    : public Thread::Implementation {
 public:
  JoiningThread(std::unique_ptr<Worker> worker,
                std::shared_ptr<ThreadOrphanage<Worker>> orphans) noexcept
      : worker_(std::move(worker)), orphans_(std::move(orphans)) {}

  JoiningThread(const JoiningThread&) = delete;
  auto operator=(const JoiningThread&) -> JoiningThread& = delete;
  JoiningThread(JoiningThread&&) = delete;
  auto operator=(JoiningThread&&) -> JoiningThread& = delete;

  ~JoiningThread() override {
    worker_->request_stop();
    if (worker_->running_here()) {
      orphans_->adopt(std::move(worker_));
    }
  }

  void request_stop() noexcept override { worker_->request_stop(); }

  auto id() const noexcept -> ThreadId override { return worker_->id(); }

 private:
  std::unique_ptr<Worker> worker_;
  std::shared_ptr<ThreadOrphanage<Worker>> orphans_;
};

StdThreadingProvider::StdThreadingProvider(ThreadPlacementPolicy policy,
                                           ThreadSetup setup)
    : policy_(std::move(policy)),
      setup_(std::move(setup)),
      orphans_(std::make_shared<ThreadOrphanage<Worker>>()) {}

StdThreadingProvider::~StdThreadingProvider() {
  if (orphans_ != nullptr) {
    orphans_->join_all();
  }
}

auto StdThreadingProvider::start(ThreadSpec spec, ThreadBody body)
    -> std::expected<Thread, ProviderError> {
  if (spec.name.empty() || !body) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  orphans_->reap();
  auto worker =
      Worker::launch(std::move(spec.name), policy_.placement(spec.role), setup_,
                     std::move(body));
  if (!worker.has_value()) {
    return std::unexpected(worker.error());
  }
  return Thread{std::make_unique<JoiningThread>(std::move(*worker), orphans_)};
}

auto StdThreadingProvider::placement() const noexcept
    -> const ThreadPlacementPolicy& {
  return policy_;
}

auto StdThreadingProvider::current_thread() const noexcept -> ThreadId {
  return identify(std::this_thread::get_id());
}

void StdThreadingProvider::adopt_current_thread(ThreadRole role) noexcept {
  const std::string_view name =
      role == ThreadRole::ui ? "arisa.ui" : "arisa.adopted";
  if (setup_) {
    setup_(name, policy_.placement(role));
  }
  adopted_ = ThreadCpuMeter::enrol(name);
}

auto StdThreadingProvider::orphans() const -> std::size_t {
  return orphans_->size();
}

auto StdThreadingProvider::identify(std::thread::id id) noexcept -> ThreadId {
  return ThreadId{std::hash<std::thread::id>{}(id)};
}

void StdThreadingProvider::name_current_thread(
    std::string_view name, const ThreadPlacement& placement) noexcept {
  static_cast<void>(placement);
  static constexpr std::size_t name_limit = 15;
  const std::string truncated{name.substr(0, name_limit)};
#if defined(__APPLE__)
  pthread_setname_np(truncated.c_str());
#elif defined(__linux__)
  pthread_setname_np(pthread_self(), truncated.c_str());
#endif
}

}
