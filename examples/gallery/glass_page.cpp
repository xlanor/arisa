#include <glass_page.hpp>

#include <arisa/components/alignment.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <string>
#include <vector>

namespace arisa::examples {

using composition::Element;
using Variant = GlassPreview::Variant;
using Tint = GlassPreview::Tint;
using Form = GlassPreview::Form;
using Size = GlassPreview::Size;
using Grouping = GlassPreview::Grouping;

auto GlassPage::chosen(composition::Context& context) -> Choices {
  return Choices{.variant = context.state(variant),
                 .tint = context.state(tint),
                 .interactive = context.state(interactive),
                 .form = context.state(form),
                 .size = context.state(size),
                 .grouping = context.state(grouping),
                 .count = context.state(count),
                 .spacing = context.state(spacing)};
}

auto GlassPage::look(const Choices& choices) -> Look {
  return Look{.variant = choices.variant.get(),
              .tint = choices.tint.get(),
              .interactive = choices.interactive.get(),
              .form = choices.form.get(),
              .size = choices.size.get(),
              .grouping = choices.grouping.get(),
              .count = choices.count.get(),
              .spacing = choices.spacing.get()};
}

void GlassPage::apply(const Choices& choices, const Look& look) {
  choices.variant.set(look.variant);
  choices.tint.set(look.tint);
  choices.interactive.set(look.interactive);
  choices.form.set(look.form);
  choices.size.set(look.size);
  choices.grouping.set(look.grouping);
  choices.count.set(look.count);
  choices.spacing.set(look.spacing);
}

auto GlassPage::row(std::string_view title, Element control) -> Element {
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::sm},
             components::Text(std::string{title})
                 .frame({.width = label_width,
                         .alignment = components::Alignment::leading}),
             std::move(control).frame(
                 {.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading}))
      .frame({.max_width = composition::infinity});
}

auto GlassPage::preset_buttons(const Choices& choices, const Look& current)
    -> Element {
  std::vector<Element> buttons;
  buttons.reserve(presets.size());
  for (const Preset& preset : presets) {
    Element button = components::Button(
        std::string{preset.title},
        [choices, look = preset.look] { apply(choices, look); });
    if (preset.look == current) {
      button = std::move(button).button_style(
          components::GlassProminentButtonStyle{});
    } else {
      button = std::move(button).button_style(components::GlassButtonStyle{});
    }
    buttons.push_back(std::move(button).id(preset.key));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::lg,
                 .distribution = components::Distribution::start,
                 .wrap = true},
             std::move(buttons))
      .frame({.max_width = composition::infinity});
}

auto GlassPage::swatch(const Choices& choices, std::size_t index) -> Element {
  const Tint shade = tints.at(index);
  const std::optional<theme::ColourValue> colour =
      GlassPreview::tint_colour(shade);
  Element dot =
      colour.has_value()
          ? Element{components::ShapeView(theme::Capsule()).fill(*colour)}
          : components::ZStack()
                .background(theme::ColourRole::control_background,
                            theme::Capsule())
                .border(theme::ColourRole::tertiary_label, 1.5F);
  Element cell =
      components::ZStack(std::move(dot).frame(
                             {.width = swatch_extent, .height = swatch_extent}))
          .frame({.width = swatch_cell, .height = swatch_cell})
          .clip_shape(theme::Capsule());
  if (choices.tint.get() == shade) {
    cell = std::move(cell).border(theme::ColourRole::label, 2.0F);
  }
  const composition::StateHandle<Tint> chosen_tint = choices.tint;
  return components::Button([chosen_tint, shade] { chosen_tint.set(shade); },
                            std::move(cell))
      .button_style(components::PlainButtonStyle{})
      .content_shape(composition::ContentShapeKinds::focus_effect,
                     theme::Capsule())
      .id(swatch_views.at(index));
}

auto GlassPage::swatches(const Choices& choices) -> Element {
  const auto line = [&choices](std::size_t first, std::size_t last) {
    std::vector<Element> dots;
    for (std::size_t index = first; index < last; ++index) {
      dots.push_back(swatch(choices, index));
    }
    return components::HStack(
        components::HStackOptions{
            .alignment = components::VerticalAlignment::center,
            .spacing = theme::Space{swatch_gap},
            .distribution = components::Distribution::start},
        std::move(dots));
  };
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::Space{swatch_gap}},
      line(0, 6),
      line(6, tints.size())
          .padding(theme::Insets{.leading = swatch_cell + swatch_gap}));
}

