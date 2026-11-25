#include <profile_form.hpp>
#include <profile_summary.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/space.hpp>

#include <cmath>
#include <format>

namespace arisa::examples {

using namespace markup;
using composition::infinity;
using theme::ColourRole;

auto ProfileSummary::body(composition::Context& context) const
    -> composition::Element {
  const GalleryProfile& profile = context.read(*model_);
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      LabeledContent("Notifications", profile.notifications ? "On" : "Off"),
      LabeledContent("Volume",
                     std::format("{}%", std::lround(profile.volume * 100.0))),
      LabeledContent("Devices", std::format("{}", profile.devices)),
      LabeledContent(id(summary_text))(
          "Plan", std::string{ProfileForm::plan_name(profile.plan)}));
}

}
