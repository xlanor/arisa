#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <string_view>

namespace arisa::examples {

class IconsPage final : public composition::Component {
 public:
  auto operator==(const IconsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto catalogue() -> composition::Element;

  static auto variants() -> composition::Element;

  static auto scales() -> composition::Element;

  static auto rendering() -> composition::Element;
};

}
