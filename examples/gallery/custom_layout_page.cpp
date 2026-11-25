#include <custom_layout_page.hpp>

#include <arisa/components/any_layout.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack_layout.hpp>
#include <arisa/components/subviews_collection.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/view_that_fits.hpp>
#include <arisa/components/vstack_layout.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto CustomLayoutPage::Flow::size_that_fits(
    components::ProposedViewSize proposal,
    const components::LayoutSubviews& subviews) const -> composition::Size {
  const float limit = proposal.width.value_or(infinity);
  float x = 0.0F;
  float y = 0.0F;
  float line = 0.0F;
  float widest = 0.0F;
  for (const components::LayoutSubview& subview : subviews) {
    const composition::Size size =
        subview.size_that_fits(components::ProposedViewSize::unspecified);
    if (x > 0.0F && x + size.width > limit) {
      y += line + spacing_;
      x = 0.0F;
      line = 0.0F;
    }
    x += size.width + spacing_;
    widest = std::max(widest, x - spacing_);
    line = std::max(line, size.height);
  }
  return composition::Size{.width = proposal.width.value_or(widest),
                           .height = y + line};
}

void CustomLayoutPage::Flow::place_subviews(
    composition::Rect bounds, components::ProposedViewSize,
    const components::LayoutSubviews& subviews) const {
  float x = bounds.x;
  float y = bounds.y;
  float line = 0.0F;
  for (const components::LayoutSubview& subview : subviews) {
    const composition::Size size =
        subview.size_that_fits(components::ProposedViewSize::unspecified);
    if (x > bounds.x && x + size.width > bounds.x + bounds.width) {
      y += line + spacing_;
      x = bounds.x;
      line = 0.0F;
    }
    subview.place(composition::Point{.x = x, .y = y});
    x += size.width + spacing_;
    line = std::max(line, size.height);
  }
}

auto CustomLayoutPage::tag_key(std::size_t index) -> std::string {
  return std::format("custom-layout-tag-{}", index);
}

auto CustomLayoutPage::card_key(std::size_t index) -> std::string {
  return std::format("custom-layout-card-{}", index);
}

auto CustomLayoutPage::field_key(std::size_t index) -> std::string {
  return std::format("custom-layout-field-{}", index);
}

auto CustomLayoutPage::variant_key(std::string_view view,
                                   std::string_view variant) -> std::string {
  return std::format("{}-{}", view, variant);
}

auto CustomLayoutPage::section(std::string_view title, Element content)
    -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto CustomLayoutPage::tags() -> Element {
  std::vector<Element> chips;
  chips.reserve(tag_names.size());
  for (std::size_t index = 0; index < tag_names.size(); ++index) {
    chips.push_back(Button(id(tag_key(index)),
                           button_style(ButtonStyle::plain()),
                           action([] {}))(Text(
        font::callout,
        padding(theme::Insets::symmetric(theme::space::md, theme::space::xs)),
        background(ColourRole::secondary_system_fill, theme::Capsule()))(
        std::string{tag_names.at(index)})));
  }
  return components::AnyLayout(Flow{8.0F})(std::move(chips))
      .frame({.max_width = flow_width})
      .id(std::string{flow_view});
}

auto CustomLayoutPage::title(std::string_view view) -> Element {
  return components::ViewThatFits(
      composition::Axes::horizontal,
      Text(font::title3, line_limit(1),
           id(variant_key(view, "long")))("Kingdom of the Lost Lantern"),
      Text(font::title3, line_limit(1),
           id(variant_key(view, "short")))("Lost Lantern"),
      Text(font::title3, line_limit(1), id(variant_key(view, "tiny")))("LL"));
}

