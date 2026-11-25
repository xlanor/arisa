#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <string_view>

namespace arisa::controls {

class EditButton final : public composition::Component {
 public:
  static constexpr std::string_view edit_label = "Edit";
  static constexpr std::string_view done_label = "Done";

  auto operator==(const EditButton&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
