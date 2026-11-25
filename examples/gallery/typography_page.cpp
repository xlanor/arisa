#include <rich_text_page.hpp>
#include <typography_page.hpp>

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto TypographyPage::samples() -> Element {
  struct Sample {
    theme::Font font;
    std::string_view name;
    std::string_view detail;
  };
  constexpr std::array samples{
      Sample{theme::Font::large_title, "Large Title", "26 pt regular display"},
      Sample{theme::Font::title, "Title", "22 pt regular display"},
      Sample{theme::Font::headline, "Headline", "13 pt bold"},
      Sample{theme::Font::body, "Body text reads comfortably at arm's length",
             "13 pt regular"},
      Sample{theme::Font::callout, "Callout", "12 pt regular"},
      Sample{theme::Font::footnote, "Footnote", "10 pt regular"},
      Sample{theme::Font::caption, "Caption", "10 pt regular"}};
  std::vector<Element> rows;
  for (const Sample& sample : samples) {
    rows.push_back(
        components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::center,
                .spacing = theme::space::lg},
            components::Text(std::string{sample.detail})
                .font(theme::Font::caption)
                .foreground_style(ColourRole::text_secondary)
                .frame({.width = theme::ScaledMetric{160.0F,
                                                     theme::TextRole::caption},
                        .alignment = composition::Alignment::leading}),
            components::Text(std::string{sample.name}).font(sample.font))
            .frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             std::move(rows))
      .padding(theme::space::xl)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::low);
}

auto TypographyPage::card(Element content) -> Element {
  return std::move(content)
      .padding(theme::space::xl)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::low);
}

auto TypographyPage::truncations() -> Element {
  struct Sample {
    std::string_view locale;
    std::string_view text;
  };
  constexpr std::array samples{
      Sample{"en", "The quick brown fox jumps over the lazy dog"},
      Sample{"ar",
             "\u0627\u0644\u0625\u0639\u062F\u0627\u062F\u0627\u062A "
             "\u0645\u062D\u0641\u0648\u0638\u0629 "
             "\u0627\u0644\u0625\u0639\u062F\u0627\u062F\u0627\u062A "
             "\u0645\u062D\u0641\u0648\u0638\u0629"},
      Sample{"ja",
             "\u8A2D\u5B9A\u3092\u5909\u66F4\u3057\u307E\u3057\u305F\u3002"
             "\u300C\u6B21\u3078\u300D\u3092\u62BC\u3057\u3066\u304F\u3060"
             "\u3055\u3044"}};
  constexpr std::array modes{composition::TruncationMode::head,
                             composition::TruncationMode::middle,
                             composition::TruncationMode::tail};
  std::vector<Element> rows;
  for (const Sample& sample : samples) {
    std::vector<Element> cells;
    for (const composition::TruncationMode mode : modes) {
      cells.push_back(components::Text(std::string{sample.text})
                          .frame({.width = 150.0F,
                                  .alignment = composition::Alignment::leading})
                          .truncation_mode(mode));
    }
    rows.push_back(
        components::HStack(
            components::HStackOptions{.spacing = theme::space::lg},
            std::move(cells))
            .environment(components::Environment::locale,
                         text::Locale::create(sample.locale)
                             .value_or(text::Locale::undetermined())));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             std::move(rows))
      .line_limit(1);
}

auto TypographyPage::cover(std::uint32_t width, std::uint32_t height)
    -> std::shared_ptr<const provider::Image> {
  std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height *
                                provider::Image::channels);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t at =
          (static_cast<std::size_t>(y) * width + x) * provider::Image::channels;
      const bool edge = x < 4 || y < 4 || x + 4 >= width || y + 4 >= height;
      pixels[at] = std::byte(edge ? 0xF2 : 0x3A);
      pixels[at + 1] = std::byte(edge ? 0x9F : 0x6E);
      pixels[at + 2] = std::byte(edge ? 0x05 : 0xC8);
      pixels[at + 3] = std::byte(0xFF);
    }
  }
  auto made = provider::Image::create(width, height, std::move(pixels));
  if (!made.has_value()) {
    return nullptr;
  }
  return std::make_shared<const provider::Image>(std::move(*made));
}

auto TypographyPage::covers() -> Element {
  static const std::array art{cover(64, 96), cover(120, 60), cover(80, 80)};
  std::vector<Element> tiles;
  for (const components::Image::ContentMode mode :
       {components::Image::ContentMode::fit,
        components::Image::ContentMode::fill}) {
    for (const std::shared_ptr<const provider::Image>& pixels : art) {
      tiles.push_back(
          components::Image::Raster(pixels, mode)
              .frame({.width = cover_width, .height = cover_height})
              .clip_shape(theme::RoundedRectangle(theme::radius::control))
              .background(ColourRole::surface,
                          theme::RoundedRectangle(theme::radius::control)));
    }
  }
  tiles.push_back(
      components::ZStack(
          components::VStack()
              .background(ColourRole::accent)
              .aspect_ratio(16.0F / 9.0F, composition::ContentMode::fit))
          .frame({.width = cover_width, .height = cover_height})
          .background(ColourRole::surface,
                      theme::RoundedRectangle(theme::radius::control)));
  return components::HStack(
             components::HStackOptions{.spacing = theme::space::sm,
                                       .wrap = true},
             std::move(tiles))
      .id(std::string{covers_id});
}

