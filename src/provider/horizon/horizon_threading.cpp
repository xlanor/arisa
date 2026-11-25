#include <arisa/provider/horizon/horizon_threading.hpp>

#include <switch.h>

#include <bit>
#include <utility>

namespace arisa::provider::horizon {

HorizonThreadingProvider::HorizonThreadingProvider(ThreadPlacementPolicy policy)
    : threads_(std::move(policy), place_current_thread) {}

auto HorizonThreadingProvider::start(ThreadSpec spec, ThreadBody body)
    -> std::expected<Thread, ProviderError> {
  return threads_.start(std::move(spec), std::move(body));
}

auto HorizonThreadingProvider::placement() const noexcept
    -> const ThreadPlacementPolicy& {
  return threads_.placement();
}

auto HorizonThreadingProvider::current_thread() const noexcept -> ThreadId {
  return threads_.current_thread();
}

void HorizonThreadingProvider::adopt_current_thread(ThreadRole role) noexcept {
  threads_.adopt_current_thread(role);
}

auto HorizonThreadingProvider::usable_cores(CoreMask requested) noexcept
    -> std::uint64_t {
  const std::uint64_t available = process_cores();
  const std::uint64_t allowed = requested.value() & available;
  if (allowed != 0) {
    return allowed;
  }
  const std::uint64_t shared = available & fallback_cores;
  return shared != 0 ? shared : available;
}

auto HorizonThreadingProvider::orphans() const -> std::size_t {
  return threads_.orphans();
}

void HorizonThreadingProvider::place_current_thread(
    std::string_view name, const ThreadPlacement& placement) noexcept {
  static_cast<void>(name);
  const std::uint64_t cores = usable_cores(placement.cores());
  if (cores != 0) {
    static_cast<void>(svcSetThreadCoreMask(
        CUR_THREAD_HANDLE, std::countr_zero(cores), static_cast<u32>(cores)));
  }
  static_cast<void>(svcSetThreadPriority(
      CUR_THREAD_HANDLE, static_cast<u32>(placement.priority().value())));
}

auto HorizonThreadingProvider::process_cores() noexcept -> std::uint64_t {
  u64 mask = 0;
  if (R_FAILED(svcGetInfo(&mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0)) ||
      mask == 0) {
    return fallback_cores;
  }
  return mask;
}

}
