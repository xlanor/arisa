#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/system_info/provider.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <string_view>

namespace arisa::components::status {

class Battery final : public composition::Component {
 public:
  static constexpr std::chrono::seconds interval{30};

  static constexpr composition::StateKey<int> ticks{"status-battery-ticks", 0};

  Battery() = default;

  Battery(theme::Font label, theme::Font symbol) noexcept
      : label_(label), symbol_(symbol) {}

  auto operator==(const Battery&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto symbol(const provider::BatteryState& state) noexcept
      -> std::string_view;

 private:
  static auto refresh(composition::StateHandle<int> count) -> async::Task<void>;

  theme::Font label_{theme::Font::footnote};
  theme::Font symbol_{theme::Font::title3};
};

}
