#include <world_page.hpp>

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/structure/file_browser.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::AppStorage<std::string> WorldPage::language{
    "gallery.language", std::string{}};

auto WorldPage::count_key(int count) -> std::string {
  return std::format("world-count-{}", count);
}

auto WorldPage::say(std::string_view key) -> Element {
  return components::Text(text::LocalizedStringKey{std::string{key}});
}

auto WorldPage::section(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)))(
      components::Text(text::LocalizedStringKey{std::string{title}})
          .font(theme::Font::headline)
          .accessibility_heading(composition::AccessibilityHeadingLevel::h2),
      std::move(content));
}

auto WorldPage::chooser(composition::Context& context) -> Element {
  const composition::Binding<std::string> chosen =
      context.app_storage(language);
  return Element{controls::Picker(
                     "Language", chosen,
                     [] {
                       std::vector<Element> options;
                       for (const Language& each : languages) {
                         options.push_back((each.tag.empty()
                                                ? say(each.name)
                                                : Element{components::Text(
                                                      std::string{each.name})})
                                               .tag(std::string{each.tag}));
                       }
                       return Element{components::Group(std::move(options))};
                     })}
      .id(std::string{language_picker});
}

auto WorldPage::formats() -> Element {
  using Style = text::DateFormatStyle;
  const auto row = [](std::string_view label, Element value,
                      std::string_view key) {
    return Element{components::LabeledContent(std::move(value), say(label))}.id(
        std::string{key});
  };
  std::vector<Element> counts;
  for (const int count : file_counts) {
    counts.push_back(
        components::Text(text::LocalizedStringKey{"{0} files", count})
            .id(count_key(count)));
  }
  return VStack(alignment::leading, spacing(theme::space::xs))(
      row("Number",
          components::Text(sample_number, text::NumberFormatStyle::number()),
          number_view),
      row("Percent",
          components::Text(sample_share, text::NumberFormatStyle::percent()),
          percent_view),
      row("Date",
          components::Text(sample_date, Style{Style::DateStyle::complete,
                                              Style::TimeStyle::omitted}),
          date_view),
      row("Time",
          components::Text(sample_date, components::Text::DateStyle::time),
          time_view),
      row("Relative", components::Text::relative(std::chrono::hours{-3}),
          relative_view),
      HStack(alignment::center, spacing(theme::space::md))(std::move(counts)));
}

auto WorldPage::mirroring(composition::Context& context) -> Element {
  const auto shown = context.state(slider_value);
  const auto cell = [](std::string_view label) {
    return components::Text(std::string{label})
        .frame({.width = 44.0F, .height = 28.0F})
        .background(ColourRole::secondary_system_fill,
                    RoundedRectangle(theme::radius::control));
  };
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(alignment::center, spacing(theme::space::sm),
             id(std::string{mirrored_row}))(
          say("Mirrored"),
          components::Image::system_name("chevron_right")
              .id(std::string{chevron_view}),
          components::Image::system_name("chevron_right")
              .flips_for_right_to_left_layout_direction(true)
              .id(std::string{flipped_view}),
          components::Image::system_name("chevron_right")
              .layout_direction_behavior(
                  composition::LayoutDirectionBehavior::mirrors()),
          cell("1"), cell("2"), cell("3")),
      HStack(alignment::center, spacing(theme::space::sm),
             id(std::string{fixed_row}))(
          say("Left to right"), cell("1").id(std::string{fixed_first}),
          cell("2"), cell("3").id(std::string{fixed_last}))
          .environment(components::Environment::layout_direction,
                       components::LayoutDirection::left_to_right),
      Element{components::Grid(
                  components::GridRow(cell("A"), cell("B"), cell("C")))}
          .id(std::string{grid_view}),
      Element{controls::Slider(composition::Binding<double>{shown})}
          .frame({.max_width = 320.0F})
          .id(std::string{slider_view}));
}

