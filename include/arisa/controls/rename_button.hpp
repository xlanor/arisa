#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <string_view>

namespace arisa::controls {

class RenameButton final : public composition::Component {
 public:
  static constexpr std::string_view label = "Rename";
  static constexpr std::string_view symbol = "edit";

  auto operator==(const RenameButton&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
