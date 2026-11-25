#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/token_editor.hpp>
#include <arisa/inspector/token_row.hpp>
#include <arisa/inspector/tokens_panel.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <string>
#include <variant>
#include <vector>

namespace arisa::inspector {

auto TokenRow::line() const -> composition::Element {
  const appearance::ThemeToken& token = appearance::ThemeToken::at(token_);
  std::vector<composition::Element> cells;
  if (const auto* colour = std::get_if<theme::Colour>(&value_)) {
    cells.push_back(TokensPanel::swatch(*colour));
  }
  cells.push_back(
      composition::Element{components::Text(std::string{token.label()})
                               .font(theme::Font::footnote)
                               .line_limit(1)}
          .frame(
              {.width = label_, .alignment = composition::Alignment::leading}));
  cells.push_back(composition::Element{
      components::Text(token.describe(value_)).font(theme::Font::footnote)}
                      .frame({.width = TokensPanel::value_width,
                              .alignment = composition::Alignment::trailing}));
  const std::size_t index = token_;
  composition::Element line{components::HStack(
      components::HStackOptions{.spacing = theme::space::xs},
      composition::Element{
          components::Button(
              [model = model_, index] { model->toggle_token(index); },
              composition::Element{
                  components::HStack(
                      components::HStackOptions{.spacing = theme::space::xs},
                      std::move(cells))}
                  .frame({.max_width = composition::infinity}))}
          .button_style(components::ButtonStyle::plain())
          .id(TokensPanel::row_key(token)),
      edited_ ? InspectorStyle::icon(
                    "Undo", "undo", false,
                    [model = model_, index] { model->undo_token(index); })
              : composition::Element{components::Spacer()}.frame(
                    {.width = TokensPanel::undo_width}))};
  line = std::move(line).frame({.max_width = composition::infinity});
  if (selected_) {
    line =
        std::move(line).background(theme::ColourRole::selected_text_background,
                                   theme::RoundedRectangle{6.0F});
  }
  return line;
}

auto TokenRow::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context);
  if (!selected_) {
    return line();
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          line(),
          composition::Element{TokenEditor{model_, token_, value_}}.id(
              std::string{TokenEditor::key}))}
      .frame({.max_width = composition::infinity});
}

}
