#include <feedback_page.hpp>

#include <input_page.hpp>
#include <interaction_page.hpp>
#include <pointer_page.hpp>
#include <symbols_page.hpp>
#include <tables_page.hpp>
#include <text_editing_page.hpp>
#include <text_formats_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/structure/haptic_strength_slider.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto FeedbackPage::key(std::size_t index) -> std::string {
  return std::format("feedback-{}", index);
}

auto FeedbackPage::body(composition::Context& context) const -> Element {
  const auto played = context.state(plays);
  const auto quiet = context.state(silent);
  const std::array<int, kinds> counts = played.get();
  std::vector<Element> buttons;
  const auto catalogue = SensoryFeedback::catalogue();
  for (std::size_t index = 0; index < catalogue.size() && index < kinds;
       ++index) {
    const SensoryFeedback feedback = catalogue[index];
    buttons.push_back(Button(action([played, index] {
                               std::array<int, kinds> next = played.get();
                               ++next[index];
                               played.set(next);
                             }),
                             button_style(ButtonStyle::bordered()),
                             sensory_feedback(feedback, counts[index]),
                             id(key(index)))(std::string{feedback.name()}));
  }
  const auto card = [](std::string_view title, Element content) {
    return VStack(alignment::leading, spacing(theme::space::md),
                  padding(theme::space::lg),
                  frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}),
                  background(ColourRole::surface_raised,
                             RoundedRectangle(theme::radius::card,
                                              RoundedCornerStyle::continuous)),
                  border(ColourRole::separator, theme::Stroke::hairline()),
                  shadow(theme::Elevation::low))(
        Text(font::headline)(std::string{title}), std::move(content));
  };
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(VStack(
      alignment::leading, spacing(theme::space::lg),
      frame({.max_width = infinity,
             .alignment = composition::Alignment::leading}))(
      card("Strength", Element{structure::HapticStrengthSlider{}}),
      card("Feedback",
           HStack(spacing(theme::space::sm), wrap,
                  frame({.max_width = infinity}))(std::move(buttons))),
      card("Disabled",
           HStack(spacing(theme::space::sm), sensory_feedback_disabled())(
               Button(action([quiet] { quiet.set(quiet.get() + 1); }),
                      button_style(ButtonStyle::bordered()),
                      sensory_feedback(SensoryFeedback::success, quiet.get()),
                      id(silent_button))("Success"))),
      card("Input", Element{InputPage::Opener{}}),
      card("Pointer", Element{PointerPage::Opener{}}),
      card("Tables", Element{TablesPage::Opener{}}),
      card("Symbols", Element{SymbolsPage::Opener{}}),
      card("Text formats", Element{TextFormatsPage::Opener{}}),
      card("Interaction", Element{InteractionPage::Opener{}}),
      card("Text editing", Element{TextEditingPage::Opener{}})));
}

}