auto WorldPage::accessibility() -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm))(
      components::Text(text::LocalizedStringKey{"Accessibility"})
          .accessibility_heading(composition::AccessibilityHeadingLevel::h3)
          .id(std::string{heading_view}),
      Button(button_style(ButtonStyle::bordered()), action([] {}))("World")
          .accessibility_label("Globe")
          .accessibility_hint("Opens the language list")
          .accessibility_identifier(std::string{described_button}),
      HStack(alignment::center, spacing(theme::space::sm))(
          components::Image::system_name("language"), components::Text("12"),
          say("Files"))
          .accessibility_element(
              composition::AccessibilityChildBehavior::combine)
          .accessibility_identifier(std::string{combined_row}),
      components::Image::system_name("star").accessibility_hidden().id(
          std::string{hidden_icon}));
}

auto WorldPage::appearance(composition::Context& context) -> Element {
  const std::optional<arisa::appearance::AppearanceModel>& model =
      context.environment(components::Environment::appearance);
  if (!model.has_value()) {
    return say("Appearance");
  }
  const arisa::appearance::AppearanceSettings& settings =
      context.read(model->settings());
  const auto toggle =
      [held = *model](
          std::string_view title, bool on,
          std::optional<bool> arisa::appearance::AppearanceOverrides::* slot) {
        return Element{controls::Toggle(
            composition::Binding<bool>{
                [on] { return on; },
                [held, slot](bool value) {
                  held.update(
                      [slot, value](arisa::appearance::AppearanceOverrides& o) {
                        o.*slot = value;
                      });
                }},
            components::Text(text::LocalizedStringKey{std::string{title}}))};
      };
  using Overrides = arisa::appearance::AppearanceOverrides;
  return VStack(alignment::leading, spacing(theme::space::xs))(
      toggle("Bold text", settings.bold_text, &Overrides::bold_text),
      toggle("Button shapes", settings.button_shapes,
             &Overrides::button_shapes),
      toggle("Differentiate without colour",
             settings.differentiate_without_color,
             &Overrides::differentiate_without_color),
      toggle("Dim flashing lights", settings.dim_flashing_lights,
             &Overrides::dim_flashing_lights),
      toggle("Increase contrast", settings.increase_contrast,
             &Overrides::increase_contrast),
      Element{components::Button(
                  [held = *model, current = settings.theme] {
                    held.update([current](Overrides& o) {
                      o.theme =
                          current == theme::Theme::high_contrast_name
                              ? std::string{"default"}
                              : std::string{theme::Theme::high_contrast_name};
                    });
                  },
                  say("High contrast"))}
          .button_style(components::ButtonStyle::bordered())
          .id(std::string{contrast_button}));
}

auto WorldPage::files(composition::Context& context) -> Element {
  const auto open = context.state(importing);
  const auto save = context.state(exporting);
  const auto relocate = context.state(moving);
  const auto picked = context.state(imported);
  const auto status = context.state(file_result);
  const std::optional<components::FileStorage>& storage =
      context.environment(components::Environment::file_storage);
  const auto report = [status](const structure::FileBrowser::Result& result,
                               std::string_view verb) {
    status.set(result.has_value() ? std::format("{} {}", verb, result->path())
                                  : result.error().explanation());
  };
  std::vector<std::byte> document;
  for (const char character : export_text) {
    document.push_back(static_cast<std::byte>(character));
  }
  const std::optional<composition::FileURL> source =
      composition::FileURL::create(picked.get());
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(alignment::center, spacing(theme::space::sm))(
          Element{components::Button([open] { open.set(true); }, say("Import"))}
              .button_style(components::ButtonStyle::bordered())
              .id(std::string{import_button})
              .file_importer(composition::Binding<bool>{open},
                             {composition::UTType::plain_text(),
                              composition::UTType{"public.image"}},
                             [picked, report,
                              storage](structure::FileBrowser::Result result) {
                               if (result.has_value()) {
                                 picked.set(std::string{result->path()});
                                 if (storage.has_value()) {
                                   if (auto bytes = storage->read(*result)) {
                                     report(result,
                                            std::format("{} B", bytes->size()));
                                     return;
                                   }
                                 }
                               }
                               report(result, "Import");
                             }),
          Element{components::Button([save] { save.set(true); }, say("Export"))}
              .button_style(components::ButtonStyle::bordered())
              .id(std::string{export_button})
              .file_exporter(
                  composition::Binding<bool>{save}, std::move(document),
                  composition::UTType::plain_text(), std::string{export_name},
                  [report](structure::FileBrowser::Result result) {
                    report(result, "Export");
                  }),
          Element{components::Button([relocate] { relocate.set(true); },
                                     say("Move"))}
              .button_style(components::ButtonStyle::bordered())
              .disabled(!source.has_value())
              .id(std::string{move_button})
              .file_mover(
                  composition::Binding<bool>{relocate}, source,
                  [report, picked](structure::FileBrowser::Result result) {
                    if (result.has_value()) {
                      picked.set(std::string{result->path()});
                    }
                    report(result, "Move");
                  })),
      components::Text(status.get())
          .font(theme::Font::footnote)
          .foreground_style(ColourRole::text_secondary)
          .id(std::string{file_status}));
}

