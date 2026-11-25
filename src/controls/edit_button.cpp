#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/controls/edit_button.hpp>

#include <optional>
#include <string>

namespace arisa::controls {

auto EditButton::body(composition::Context& context) const
    -> composition::Element {
  const std::optional<components::EditModeBinding>& mode =
      context.environment(components::Environment::edit_mode_binding);
  const bool editing =
      context.environment(components::Environment::edit_mode) !=
      components::EditMode::inactive;
  return components::Button(std::string{editing ? done_label : edit_label},
                            [mode] {
                              if (mode.has_value()) {
                                mode->toggle();
                              }
                            })
      .disabled(!mode.has_value());
}

}
