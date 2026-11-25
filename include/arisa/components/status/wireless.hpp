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

class Wireless final : public composition::Component {
 public:
  static constexpr std::chrono::seconds interval{10};

  static constexpr composition::StateKey<int> ticks{"status-wireless-ticks", 0};

  Wireless() = default;

  explicit Wireless(theme::Font font) noexcept : font_(font) {}

  auto operator==(const Wireless&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto symbol(const provider::NetworkState& state) noexcept
      -> std::string_view;

 private:
  static auto refresh(composition::StateHandle<int> count) -> async::Task<void>;

  theme::Font font_{theme::Font::title3};
};

}
