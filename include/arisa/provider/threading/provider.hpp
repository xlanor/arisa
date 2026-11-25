#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_placement_policy.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>

#include <expected>
#include <functional>
#include <stop_token>

namespace arisa::provider {

using ThreadBody = std::move_only_function<void(std::stop_token)>;

class ThreadingProvider {
 public:
  virtual ~ThreadingProvider() = default;

  [[nodiscard]] virtual auto start(ThreadSpec spec, ThreadBody body)
      -> std::expected<Thread, ProviderError> = 0;

  virtual auto placement() const noexcept -> const ThreadPlacementPolicy& = 0;

  virtual auto current_thread() const noexcept -> ThreadId = 0;

  virtual void adopt_current_thread(ThreadRole role) noexcept = 0;
};

}