auto GlassPage::form_icon(Form shape) -> Element {
  composition::Size icon{.width = 22.0F, .height = 12.0F};
  if (shape == Form::circle) {
    icon = {.width = 13.0F, .height = 13.0F};
  } else if (shape == Form::panel) {
    icon = {.width = 20.0F, .height = 14.0F};
  }
  theme::Shape outline = GlassPreview::outline(shape, Size::small);
  if (shape == Form::rounded) {
    outline =
        theme::RoundedRectangle(3.0F, theme::RoundedCornerStyle::continuous);
  } else if (shape == Form::panel) {
    outline =
        theme::RoundedRectangle(5.0F, theme::RoundedCornerStyle::continuous);
  }
  return components::ShapeView(outline).fill().frame(
      {.width = icon.width, .height = icon.height});
}

auto GlassPage::panel(const Choices& choices, const Look& current) -> Element {
  const bool grouped = current.grouping != Grouping::single;
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 preset_buttons(choices, current),
                 row("Variant",
                     controls::Picker("Variant", choices.variant,
                                      [] {
                                        return components::Group(
                                            components::Text("Regular").tag(
                                                Variant::regular),
                                            components::Text("Clear").tag(
                                                Variant::clear),
                                            components::Text("Identity")
                                                .tag(Variant::identity));
                                      })
                         .picker_style(controls::SegmentedPickerStyle{})),
                 row("Tint", swatches(choices)),
                 controls::Toggle("Interactive", choices.interactive),
                 row("Shape",
                     controls::Picker(
                         "Shape", choices.form,
                         [] {
                           return components::Group(
                               form_icon(Form::capsule).tag(Form::capsule),
                               form_icon(Form::rounded).tag(Form::rounded),
                               form_icon(Form::circle).tag(Form::circle),
                               form_icon(Form::panel).tag(Form::panel));
                         })
                         .picker_style(controls::SegmentedPickerStyle{})),
                 row("Size",
                     controls::Picker(
                         "Size", choices.size,
                         [] {
                           return components::Group(
                               components::Text("Small").tag(Size::small),
                               components::Text("Medium").tag(Size::medium),
                               components::Text("Large").tag(Size::large));
                         })
                         .picker_style(controls::SegmentedPickerStyle{})),
                 row("Container",
                     controls::Picker(
                         "Container", choices.grouping,
                         [] {
                           return components::Group(
                               components::Text("Off").tag(Grouping::single),
                               components::Text("On").tag(Grouping::container),
                               components::Text("Union").tag(Grouping::united));
                         })
                         .picker_style(controls::SegmentedPickerStyle{})),
                 row("Shapes",
                     controls::Picker("Shapes", choices.count,
                                      [] {
                                        return components::Group(
                                            components::Text("Two").tag(2),
                                            components::Text("Three").tag(3));
                                      })
                         .picker_style(controls::SegmentedPickerStyle{}))
                     .disabled(!grouped),
                 row("Spacing",
                     controls::Slider(
                         choices.spacing,
                         controls::ClosedRange<double>{
                             .lower_bound = 0.0, .upper_bound = spacing_limit},
                         spacing_step))
                     .disabled(!grouped))
                 .padding(theme::space::lg)
                 .frame({.max_width = composition::infinity,
                         .alignment = composition::Alignment::leading})
                 .background(theme::Material::regular,
                             theme::RoundedRectangle(
                                 theme::radius::card,
                                 theme::RoundedCornerStyle::continuous)))
      .frame({.width = panel_width, .max_height = composition::infinity})
      .flex_shrink(0.0F)
      .id(panel_view);
}

auto GlassPage::body(composition::Context& context) const -> Element {
  const Choices choices = chosen(context);
  const Look current = look(choices);
  return components::ScrollView(
             components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::top,
                     .spacing = theme::space::lg},
                 panel(choices, current),
                 Element{GlassPreview{current, photo_}}.frame(
                     {.max_width = composition::infinity,
                      .max_height = composition::infinity}))
                 .frame({.max_width = composition::infinity,
                         .max_height = composition::infinity,
                         .alignment = composition::Alignment::top}))
      .scroll_disabled()
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity});
}

}
