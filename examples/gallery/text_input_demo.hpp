#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include <string>
#include <string_view>

namespace arisa::examples {

class TextInputDemo final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> age{"text-input-age", 30};
  static constexpr composition::StateKey<double> price{"text-input-price", 4.5};
  static constexpr composition::StateKey<int> searches{"text-input-searches",
                                                       0};
  static const composition::StateKey<std::string> query;
  static const composition::StateKey<std::string> notes;
  static const composition::StateKey<std::string> handle;
  static constexpr std::string_view age_field = "text-input-age";
  static constexpr std::string_view search_field = "text-input-search";
  static constexpr std::string_view notes_editor = "text-input-notes";

  auto operator==(const TextInputDemo&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
