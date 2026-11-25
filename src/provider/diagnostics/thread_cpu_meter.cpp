#include <arisa/provider/diagnostics/thread_cpu_meter.hpp>

#if defined(__SWITCH__)
#include <switch.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <pthread.h>
#elif defined(__linux__)
#include <pthread.h>
#include <time.h>
#endif

#include <algorithm>
#include <utility>

namespace arisa::provider {

auto ThreadCpuMeter::registry() -> Registry& {
  static Registry shared;
  return shared;
}

auto ThreadCpuMeter::measuring() noexcept -> bool {
#if defined(__SWITCH__) || defined(__APPLE__) || defined(__linux__)
  return true;
#else
  return false;
#endif
}

auto ThreadCpuMeter::enrol(std::string_view name) -> Enrolment {
  if (!measuring()) {
    return Enrolment{};
  }
  Registry& shared = registry();
  const std::scoped_lock lock{shared.mutex};
  const std::uint64_t id = shared.next++;
  shared.entries.push_back(
      Entry{.id = id, .name = std::string{name}, .native = current_native()});
  return Enrolment{id};
}

void ThreadCpuMeter::withdraw(std::uint64_t id) noexcept {
  Registry& shared = registry();
  const std::scoped_lock lock{shared.mutex};
  std::erase_if(shared.entries,
                [id](const Entry& entry) { return entry.id == id; });
}

auto ThreadCpuMeter::sample() -> std::vector<ThreadCpu> {
  Registry& shared = registry();
  const std::scoped_lock lock{shared.mutex};
  std::vector<ThreadCpu> taken;
  taken.reserve(shared.entries.size());
  for (const Entry& entry : shared.entries) {
    taken.push_back(ThreadCpu{
        .id = entry.id, .name = entry.name, .cpu = read(entry.native)});
  }
  return taken;
}

auto ThreadCpuMeter::since(const std::vector<ThreadCpu>& before,
                           const std::vector<ThreadCpu>& after)
    -> std::vector<ThreadCpu> {
  std::vector<ThreadCpu> spent;
  spent.reserve(after.size());
  for (const ThreadCpu& now : after) {
    const auto earlier = std::ranges::find(before, now.id, &ThreadCpu::id);
    const std::chrono::nanoseconds start =
        earlier == before.end() ? std::chrono::nanoseconds{0} : earlier->cpu;
    spent.push_back(ThreadCpu{
        .id = now.id,
        .name = now.name,
        .cpu = std::max(now.cpu - start, std::chrono::nanoseconds{0})});
  }
  return spent;
}

auto ThreadCpuMeter::current_native() noexcept -> std::uint64_t {
#if defined(__SWITCH__)
  return static_cast<std::uint64_t>(threadGetCurHandle());
#elif defined(__APPLE__)
  return static_cast<std::uint64_t>(pthread_mach_thread_np(pthread_self()));
#elif defined(__linux__)
  clockid_t clock{};
  if (pthread_getcpuclockid(pthread_self(), &clock) != 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(static_cast<std::int64_t>(clock));
#else
  return 0;
#endif
}

auto ThreadCpuMeter::read(std::uint64_t native) noexcept
    -> std::chrono::nanoseconds {
#if defined(__SWITCH__)
  u64 ticks = 0;
  if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount,
                          static_cast<Handle>(native), UINT64_MAX))) {
    return std::chrono::nanoseconds{0};
  }
  return std::chrono::nanoseconds{
      static_cast<std::chrono::nanoseconds::rep>(armTicksToNs(ticks))};
#elif defined(__APPLE__)
  thread_basic_info_data_t info{};
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  if (thread_info(static_cast<thread_act_t>(native), THREAD_BASIC_INFO,
                  reinterpret_cast<thread_info_t>(&info),
                  &count) != KERN_SUCCESS) {
    return std::chrono::nanoseconds{0};
  }
  const auto seconds = static_cast<std::int64_t>(info.user_time.seconds) +
                       static_cast<std::int64_t>(info.system_time.seconds);
  const auto micros = static_cast<std::int64_t>(info.user_time.microseconds) +
                      static_cast<std::int64_t>(info.system_time.microseconds);
  return std::chrono::seconds{seconds} + std::chrono::microseconds{micros};
#elif defined(__linux__)
  timespec spent{};
  if (clock_gettime(static_cast<clockid_t>(static_cast<std::int64_t>(native)),
                    &spent) != 0) {
    return std::chrono::nanoseconds{0};
  }
  return std::chrono::seconds{spent.tv_sec} +
         std::chrono::nanoseconds{spent.tv_nsec};
#else
  static_cast<void>(native);
  return std::chrono::nanoseconds{0};
#endif
}

}