auto CustomLayoutPage::cards(bool across,
                             composition::StateHandle<bool> flipped)
    -> Element {
  std::vector<Element> shown;
  for (std::size_t index = 0; index < 3; ++index) {
    shown.push_back(
        VStack(alignment::leading, spacing(theme::space::xs),
               padding(theme::space::md),
               frame({.max_width = infinity,
                      .alignment = composition::Alignment::leading}),
               background(ColourRole::surface_raised,
                          RoundedRectangle(theme::radius::control)),
               id(card_key(index)))(
            title(card_key(index)),
            Text(font::caption, foreground_style(ColourRole::text_secondary))(
                std::format("Chapter {}", index + 1))));
  }
  const components::AnyLayout layout =
      across ? components::AnyLayout(
                   components::HStackLayout(components::HStackOptions{
                       .alignment = components::VerticalAlignment::top,
                       .spacing = theme::space::md}))
             : components::AnyLayout(
                   components::VStackLayout(components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::md}));
  return VStack(alignment::leading, spacing(theme::space::sm),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      Button(id(std::string{switch_button}),
             button_style(ButtonStyle::bordered()), action([flipped] {
               motion::with_animation(
                   motion::Animation::smooth(0.35),
                   [&flipped] { flipped.set(!flipped.get()); });
             }))(across ? "Stack" : "Row"),
      layout(std::move(shown))
          .frame({.max_width = infinity,
                  .alignment = composition::Alignment::leading})
          .animation(motion::Animation::smooth(0.35), across)
          .id(std::string{cards_view}));
}

auto CustomLayoutPage::titles() -> Element {
  return HStack(alignment::top, spacing(theme::space::md))(
      VStack(alignment::leading, padding(theme::space::sm),
             frame({.width = wide_title,
                    .alignment = composition::Alignment::leading}),
             background(ColourRole::surface_raised,
                        RoundedRectangle(theme::radius::control)),
             id(std::string{wide_view}))(title(wide_view)),
      VStack(alignment::leading, padding(theme::space::sm),
             frame({.width = narrow_title,
                    .alignment = composition::Alignment::leading}),
             background(ColourRole::surface_raised,
                        RoundedRectangle(theme::radius::control)),
             id(std::string{narrow_view}))(title(narrow_view)));
}

auto CustomLayoutPage::form() -> Element {
  std::vector<Element> rows;
  for (std::size_t index = 0; index < field_names.size(); ++index) {
    rows.push_back(HStack(spacing(theme::space::sm))(
        Text(font::body, foreground_style(ColourRole::text_secondary))(
            std::string{field_names.at(index)}),
        Text(font::body,
             id(field_key(index)))(std::string{field_values.at(index)})
            .alignment_guide(colon, [](const composition::ViewDimensions& d) {
              return d[composition::HorizontalAlignment::leading];
            })));
  }
  return components::VStack(
             components::VStackOptions{.spacing = theme::space::xs,
                                       .guide = colon},
             std::move(rows))
      .id(std::string{form_view});
}

auto CustomLayoutPage::highlights() -> Element {
  return HStack(spacing(theme::space::sm),
                id(std::string{highlight_view}))(components::ForEach(
      components::SubviewsCollection{components::Group(
          Text(font::body)("Lantern").container_value(featured, true),
          Text(font::body)("Harbor"),
          Text(font::body)("Summit").container_value(featured, true))},
      [](components::Subview subview) -> Element {
        const bool chosen = subview[featured];
        return std::move(subview)
            .view()
            .padding(
                theme::Insets::symmetric(theme::space::md, theme::space::xs))
            .background(
                chosen ? ColourRole::accent : ColourRole::secondary_system_fill,
                theme::Capsule())
            .foreground_style(chosen ? ColourRole::on_accent
                                     : ColourRole::label);
      }));
}

auto CustomLayoutPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto flipped = context.state(flipped_key);
  const bool docked =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::regular;
  const bool across = docked != flipped.get();
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    background(ColourRole::system_grouped_background,
                               theme::Shape::rectangle()))(
      VStack(alignment::leading, spacing(theme::space::md),
             padding(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top_leading}))(
          Button(id(std::string{close_button}),
                 button_style(ButtonStyle::bordered()),
                 action([shown] { shown.set(false); }))("Close"),
          section("Tag flow", tags()),
          section("Switching layout", cards(across, flipped)),
          section("View that fits", titles()),
          HStack(alignment::top, spacing(theme::space::md),
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading}))(
              section("Aligned form", form()),
              section("Custom container", highlights()))));
}

auto CustomLayoutPage::Link::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return Button(id(std::string{button}), button_style(ButtonStyle::bordered()),
                action([shown] { shown.set(true); }),
                full_screen_cover(shown, [shown] {
                  return Element{CustomLayoutPage{shown}};
                }))("Custom layout");
}

}
