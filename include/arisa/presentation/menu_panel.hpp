#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>

namespace arisa::presentation {

class MenuPanel {
 public:
  struct ItemStyle {
    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> composition::Element;

    friend auto operator==(const ItemStyle&, const ItemStyle&)
        -> bool = default;
  };

  static constexpr float panel_width = 240.0F;

  static auto items(composition::Element declared,
                    composition::Binding<bool> shown) -> composition::Element;

  static auto items(composition::Element declared,
                    composition::Binding<bool> shown, bool reversed,
                    bool dismisses) -> composition::Element;
};

}
