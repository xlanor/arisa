#pragma once

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace arisa::provider {

template <class... Args>
class GpuSurfaces;

class FrameSchedule : public std::enable_shared_from_this<FrameSchedule> {
 public:
  struct Demand {
    bool continuous{false};
    std::optional<double> frame_rate{};

    friend constexpr auto operator==(const Demand&, const Demand&)
        -> bool = default;
  };

  struct Listing {
    FrameHookId id{0};
    FrameDrive drive{FrameDrive::on_demand};
    std::optional<double> frame_rate{};
    bool surface{false};

    friend constexpr auto operator==(const Listing&, const Listing&)
        -> bool = default;
  };

  using Waker = std::move_only_function<void()>;

  FrameSchedule() = default;
  FrameSchedule(const FrameSchedule&) = delete;
  auto operator=(const FrameSchedule&) -> FrameSchedule& = delete;
  FrameSchedule(FrameSchedule&&) = delete;
  auto operator=(FrameSchedule&&) -> FrameSchedule& = delete;
  ~FrameSchedule() = default;

  auto demand() const -> Demand;

  auto installed() const noexcept -> bool;

  auto take_request() noexcept -> bool;

  void request_frame();

  auto wake_with(Waker waker) -> support::ScopedCallback;

  auto next_serial() noexcept -> GpuSerial;

  auto next_serial_after(GpuSerial last) noexcept -> std::optional<GpuSerial>;

  auto serial() const noexcept -> GpuSerial;

  auto listing() const -> std::vector<Listing>;

 protected:
  auto adopt(FrameHookOptions options) -> FrameHookId;

  void retire(FrameHookId id);

 private:
  friend class FrameHook;

  template <class... Args>
  friend class GpuSurfaces;

  struct Lease {
    FrameHookOptions options;
    bool hooked{true};
  };

  auto lease(FrameHookOptions options, bool hooked) -> FrameHookId;

  void set_drive(FrameHookId id, FrameDrive drive);

  auto drive(FrameHookId id) const -> FrameDrive;

  void wake();

  mutable std::mutex mutex_;
  std::flat_map<std::uint64_t, Lease> hooks_;
  std::uint64_t next_id_{1};
  std::size_t hooked_{0};
  std::atomic<std::size_t> installed_{0};
  std::atomic<bool> requested_{false};
  std::atomic<std::uint64_t> serial_{0};
  std::mutex waker_mutex_;
  Waker waker_;
};

}
