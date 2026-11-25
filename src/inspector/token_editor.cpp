#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/token_editor.hpp>
#include <arisa/inspector/tokens_panel.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <optional>
#include <variant>
#include <vector>

namespace arisa::inspector {

const composition::StateKey<std::string> TokenEditor::hex{"token-hex"};

auto TokenEditor::Channels::hsb() const -> theme::Hsb {
  return theme::Hsb{.hue = static_cast<float>(hue.get()),
                    .saturation = static_cast<float>(saturation.get()),
                    .brightness = static_cast<float>(brightness.get()),
                    .alpha = static_cast<float>(alpha.get())};
}

void TokenEditor::Channels::show(theme::Colour colour) const {
  const theme::Hsb shown = theme::Hsb::from(colour);
  hue.set(shown.hue);
  saturation.set(shown.saturation);
  brightness.set(shown.brightness);
  alpha.set(shown.alpha);
}

auto TokenEditor::choice_key(std::size_t choice) -> std::string {
  return std::format("arisa.inspector.token.choice.{}", choice);
}

auto TokenEditor::labelled(std::string_view label, composition::Element control)
    -> composition::Element {
  return composition::Element{
      components::HStack(
          components::HStackOptions{.spacing = theme::space::sm},
          composition::Element{
              components::Text(std::string{label}).font(theme::Font::footnote)}
              .frame({.width = label_width,
                      .alignment = composition::Alignment::leading}),
          std::move(control).frame({.max_width = composition::infinity}))}
      .frame({.max_width = composition::infinity});
}

auto TokenEditor::colour(composition::Context& context,
                         theme::Colour current) const -> composition::Element {
  const Channels channels{.hue = context.state(hue),
                          .saturation = context.state(saturation),
                          .brightness = context.state(brightness),
                          .alpha = context.state(alpha)};
  const composition::StateHandle<std::string> text = context.state(hex);
  const std::size_t token = token_;
  const auto push = [model = model_, token, channels, current] {
    if (channels.hsb() != theme::Hsb::from(current)) {
      model->edit_token(token, channels.hsb().colour());
    }
  };
  const auto channel = [&push](std::string_view label,
                               composition::StateHandle<double> value,
                               double upper, double step, std::string_view id) {
    return labelled(
        label,
        composition::Element{
            controls::Slider(value,
                             controls::ClosedRange<double>{
                                 .lower_bound = 0.0, .upper_bound = upper},
                             step)}
            .id(std::string{id})
            .on_change(value.get(), push));
  };
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          labelled(
              "Hex",
              composition::Element{
                  components::HStack(
                      components::HStackOptions{.spacing = theme::space::sm},
                      TokensPanel::swatch(current),
                      composition::Element{controls::TextField("Hex", text)}
                          .frame({.max_width = composition::infinity})
                          .id(std::string{hex_key}))}
                  .on_change(text.get(),
                             [model = model_, token, text, current] {
                               const std::optional<theme::Colour> typed =
                                   appearance::ThemeVocabulary::colour(
                                       text.get());
                               if (typed.has_value() && *typed != current) {
                                 model->edit_token(token, *typed);
                               }
                             })),
          channel("Hue", channels.hue, 360.0, 1.0, hue_key),
          channel("Saturation", channels.saturation, 1.0, 0.01, saturation_key),
          channel("Brightness", channels.brightness, 1.0, 0.01, brightness_key),
          channel("Alpha", channels.alpha, 1.0, 0.01, alpha_key))}
      .frame({.max_width = composition::infinity})
      .on_change(current, true, [channels, text, current] {
        if (appearance::ThemeToken::quantised(channels.hsb().colour()) !=
            current) {
          channels.show(current);
        }
        if (appearance::ThemeVocabulary::colour(text.get()) != current) {
          text.set(appearance::ThemeVocabulary::hex(current));
        }
      });
}

auto TokenEditor::slider(composition::Context& context, float current) const
    -> composition::Element {
  const appearance::ThemeToken& token = appearance::ThemeToken::at(token_);
  const appearance::TokenRange range =
      token.range().value_or(appearance::TokenRange{});
  const composition::StateHandle<double> value = context.state(number);
  const std::size_t index = token_;
  return labelled(
      "Value",
      composition::Element{controls::Slider(value,
                                            controls::ClosedRange<double>{
                                                .lower_bound = range.minimum,
                                                .upper_bound = range.maximum},
                                            range.step)}
          .id(std::string{number_key})
          .on_change(value.get(),
                     [model = model_, index, value, current] {
                       if (static_cast<float>(value.get()) != current) {
                         model->edit_token(index,
                                           static_cast<float>(value.get()));
                       }
                     })
          .on_change(current, true, [value, current] {
            if (static_cast<float>(value.get()) != current) {
              value.set(static_cast<double>(current));
            }
          }));
}

auto TokenEditor::switcher(composition::Context& context, bool current) const
    -> composition::Element {
  const composition::StateHandle<bool> value = context.state(toggle);
  const std::size_t index = token_;
  return composition::Element{
      controls::Toggle(std::string{appearance::ThemeToken::at(token_).label()},
                       value)}
      .id(std::string{toggle_key})
      .on_change(value.get(),
                 [model = model_, index, value, current] {
                   if (value.get() != current) {
                     model->edit_token(index, value.get());
                   }
                 })
      .on_change(current, true, [value, current] {
        if (value.get() != current) {
          value.set(current);
        }
      });
}

auto TokenEditor::chooser(std::size_t current) const -> composition::Element {
  const appearance::ThemeToken& token = appearance::ThemeToken::at(token_);
  std::vector<composition::Element> choices;
  const std::size_t index = token_;
  for (std::size_t choice = 0; choice < token.choices().size(); ++choice) {
    choices.push_back(InspectorStyle::tab(token.choices()[choice],
                                          choice == current,
                                          [model = model_, index, choice] {
                                            model->edit_token(index, choice);
                                          })
                          .id(choice_key(choice)));
  }
  return composition::Element{
      components::HStack(
          components::HStackOptions{.spacing = theme::space::xxs},
          std::move(choices))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto TokenEditor::body(composition::Context& context) const
    -> composition::Element {
  composition::Element editor{components::Spacer()};
  if (const auto* shade = std::get_if<theme::Colour>(&value_)) {
    editor = colour(context, *shade);
  } else if (const auto* amount = std::get_if<float>(&value_)) {
    editor = slider(context, *amount);
  } else if (const auto* on = std::get_if<bool>(&value_)) {
    editor = switcher(context, *on);
  } else {
    editor = chooser(std::get<std::size_t>(value_));
  }
  return std::move(editor)
      .padding(theme::Insets{.top = theme::space::xs,
                             .leading = theme::space::md,
                             .bottom = theme::space::sm})
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

}
