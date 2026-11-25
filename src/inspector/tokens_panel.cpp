#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/token_row.hpp>
#include <arisa/inspector/tokens_panel.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::inspector {

auto TokensPanel::row_key(const appearance::ThemeToken& token) -> std::string {
  return std::format("{}{}", row_prefix, token.path());
}

auto TokensPanel::group_key(appearance::TokenGroup group) -> std::string {
  return std::format("{}{}", group_prefix, appearance::ThemeToken::name(group));
}

auto TokensPanel::swatch(theme::Colour colour) -> composition::Element {
  return composition::Element{
      components::ShapeView(theme::RoundedRectangle{4.0F}).fill(colour)}
      .overlay(composition::Element{
          components::ShapeView(theme::RoundedRectangle{4.0F})
              .stroke(theme::ColourRole::separator, 1.0F)})
      .frame({.width = swatch_size, .height = swatch_size});
}

auto TokensPanel::actions(const appearance::AppearanceModel& appearance) const
    -> composition::Element {
  std::vector<composition::Element> rows;
  rows.push_back(composition::Element{
      components::HStack(
          components::HStackOptions{.spacing = theme::space::xs},
          composition::Element{
              components::Button("Reset all",
                                 [model = model_] { model->reset_tokens(); })}
              .button_style(components::ButtonStyle::bordered())
              .disabled(appearance.edits().empty())
              .id(std::string{reset_key}),
          composition::Element{components::Button("Export",
                                                  [model = model_] {
                                                    static_cast<void>(
                                                        model->export_tokens());
                                                  })}
              .button_style(components::ButtonStyle::bordered())
              .id(std::string{export_key}),
          components::Spacer())}
                     .frame({.max_width = composition::infinity}));
  if (!model_->export_status().empty()) {
    rows.push_back(InspectorStyle::line(model_->export_status()));
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto TokensPanel::row(const appearance::ThemeToken& token,
                      const appearance::AppearanceModel& appearance) const
    -> composition::Element {
  appearance::TokenValue value = token.read(appearance.theme());
  const bool shaded = std::holds_alternative<theme::Colour>(value);
  const float label =
      std::max(model_->panel_width() - InspectorModel::row_inset - value_width -
                   (shaded ? swatch_size + cell_gap : 0.0F) - undo_width,
               0.0F);
  return TokenRow{model_,
                  token.index(),
                  std::move(value),
                  appearance.edits().edited(token),
                  model_->selected_token() == token.index(),
                  label};
}

auto TokensPanel::group(appearance::TokenGroup group,
                        const appearance::AppearanceModel& appearance) const
    -> composition::Element {
  const bool open = model_->token_group() == group;
  std::vector<composition::Element> rows;
  rows.push_back(composition::Element{
      components::Button(
          [model = model_, group] { model->toggle_tokens(group); },
          composition::Element{
              components::HStack(
                  components::HStackOptions{.spacing = theme::space::xs},
                  components::Text(open ? "-" : "+")
                      .font(theme::Font::headline),
                  InspectorStyle::heading(appearance::ThemeToken::name(group)),
                  components::Spacer())}
              .frame({.max_width = composition::infinity}))}
                     .button_style(components::ButtonStyle::plain())
                     .frame({.max_width = composition::infinity})
                     .id(group_key(group)));
  if (open) {
    const theme::ColorScheme scheme = model_->token_scheme();
    for (const appearance::ThemeToken& token :
         appearance::ThemeToken::in(group)) {
      const std::optional<theme::ColorScheme> side = token.scheme();
      if (group == appearance::TokenGroup::colours && side.has_value() &&
          *side != scheme) {
        continue;
      }
      rows.push_back(row(token, appearance));
    }
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xxs},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto TokensPanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  const std::optional<appearance::AppearanceModel> appearance =
      model_->appearance();
  if (!appearance.has_value()) {
    return InspectorStyle::line("No appearance model");
  }
  static_cast<void>(context.read(appearance->settings()));
  const std::optional<std::string> target =
      model_->selected_token().transform([](std::size_t token) {
        return row_key(appearance::ThemeToken::at(token));
      });
  return components::ScrollViewReader([panel = *this, model = *appearance,
                                       target](
                                          composition::ScrollViewProxy proxy)
                                          -> composition::Element {
    std::vector<composition::Element> sections;
    sections.push_back(panel.actions(model));
    for (const appearance::TokenGroup each : appearance::ThemeToken::groups) {
      sections.push_back(panel.group(each, model));
    }
    return composition::Element{
        components::ScrollView(
            composition::Element{
                components::VStack(
                    components::VStackOptions{
                        .alignment = components::HorizontalAlignment::leading,
                        .spacing = theme::space::sm},
                    std::move(sections))}
                .frame({.max_width = composition::infinity,
                        .alignment = composition::Alignment::top_leading}))}
        .on_change(target, true, [proxy, target] {
          if (target.has_value()) {
            proxy.scroll_to(composition::ElementKey{*target},
                            motion::UnitPoint::center);
          }
        });
  });
}

}
