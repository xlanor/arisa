#include <profile_form.hpp>

#include <arisa/composition/bindable.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/space.hpp>

#include <format>
#include <optional>
#include <string>

namespace arisa::examples {

using namespace markup;
using composition::infinity;

auto ProfileForm::plan_name(GalleryProfile::Plan plan) -> std::string_view {
  switch (plan) {
    case GalleryProfile::Plan::basic:
      return "Basic";
    case GalleryProfile::Plan::plus:
      return "Plus";
    case GalleryProfile::Plan::pro:
      return "Pro";
  }
  return "Basic";
}

auto ProfileForm::body(composition::Context& context) const
    -> composition::Element {
  const composition::Bindable profile{context, *model_};
  return VStack(alignment::leading, spacing(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      Toggle("Notifications", profile[&GalleryProfile::notifications]),
      Slider(id(volume_slider))(
          profile[&GalleryProfile::volume],
          ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
          std::optional<double>{0.05},
          components::Image::system_name("volume_down"),
          components::Image::system_name("volume_up")),
      Stepper(std::format("Devices {}", model_->get().devices),
              profile[&GalleryProfile::devices],
              ClosedRange<int>{.lower_bound = 0, .upper_bound = 8}),
      Picker(picker_style(SegmentedPickerStyle{}))(
          "Plan", profile[&GalleryProfile::plan], [] {
            return Group(Text(tag(GalleryProfile::Plan::basic))("Basic"),
                         Text(tag(GalleryProfile::Plan::plus))("Plus"),
                         Text(tag(GalleryProfile::Plan::pro))("Pro"));
          }));
}

}
