#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

namespace arisa::examples {

class SettingsPage final : public composition::Component {
 public:
  auto operator==(const SettingsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
