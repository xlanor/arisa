#include <rich_text_page.hpp>

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto RichTextPage::face_key(std::string_view name) -> std::string {
  return std::format("rich-text-face-{}", name);
}

auto RichTextPage::log_key(std::size_t index) -> std::string {
  return std::format("rich-text-log-{}", index);
}

auto RichTextPage::section(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto RichTextPage::notes() -> Element {
  return components::Text::markdown(release_notes)
      .frame(
          {.max_width = 520.0F, .alignment = composition::Alignment::leading})
      .id(std::string{notes_view});
}

auto RichTextPage::log() -> Element {
  std::vector<Element> lines;
  for (std::size_t index = 0; index < log_lines.size(); ++index) {
    lines.push_back(components::Text(std::string{log_lines.at(index)})
                        .font(theme::Font::footnote)
                        .monospaced()
                        .id(log_key(index)));
  }
  return VStack(alignment::leading, spacing(theme::space::xxs),
                padding(theme::space::sm),
                background(ColourRole::secondary_system_fill,
                           RoundedRectangle(theme::radius::control)),
                id(std::string{log_view}))(std::move(lines));
}

auto RichTextPage::prices() -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(alignment::first_text_baseline, spacing(theme::space::sm),
             id(std::string{price_view}))(
          components::Text("$59.99").strikethrough().foreground_style(
              ColourRole::text_secondary),
          components::Text("$29.99").font(theme::Font::title3).bold()),
      (components::Text("Deluxe Edition ") +
       components::Text("$59.99").strikethrough() + components::Text(" ") +
       components::Text("$29.99").bold().foreground_color(ColourRole::accent))
          .id(std::string{joined_view}));
}

auto RichTextPage::faces() -> Element {
  using theme::Font;
  const auto sample = [](std::string_view name, components::Text text) {
    return Element{std::move(text).id(face_key(name))};
  };
  return VStack(alignment::leading, spacing(theme::space::xs),
                id(std::string{faces_view}))(
      sample("standard", components::Text("Standard Inter")),
      sample("italic", components::Text("Italic Inter").italic()),
      sample("serif",
             components::Text("Serif design").font_design(Font::Design::serif)),
      sample("rounded", components::Text("Rounded design")
                            .font_design(Font::Design::rounded)),
      sample("monospaced", components::Text("Monospaced design")
                               .font_design(Font::Design::monospaced)),
      sample("condensed", components::Text("Condensed width")
                              .font_width(Font::Width::condensed)),
      sample(
          "expanded",
          components::Text("Expanded width").font_width(Font::Width::expanded)),
      sample("secondary", components::Text("Secondary scale")
                              .text_scale(composition::TextScale::secondary)),
      sample("underline", components::Text("Underlined").underline()),
      components::Text("Custom face")
          .font(Font::custom(custom_face, 28.0F))
          .id(std::string{custom_view}));
}

auto RichTextPage::body(composition::Context&) const -> Element {
  const auto shown = shown_;
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
          section("Release notes", notes()), section("Log", log()),
          section("Price", prices()), section("Faces", faces())));
}

auto RichTextPage::Link::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return Button(id(std::string{button}), button_style(ButtonStyle::bordered()),
                action([shown] { shown.set(true); }),
                full_screen_cover(shown, [shown] {
                  return Element{RichTextPage{shown}};
                }))("Rich text");
}

}
