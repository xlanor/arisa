#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/crash_reporter.hpp>

#include <string>
#include <string_view>

namespace arisa::examples {

class CrashesPage final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> delivery{"crashes-delivery", 0};
  static constexpr std::string_view delivery_key = "crashes-delivery";

  auto operator==(const CrashesPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto key(provider::CrashDrill drill) -> std::string;

  static auto title(provider::CrashDrill drill) noexcept -> std::string_view;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto drills() -> composition::Element;

  static auto probe(composition::StateHandle<int> answer)
      -> composition::Element;
};

}
