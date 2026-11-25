#include <text_input_demo.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/space.hpp>

#include <format>
#include <string>

namespace arisa::examples {

using namespace markup;
using composition::infinity;
using theme::ColourRole;

const composition::StateKey<std::string> TextInputDemo::query{
    "text-input-query", std::string{}};
const composition::StateKey<std::string> TextInputDemo::notes{
    "text-input-notes", std::string{"First line\nSecond line"}};
const composition::StateKey<std::string> TextInputDemo::handle{
    "text-input-handle", std::string{}};

auto TextInputDemo::body(composition::Context& context) const
    -> composition::Element {
  const auto years = context.state(age);
  const auto cost = context.state(price);
  const auto searched = context.state(searches);
  const auto typed = context.state(query);
  const auto written = context.state(notes);
  const auto named = context.state(handle);
  return VStack(alignment::leading, spacing(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      TextField(id(age_field))("Age", years, NumberFormat::number()),
      TextField("Price", cost, NumberFormat::number().precision(2)),
      TextField(id(search_field), submit_label(SubmitLabel::search),
                on_submit([searched] { searched.set(searched.get() + 1); }))(
          "Search", typed, TextFieldOptions{.prompt = "Games"}),
      Text(font::footnote, foreground_style(ColourRole::text_secondary))(
          std::format("Searches {}", searched.get())),
      TextField(
          text_input_autocapitalization(TextInputAutocapitalization::never),
          autocorrection_disabled(),
          text_content_type(TextContentType::username))("Handle", named),
      TextEditor(id(notes_editor), frame({.height = 120.0F}))(written));
}

}
