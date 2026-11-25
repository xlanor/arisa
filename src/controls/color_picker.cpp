#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/color_picker.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::controls {

const composition::StateKey<std::string> ColorPicker::Panel::hex{
    "color-picker-hex", std::string{}};

ColorPicker::ColorPicker(std::string title,
                         composition::Binding<theme::Colour> selection,
                         bool supports_opacity)
    : ColorPicker(std::move(selection),
                  components::Text(text::LocalizedStringKey{std::move(title)}),
                  supports_opacity) {}

ColorPicker::ColorPicker(composition::Binding<theme::Colour> selection,
                         composition::Element label, bool supports_opacity)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(selection), supports_opacity)) {}

auto ColorPicker::swatch(theme::Colour colour) -> composition::Element {
  return components::ZStack()
      .frame({.width = swatch_width - 8.0F, .height = swatch_height - 8.0F})
      .background(colour, theme::Shape::rounded(theme::radius::control, true))
      .border(theme::ColourRole::separator, theme::Stroke::hairline());
}

auto ColorPicker::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ColorPicker::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto open = context.state(shown);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  composition::Element well =
      components::Button([open] { open.set(true); }, swatch(selection_.get()))
          .button_style(components::ButtonStyle::bordered())
          .id(swatch_key)
          .popover(open, composition::PopoverAttachmentAnchor::rect(),
                   theme::Edge::top,
                   [selection = selection_, opacity = supports_opacity_] {
                     return composition::Element{Panel{selection, opacity}};
                   });
  std::vector<composition::Element> produced;
  if (hidden) {
    produced.push_back(std::move(well));
    return produced;
  }
  composition::Element label{composition::PlaceholderContentView(label_)};
  if (const std::optional<components::LabelColumn> column =
          components::LabeledContent::column(context)) {
    produced.push_back(components::LabeledContent::columned(
        *column, std::move(label), std::move(well)));
    return produced;
  }
  produced.push_back(components::HStack(
                         components::HStackOptions{
                             .alignment = components::VerticalAlignment::center,
                             .spacing = theme::space::md},
                         std::move(label), components::Spacer(),
                         std::move(well))
                         .frame({.max_width = composition::infinity}));
  return produced;
}

auto ColorPicker::Panel::channel(std::string_view title,
                                 composition::StateHandle<double> value,
                                 double upper, double step,
                                 std::string_view key,
                                 std::function<void()> push)
    -> composition::Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::xxs},
      components::Text(std::string{title})
          .font(theme::Font::footnote)
          .foreground_style(theme::ColourRole::text_secondary),
      composition::Element{
          Slider(value,
                 ClosedRange<double>{.lower_bound = 0.0, .upper_bound = upper},
                 step)}
          .id(std::string{key})
          .on_change(value.get(), [push = std::move(push)] { push(); }));
}

auto ColorPicker::Panel::body(composition::Context& context) const
    -> composition::Element {
  const auto h = context.state(hue);
  const auto s = context.state(saturation);
  const auto b = context.state(brightness);
  const auto a = context.state(alpha);
  const auto seed = context.state(seeded);
  const auto text = context.state(hex);
  const composition::Binding<theme::Colour> selection = selection_;
  const bool opacity = supports_opacity_;
  const theme::Colour current = selection.get();
  const auto show = [h, s, b, a, text](theme::Colour colour) {
    const theme::Hsb parts = theme::Hsb::from(colour);
    h.set(parts.hue);
    s.set(parts.saturation);
    b.set(parts.brightness);
    a.set(parts.alpha);
    text.set(appearance::ThemeVocabulary::hex(colour));
  };
  const std::function<void()> push = [h, s, b, a, selection, opacity, seed,
                                      text] {
    if (!seed.get()) {
      return;
    }
    const theme::Colour picked =
        theme::Hsb{.hue = static_cast<float>(h.get()),
                   .saturation = static_cast<float>(s.get()),
                   .brightness = static_cast<float>(b.get()),
                   .alpha = opacity ? static_cast<float>(a.get()) : 1.0F}
            .colour();
    const std::string written = appearance::ThemeVocabulary::hex(picked);
    if (written != appearance::ThemeVocabulary::hex(selection.get())) {
      selection.set(picked);
      text.set(written);
    }
  };
  std::vector<composition::Element> rows;
  rows.push_back(
      components::ZStack()
          .frame({.height = preview_height, .max_width = composition::infinity})
          .background(current,
                      theme::Shape::rounded(theme::radius::control, true))
          .border(theme::ColourRole::separator, theme::Stroke::hairline()));
  rows.push_back(composition::Element{TextField("Hex", text)}
                     .id(std::string{hex_key})
                     .on_change(text.get(), [text, selection, show, seed] {
                       const std::optional<theme::Colour> typed =
                           appearance::ThemeVocabulary::colour(text.get());
                       if (seed.get() && typed.has_value() &&
                           *typed != selection.get()) {
                         selection.set(*typed);
                         show(*typed);
                       }
                     }));
  rows.push_back(channel("Hue", h, 360.0, 1.0, hue_key, push));
  rows.push_back(channel("Saturation", s, 1.0, 0.01, saturation_key, push));
  rows.push_back(channel("Brightness", b, 1.0, 0.01, brightness_key, push));
  if (opacity) {
    rows.push_back(channel("Opacity", a, 1.0, 0.01, alpha_key, push));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             std::move(rows))
      .padding(theme::space::lg)
      .frame({.width = panel_width})
      .on_appear([show, seed, current] {
        show(current);
        seed.set(true);
      });
}

}