auto TypographyPage::modifiers(composition::StateHandle<int> count) -> Element {
  std::vector<Element> rows;
  rows.push_back(
      components::Text("Text and image fit").font(theme::Font::headline));
  rows.push_back(
      components::VStack(
          components::VStackOptions{.spacing = theme::space::xs},
          components::Text("Centred lines\ninherit their alignment"),
          components::Text("from the stack"))
          .multiline_text_alignment(composition::TextAlignment::center)
          .id(std::string{alignment_id}));
  rows.push_back(components::Text("Three lines reserved")
                     .line_limit(3, true)
                     .padding(theme::space::sm)
                     .border(ColourRole::separator, theme::Stroke::hairline())
                     .id(std::string{reserved_id}));
  rows.push_back(truncations());
  rows.push_back(components::Text("A long title that shrinks to fit")
                     .font(theme::Font::title)
                     .frame({.width = 170.0F,
                             .alignment = composition::Alignment::leading})
                     .line_limit(1)
                     .minimum_scale_factor(0.5F)
                     .id(std::string{shrinking_id}));
  rows.push_back(components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::lg},
      components::Button(
          "Count", [count] { count.set(((count.get() * 7) + 3) % 100000); }),
      components::Text(std::format("{}", count.get()))
          .font(theme::Font::title2)
          .monospaced_digit()
          .id(std::string{counter_id}),
      components::Text("Tracking").tracking(4.0F),
      components::Text("Baseline").baseline_offset(6.0F)));
  rows.push_back(
      components::Button(
          [] {},
          components::Text("A focused tile title that is far too long to fit")
              .marquee())
          .frame({.width = 220.0F})
          .id(std::string{marquee_id}));
  rows.push_back(covers());
  return card(components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      std::move(rows)));
}

auto TypographyPage::body(composition::Context& context) const -> Element {
  const std::optional<appearance::AppearanceModel>& model =
      context.environment(components::Environment::appearance);
  const provider::DisplayMode mode =
      context.environment(components::Environment::display_mode);
  const auto size = context.state(local_size);
  const auto scale = context.state(local_scale);
  Element sizes =
      controls::Stepper(
          std::format(
              "Text size: {}",
              structure::AppearanceSettingsSection::size_label(size.get())),
          size,
          controls::ClosedRange<int>{.lower_bound = 0,
                                     .upper_bound = static_cast<int>(
                                         theme::dynamic_type_size_count - 1)})
          .fixed_size(true, false)
          .id("typography.size");
  Element scales =
      controls::Stepper(std::format("Interface scale: {}%",
                                    std::lround(scale.get() * 100.0F)),
                        scale,
                        controls::ClosedRange<float>{
                            .lower_bound = appearance::UiScales::minimum,
                            .upper_bound = appearance::UiScales::maximum},
                        structure::AppearanceSettingsSection::scale_step)
          .fixed_size(true, false)
          .id("typography.scale");
  if (model.has_value()) {
    const appearance::AppearanceModel held = *model;
    sizes = std::move(sizes).on_change(size.get(), [held, size] {
      held.update(
          [index = size.get()](appearance::AppearanceOverrides& overrides) {
            overrides.dynamic_type_size =
                static_cast<theme::DynamicTypeSize>(index);
          });
    });
    scales = std::move(scales).on_change(scale.get(), [held, scale, mode] {
      held.update([value = scale.get(),
                   mode](appearance::AppearanceOverrides& overrides) {
        overrides.ui_scale = overrides.ui_scale.with(mode, value);
      });
    });
  } else {
    scales = std::move(scales).disabled(true);
  }
  std::vector<Element> controls;
  controls.push_back(std::move(sizes));
  controls.push_back(std::move(scales));
  Element shown = samples();
  if (!model.has_value()) {
    shown = std::move(shown).dynamic_type_size(
        static_cast<theme::DynamicTypeSize>(size.get()));
  }
  Element page = components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::leading,
                         .spacing = theme::space::md},
                     components::HStack(
                         components::HStackOptions{
                             .alignment = components::VerticalAlignment::center,
                             .spacing = theme::space::xl,
                             .wrap = true},
                         std::move(controls)),
                     std::move(shown), modifiers(context.state(local_count)),
                     RichTextPage::Link{})
                     .frame({.max_width = infinity,
                             .alignment = composition::Alignment::leading});
  if (model.has_value()) {
    const appearance::AppearanceSettings& settings =
        context.read(model->settings());
    page = std::move(page).on_change(
        settings, true, [settings, size, scale, mode] {
          size.set(static_cast<int>(settings.dynamic_type_size));
          scale.set(settings.ui_scale.resolve(mode));
        });
  }
  return components::ScrollView(std::move(page))
      .frame({.max_width = infinity, .max_height = infinity});
}

}