auto WorldPage::applets(composition::Context& context) -> Element {
  const auto status = context.state(applet_result);
  const std::optional<components::SystemApplets>& system =
      context.environment(components::Environment::system_applets);
  const auto failed = [status](const provider::ProviderError& error) {
    status.set(error.explanation());
  };
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(alignment::center, spacing(theme::space::sm))(
          Element{components::Button(
                      [system, status, failed] {
                        if (!system.has_value()) {
                          return;
                        }
                        auto chosen = system->select_account();
                        if (!chosen.has_value()) {
                          failed(chosen.error());
                        } else {
                          status.set(chosen->has_value() ? (*chosen)->nickname
                                                         : std::string{});
                        }
                      },
                      say("Choose user"))}
              .button_style(components::ButtonStyle::bordered())
              .disabled(!system.has_value())
              .id(std::string{user_button}),
          Element{components::Button(
                      [system, status, failed] {
                        if (!system.has_value()) {
                          return;
                        }
                        auto players = system->select_players(
                            provider::PlayerSelection{.minimum = 1,
                                                      .maximum = 4,
                                                      .single_player = false});
                        if (!players.has_value()) {
                          failed(players.error());
                        } else {
                          status.set(std::format("{}", *players));
                        }
                      },
                      say("Players"))}
              .button_style(components::ButtonStyle::bordered())
              .disabled(!system.has_value())
              .id(std::string{players_button}),
          Element{components::Button(
                      [system, failed] {
                        if (!system.has_value()) {
                          return;
                        }
                        if (auto shown =
                                system->show_error(provider::ErrorReport{
                                    .code = 2000,
                                    .message = "arisa gallery error",
                                    .details = {}});
                            !shown.has_value()) {
                          failed(shown.error());
                        }
                      },
                      say("Show error"))}
              .button_style(components::ButtonStyle::bordered())
              .disabled(!system.has_value())
              .id(std::string{error_button})),
      components::Text(status.get())
          .font(theme::Font::footnote)
          .foreground_style(ColourRole::text_secondary)
          .id(std::string{applet_status}));
}

auto WorldPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    background(ColourRole::system_grouped_background,
                               theme::Shape::rectangle()))(
      VStack(alignment::leading, spacing(theme::space::md),
             padding(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top_leading}))(
          Element{
              components::Button([shown] { shown.set(false); }, say("Close"))}
              .button_style(components::ButtonStyle::bordered())
              .id(std::string{close_button}),
          section("Language", chooser(context)), section("Formats", formats()),
          section("Right to left", mirroring(context)),
          section("Accessibility", accessibility()),
          section("Appearance", appearance(context)),
          section("Files", files(context)),
          section("Applets", applets(context))));
}

auto WorldPage::Link::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return Element{
      components::Button([shown] { shown.set(true); }, WorldPage::say("World"))}
      .button_style(components::ButtonStyle::bordered())
      .id(std::string{button})
      .full_screen_cover(composition::Binding<bool>{shown},
                         [shown] { return Element{WorldPage{shown}}; });
}

}
