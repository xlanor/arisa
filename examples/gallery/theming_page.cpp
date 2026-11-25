#include <theming_page.hpp>

#include <arisa/appearance/theme_file.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <string>
#include <utility>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto ThemingPage::sample()
    -> std::expected<theme::Theme, appearance::ThemeError> {
  return appearance::ThemeFile::parse(
      sample_file, sample_name,
      [](std::string_view) { return theme::Theme::defaults(); });
}

auto ThemingPage::broken()
    -> std::expected<theme::Theme, appearance::ThemeError> {
  return appearance::ThemeFile::parse(
      broken_file, broken_name,
      [](std::string_view) { return theme::Theme::defaults(); });
}

auto ThemingPage::warm_theme(const theme::Theme& base) -> theme::Theme {
  return base.with_accent(theme::Colour::hex(0xFF9500))
      .with(theme::Theme::Radii{
          .control = 12.0F, .card = 24.0F, .sheet = 28.0F, .continuous = true})
      .named("warm");
}

auto ThemingPage::card(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::low);
}

auto ThemingPage::swatch(std::string_view label, std::string_view marker)
    -> Element {
  Element bar = components::ZStack()
                    .frame({.width = 36.0F, .height = 14.0F})
                    .background(ColourRole::accent,
                                theme::Shape::rounded(theme::radius::control));
  if (!marker.empty()) {
    bar = std::move(bar).id(marker);
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{label}).font(theme::Font::callout),
             components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::center,
                     .spacing = theme::space::sm},
                 std::move(bar),
                 components::Text("Accent")
                     .font(theme::Font::footnote)
                     .foreground_style(ColourRole::tint)))
      .padding(theme::space::md)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          ColourRole::background,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous));
}

auto ThemingPage::files() -> std::array<Element, 2> {
  auto loaded = sample();
  auto failed = broken();
  Element preview =
      loaded.has_value()
          ? swatch("orchard").theme_override(*loaded)
          : Element{components::Text(loaded.error().description())
                        .font(theme::Font::footnote)
                        .foreground_style(ColourRole::destructive)};
  Element reported =
      failed.has_value()
          ? Element{components::Text("Loaded").font(theme::Font::footnote)}
          : Element{components::Text(failed.error().description())
                        .font(theme::Font::footnote)
                        .foreground_style(ColourRole::destructive)
                        .frame({.max_width = infinity,
                                .alignment = composition::Alignment::leading})};
  return {
      card(sample_name, std::move(preview)).id(sample_card),
      card(broken_name, std::move(reported).id(broken_error)).id(broken_card)};
}

auto ThemingPage::scoped(const theme::Theme& base) -> Element {
  return components::HStack(
      components::HStackOptions{.alignment = components::VerticalAlignment::top,
                                .spacing = theme::space::md},
      swatch("Inherited"),
      swatch("theme_override(warm)").theme_override(warm_theme(base)),
      swatch("Dark scope")
          .environment(components::Environment::colour_scheme,
                       std::optional{theme::ColorScheme::dark}));
}

auto ThemingPage::switching(composition::StateHandle<bool> warm) -> Element {
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::md},
      components::Button("Switch theme",
                         [warm] {
                           motion::with_animation(
                               motion::Animation::smooth(0.6),
                               [warm] { warm.set(!warm.get()); });
                         })
          .id(switch_button),
      swatch(warm.get() ? "Warm" : "Default", switch_accent).id(switch_swatch));
}

auto ThemingPage::buttons() -> Element {
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::md},
      components::Button("Glass", [] {})
          .button_style(components::GlassButtonStyle{}),
      components::Button("Clear", [] {})
          .button_style(components::GlassButtonStyle{theme::Glass::clear()}),
      components::Button("Prominent", [] {
      }).button_style(components::GlassProminentButtonStyle{}));
}

auto ThemingPage::container(composition::Namespace::ID space) -> Element {
  return components::GlassEffectContainer(
      theme::Space{20.0F},
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::sm},
          components::Text("Alone").padding(theme::space::md).glass_effect(),
          components::Text("Tinted")
              .padding(theme::space::md)
              .glass_effect(theme::Glass::regular().tint(ColourRole::accent)),
          components::Text("Union A")
              .padding(theme::space::md)
              .glass_effect(theme::Glass::regular(),
                            theme::Shape::rounded(theme::radius::control))
              .glass_effect_union(composition::ElementKey{"pair"}, space),
          components::Text("Union B")
              .padding(theme::space::md)
              .glass_effect(theme::Glass::regular(),
                            theme::Shape::rounded(theme::radius::control))
              .glass_effect_union(composition::ElementKey{"pair"}, space)));
}

auto ThemingPage::morph(composition::StateHandle<bool> together,
                        composition::Namespace::ID space) -> Element {
  const auto toggle = [together] {
    motion::with_animation(motion::Animation::bouncy(0.5, 0.2),
                           [together] { together.set(!together.get()); });
  };
  Element shapes =
      together.get()
          ? Element{components::Text("Merged")
                        .padding(theme::Insets::symmetric(theme::space::xl,
                                                          theme::space::md))
                        .glass_effect()
                        .glass_effect_id(composition::ElementKey{"blob"},
                                         space)}
          : Element{components::HStack(
                components::HStackOptions{.spacing = theme::space::xl},
                components::Text("One")
                    .padding(theme::space::md)
                    .glass_effect()
                    .glass_effect_id(composition::ElementKey{"blob"}, space),
                components::Text("Two")
                    .padding(theme::space::md)
                    .glass_effect()
                    .glass_effect_id(composition::ElementKey{"other"}, space))};
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::lg},
      components::Button(together.get() ? "Split" : "Merge", toggle)
          .id(morph_button),
      components::GlassEffectContainer(std::move(shapes))
          .frame({.min_height = 56.0F}));
}

auto ThemingPage::body(composition::Context& context) const -> Element {
  const auto together = context.state(merged);
  const composition::Namespace::ID space = context.namespace_id(glass_space);
  const theme::Theme& inherited =
      context.environment(components::Environment::theme);
  auto [sampled, failed] = files();
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 std::move(sampled), std::move(failed),
                 card("Scoped themes", scoped(inherited)),
                 card("Switch the app theme", switching(warm_)),
                 card("Glass buttons", buttons()),
                 card("Glass container", container(space)),
                 card("Glass morph", morph(together, space)))
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .content_margins(theme::space::sm)
      .frame({.max_width = infinity, .max_height = infinity});
}

}
