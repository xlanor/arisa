#include <text_editing_page.hpp>

#include <arisa/components/attributed_text_formatting.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_selectability.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/controls/text_input_border_shape.hpp>
#include <arisa/controls/text_input_suggestions.hpp>
#include <arisa/controls/text_selection_host.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;

void TextEditingPage::Wave::draw(const components::Text::Layout& layout,
                                 components::GraphicsContext& context) const {
  float phase = 0.0F;
  for (const components::Text::Layout::Line line : layout) {
    for (const components::Text::Layout::Run run : line) {
      for (const components::Text::Layout::RunSlice glyph : run) {
        const graphics::AffineTransform saved = context.transform();
        context.translate_by(0.0F, amplitude * std::sin(phase));
        context.draw(glyph);
        context.set_transform(saved);
        phase += 0.6F;
      }
    }
  }
}

auto TextEditingPage::Wave::display_padding() const -> theme::EdgeInsets {
  return theme::EdgeInsets::all(theme::Space{amplitude});
}

auto TextEditingPage::section(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto TextEditingPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const auto named = context.state(name);
  const auto chosen = context.state(selection);
  const auto written = context.state(notes);
  const auto formatted = context.state(styled);
  const auto formatted_selection = context.state(styled_selection);
  const auto sized = context.state(size);
  const auto priced = context.state(cost);
  const auto dated = context.state(day);
  const auto place = context.state(town);
  const auto searching = context.state(finding);
  return presentation::NavigationStack([=] {
    using controls::TextField;
    std::vector<std::string> cities{"Lisbon", "Lima", "Linz", "Lyon"};
    return components::ScrollView(
               components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::xl},
                   section(
                       "Selection",
                       components::VStack(
                           TextField(
                               "Name", composition::Binding<std::string>{named},
                               composition::Binding<
                                   std::optional<composition::TextSelection>>{
                                   chosen})
                               .id(std::string{field}),
                           controls::TextEditor(
                               composition::Binding<std::string>{written})
                               .frame({.height = 96.0F})
                               .find_navigator(
                                   composition::Binding<bool>{searching})
                               .id(std::string{editor}))),
                   section(
                       "Selectable text",
                       components::Text(
                           "Hold to copy this sentence, or double tap a word")
                           .text_selection(
                               composition::TextSelectability::enabled)
                           .id(std::string{selectable})),
                   section(
                       "Rich text",
                       controls::TextEditor(
                           composition::Binding<components::AttributedString>{
                               formatted},
                           composition::Binding<
                               components::AttributedTextSelection>{
                               formatted_selection})
                           .attributed_text_formatting_definition(
                               components::AttributedTextFormatting::
                                   ValueConstraint<
                                       components::AttributedTextFormatting::
                                           Weight>{{std::nullopt,
                                                    theme::Font::Weight::bold},
                                                   std::nullopt})
                           .frame({.height = 72.0F})
                           .id(std::string{rich})),
                   section("Renderer", components::Text("Waves of glyphs")
                                           .font(theme::Font::title2)
                                           .text_renderer(Wave{})
                                           .id(std::string{wave})),
                   section(
                       "Styles",
                       components::VStack(
                           TextField("Bordered",
                                     composition::Binding<std::string>{named})
                               .text_field_style(
                                   controls::TextFieldStyle::bordered())
                               .id(std::string{bordered}),
                           TextField("Square",
                                     composition::Binding<std::string>{named})
                               .text_field_style(
                                   controls::TextFieldStyle::square_border())
                               .id(std::string{square}),
                           TextField("Capsule",
                                     composition::Binding<std::string>{named})
                               .text_field_style(
                                   controls::TextFieldStyle::bordered())
                               .text_input_border_shape(
                                   controls::TextInputBorderShape::capsule())
                               .id(std::string{capsule}))),
                   section(
                       "Formats",
                       components::VStack(
                           TextField(
                               "Date", dated,
                               text::DateFormatStyle{
                                   text::DateFormatStyle::DateStyle::numeric,
                                   text::DateFormatStyle::TimeStyle::omitted})
                               .id(std::string{date}),
                           TextField("Size", sized,
                                     text::ByteCountFormatStyle{})
                               .id(std::string{bytes}),
                           TextField("Price", priced,
                                     text::NumberFormatStyle::currency("EUR"))
                               .id(std::string{price}))),
                   section("Suggestions", TextField(
                                              "City",
                                              composition::Binding<std::string>{
                                                  place})
                                              .text_input_suggestions(
                                                  cities,
                                                  [](const std::string& entry) {
                                                    return components::Text(
                                                               entry)
                                                        .text_input_completion(
                                                            entry);
                                                  })
                                              .id(std::string{city})))
                   .padding(theme::space::xl)
                   .frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))
        .frame({.max_width = infinity, .max_height = infinity})
        .navigation_title("Text editing")
        .toolbar(
            composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close",
                                   [shown_flag] { shown_flag.set(false); })
                    .id(std::string{close_button})),
            composition::ToolbarItem(
                composition::ToolbarItemPlacement::primary_action,
                components::Button(
                    "Find", [searching] { searching.set(!searching.get()); })
                    .id(std::string{find_button})),
            composition::ToolbarItem(
                composition::ToolbarItemPlacement::keyboard,
                components::Button("Done", [] {
                }).id(std::string{done_button})));
  });
}

auto TextEditingPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Text editing", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{TextEditingPage{shown}}; });
}

}
