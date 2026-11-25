#pragma once

#include <arisa/provider/diagnostics/thread_cpu_meter.hpp>
#include <arisa/provider/threading/provider.hpp>
#include <arisa/provider/threading/thread_orphanage.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

namespace arisa::provider::standard {

class StdThreadingProvider final : public ThreadingProvider {
 public:
  using ThreadSetup =
      std::function<void(std::string_view name, const ThreadPlacement&)>;

  explicit StdThreadingProvider(ThreadPlacementPolicy policy = {},
                                ThreadSetup setup = name_current_thread);

  StdThreadingProvider(const StdThreadingProvider&) = delete;
  auto operator=(const StdThreadingProvider&) -> StdThreadingProvider& = delete;
  StdThreadingProvider(StdThreadingProvider&& other) noexcept = default;
  auto operator=(StdThreadingProvider&& other) noexcept
      -> StdThreadingProvider& = default;
  ~StdThreadingProvider() override;

  [[nodiscard]] auto start(ThreadSpec spec, ThreadBody body)
      -> std::expected<Thread, ProviderError> override;

  auto placement() const noexcept -> const ThreadPlacementPolicy& override;

  auto current_thread() const noexcept -> ThreadId override;

  void adopt_current_thread(ThreadRole role) noexcept override;

  auto orphans() const -> std::size_t;

 private:
  class Worker;
  class JoiningThread;

  static auto identify(std::thread::id id) noexcept -> ThreadId;
  static void name_current_thread(std::string_view name,
                                  const ThreadPlacement& placement) noexcept;

  ThreadPlacementPolicy policy_;
  ThreadSetup setup_;
  std::shared_ptr<ThreadOrphanage<Worker>> orphans_;
  ThreadCpuMeter::Enrolment adopted_;
};

}
