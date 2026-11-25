#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/controls/rename_button.hpp>

#include <optional>
#include <string>

namespace arisa::controls {

auto RenameButton::body(composition::Context& context) const
    -> composition::Element {
  const std::optional<components::RenameAction>& rename =
      context.environment(components::Environment::rename_action);
  return components::Button(
             [rename] {
               if (rename.has_value()) {
                 (*rename)();
               }
             },
             components::Label(
                 std::string{label},
                 components::Image::system_name(std::string{symbol})))
      .disabled(!rename.has_value());
}

}
