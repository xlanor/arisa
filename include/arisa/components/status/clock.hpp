#pragma once

#include <arisa/async/task.hpp>
#include <arisa/components/system_status.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <string>

namespace arisa::components::status {

class Clock final : public composition::Component {
 public:
  static constexpr std::chrono::seconds minute{60};

  static constexpr composition::StateKey<int> ticks{"status-clock-ticks", 0};

  Clock() = default;

  explicit Clock(theme::Font font) noexcept : font_(font) {}

  auto operator==(const Clock&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto text(SystemStatus::LocalTime time) -> std::string;

  static auto until_next_minute(SystemStatus::LocalTime time) noexcept
      -> std::chrono::seconds;

 private:
  static auto refresh(composition::StateHandle<int> count, SystemStatus status)
      -> async::Task<void>;

  theme::Font font_{theme::Font::headline};
};

}
