#pragma once

#include <arisa/support/scoped_callback.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::provider {

enum class FrameStage : std::uint8_t {
  before_ui,
  after_ui,
};

enum class FrameDrive : std::uint8_t {
  on_demand,
  continuous,
};

struct FrameHookOptions {
  FrameDrive drive{FrameDrive::on_demand};
  std::optional<double> frame_rate{};

  friend constexpr auto operator==(const FrameHookOptions&,
                                   const FrameHookOptions&) -> bool = default;
};

struct FrameHookIdTag;

using FrameHookId = support::StrongValue<std::uint64_t, FrameHookIdTag>;

class FrameSchedule;

template <class... Args>
class FrameHooks;

class FrameHook {
 public:
  FrameHook(FrameHook&& other) noexcept = default;
  auto operator=(FrameHook&& other) noexcept -> FrameHook& = default;
  FrameHook(const FrameHook&) = delete;
  auto operator=(const FrameHook&) -> FrameHook& = delete;
  ~FrameHook() = default;

  void request_frame() const;

  void set_drive(FrameDrive drive) const;

  auto drive() const -> FrameDrive;

  auto active() const noexcept -> bool;

 private:
  template <class... Args>
  friend class FrameHooks;

  FrameHook(std::shared_ptr<FrameSchedule> schedule, FrameHookId id,
            support::ScopedCallback release) noexcept;

  std::shared_ptr<FrameSchedule> schedule_;
  FrameHookId id_;
  support::ScopedCallback release_;
};

}
