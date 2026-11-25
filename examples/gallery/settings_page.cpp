#include <settings_page.hpp>
#include <world_page.hpp>

#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/theme/space.hpp>

namespace arisa::examples {

auto SettingsPage::body(composition::Context&) const -> composition::Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             structure::Form(structure::AppearanceSettingsSection{})
                 .frame({.max_width = composition::infinity,
                         .min_height = 0.0F,
                         .max_height = composition::infinity})
                 .flex_shrink(1.0F),
             WorldPage::Link{})
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::leading});
}

}
