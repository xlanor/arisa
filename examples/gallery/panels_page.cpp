#include <panels_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/qr_code.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/structure/callout_board.hpp>
#include <arisa/structure/disclosure_row.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/pin_field.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tile_row.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <format>
#include <string_view>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;

auto PanelsPage::panel_key(components::PanelRadius radius) -> std::string {
  return std::format("panels-radius-{}",
                     components::GlassPanel::points(radius));
}

auto PanelsPage::card(std::string_view title, Element content) -> Element {
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
          theme::ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous))
      .border(theme::ColourRole::separator, theme::Stroke::hairline());
}

auto PanelsPage::panels() -> Element {
  std::vector<Element> shown;
  for (const components::PanelRadius radius : components::all_panel_radii) {
    const auto points = components::GlassPanel::points(radius);
    shown.push_back(components::Text(std::format("{} pt", points))
                        .font(theme::Font::callout)
                        .frame({.width = panel_width, .height = 64.0F})
                        .glass_panel(radius)
                        .id(panel_key(radius)));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      std::move(shown));
}

auto PanelsPage::form(composition::StateHandle<bool> shown,
                      composition::StateHandle<bool> felt) -> Element {
  return structure::Form(
             structure::Section(
                 "Stream",
                 controls::Toggle("Statistics overlay",
                                  composition::Binding<bool>{shown}),
                 components::LabeledContent("Resolution", "1080p"),
                 components::LabeledContent("Frame rate", "60 fps")),
             structure::Section(
                 "Controller",
                 controls::Toggle("Rumble", composition::Binding<bool>{felt}),
                 components::LabeledContent("Layout", "Standard"),
                 structure::DisclosureRow("Button mapping", "Default", [] {})
                     .id(disclosure_key)))
      .form_style(structure::FormStyle::cards())
      .frame({.height = form_height, .max_width = infinity})
      .id(form_key);
}

auto PanelsPage::progress(composition::StateHandle<std::size_t> current)
    -> Element {
  const std::size_t at = current.get();
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg},
      controls::ProgressView(static_cast<double>(at),
                             static_cast<double>(steps))
          .progress_view_style(controls::ProgressViewStyle::segmented(steps))
          .id(progress_key),
      components::Button(
          "Next step",
          [current] { current.set((current.get() + 1) % (steps + 1)); })
          .button_style(components::ButtonStyle::bordered())
          .id(advance_key));
}

auto PanelsPage::dock(composition::StateHandle<std::size_t> selection,
                      composition::StateHandle<int> count) -> Element {
  const auto items = [] {
    return std::vector<structure::DockBar::Item>{
        {.title = "Display", .system_image = "tune"},
        {.title = "Sound", .system_image = "article"},
        {.title = "Controls", .system_image = "sports_esports"},
        {.title = "Network", .system_image = "explore"}};
  };
  const auto trailing = [count] {
    return std::vector<structure::DockBar::Action>{
        {.title = "Settings", .system_image = "settings", .perform = [count] {
           count.set(count.get() + 1);
         }}};
  };
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      Element{structure::DockBar(composition::Binding<std::size_t>{selection},
                                 items(), trailing())}
          .id(dock_key),
      Element{structure::DockBar(composition::Binding<std::size_t>{selection},
                                 items())}
          .labels_hidden()
          .id(compact_dock_key));
}

auto PanelsPage::rails(composition::StateHandle<std::size_t> selection)
    -> Element {
  const auto items = [] {
    return std::vector<structure::DockBar::Item>{
        {.title = "Consoles", .system_image = "sports_esports"},
        {.title = "Library", .system_image = "apps"}};
  };
  return components::HStack(
      components::HStackOptions{.alignment = components::VerticalAlignment::top,
                                .spacing = theme::space::lg},
      Element{
          structure::DockBar(
              composition::Binding<std::size_t>{selection}, items(),
              {{.title = "Settings",
                .system_image = "settings",
                .perform = [] {}}},
              structure::DockBar::Options{.axis = composition::Axis::vertical})}
          .frame({.height = rail_height})
          .id(rail_key),
      Element{structure::DockBar(composition::Binding<std::size_t>{selection},
                                 items())}
          .control_size(components::ControlSize::small)
          .id(small_dock_key));
}

auto PanelsPage::callouts() -> Element {
  const auto chip = [](std::string text) {
    return Element{components::Text(std::move(text))
                       .font(theme::Font::callout)
                       .padding(theme::Insets::symmetric(theme::space::sm,
                                                         theme::space::xs))
                       .glass_panel(components::PanelRadius::small)};
  };
  std::vector<structure::CalloutBoard::Callout> placed;
  placed.push_back({.chip = chip("L1"),
                    .anchor = {.x = 0.22F, .y = 0.12F},
                    .side = structure::CalloutBoard::Side::leading});
  placed.push_back({.chip = chip("D-pad"),
                    .anchor = {.x = 0.25F, .y = 0.45F},
                    .side = structure::CalloutBoard::Side::leading});
  placed.push_back({.chip = chip("R1"),
                    .anchor = {.x = 0.78F, .y = 0.12F},
                    .side = structure::CalloutBoard::Side::trailing});
  placed.push_back({.chip = chip("Cross"),
                    .anchor = {.x = 0.75F, .y = 0.55F},
                    .side = structure::CalloutBoard::Side::trailing});
  return Element{
      structure::CalloutBoard(
          components::Image(components::Image::SystemName{"sports_esports"})
              .font(theme::Font::system(140.0F))
              .shadow(
                  theme::ColourValue{theme::Colour{0.0F, 0.0F, 0.0F, 0.35F}},
                  10.0F, 0.0F, 8.0F),
          std::move(placed),
          structure::CalloutBoard::Options{
              .art = {.width = 220.0F, .height = 160.0F},
              .column_width = 110.0F,
              .gap = 28.0F})}
      .id(callout_key);
}

auto PanelsPage::qr_code() -> Element {
  static constexpr std::array<std::string_view, 21> rows{
      "#######..##.#.#######",
      "#.....#.#..#..#.....#",
      "#.###.#.#..##.#.###.#",
      "#.###.#.#.##..#.###.#",
      "#.###.#.#..#..#.###.#",
      "#.....#.##....#.....#",
      "#######.#.#.#.#######",
      ".........#.#.........",
      "..#..#########.#####.",
      ".#..##.#..####.#...##",
      "####..#..#.##..###..#",
      ".#.....#...#.###.#.#.",
      "#.....##.##....###.##",
      "........#.##......#.#",
      "#######.###.###...#.#",
      "#.....#.#..#....##.#.",
      "#.###.#..###.#......#",
      "#.###.#..#...........",
      "#.###.#.####...##..##",
      "#.....#..##.####.#...",
      "#######..#.#...###..#"};
  std::vector<bool> modules;
  modules.reserve(rows.size() * rows.size());
  for (const std::string_view row : rows) {
    for (const char module : row) {
      modules.push_back(module == '#');
    }
  }
  std::optional<components::QrCode::Matrix> matrix =
      components::QrCode::Matrix::create(rows.size(), std::move(modules));
  if (!matrix.has_value()) {
    return components::Text("No code");
  }
  return Element{components::QrCode(std::move(*matrix))}
      .frame({.width = qr_extent, .height = qr_extent})
      .id(qr_key);
}

auto PanelsPage::pin(composition::StateHandle<std::string> code) -> Element {
  return Element{structure::PinField(pin_length,
                                     composition::Binding<std::string>{code})}
      .id(pin_key);
}

auto PanelsPage::tiles() -> Element {
  return Element{
      structure::TileRow(
          {{.title = "Remote Play",
            .system_image = "sports_esports",
            .hint = "Stream from a console on your network.",
            .action = [] {},
            .prominent = true},
           {.title = "Cloud gaming in the browser",
            .system_image = "cloud",
            .hint = "Play streaming titles from the cloud.",
            .action = [] {},
            .prominent = false},
           {.title = "Couch",
            .system_image = "group",
            .hint = "Share a seat with a friend.",
            .action = [] {},
            .prominent = false}},
          structure::TileRow::Options{.tile_width = 168.0F,
                                      .tile_height = 112.0F,
                                      .default_focus = std::nullopt,
                                      .label_font = theme::Font::subheadline,
                                      .label_lines = 2})}
      .id(tiles_key);
}

auto PanelsPage::body(composition::Context& context) const -> Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 card("Tab dock", dock(context.state(dock_selection),
                                       context.state(opened))),
                 card("Glass panels", panels()),
                 card("Segmented progress", progress(context.state(step))),
                 card("Help",
                      components::Button("Quality", [] {})
                          .button_style(components::ButtonStyle::bordered())
                          .help(std::string{help_title}, std::string{help_body})
                          .id(help_key)),
                 card("Callout board", callouts()),
                 card("Card form",
                      form(context.state(stats), context.state(haptics))),
                 card("Rail and small dock",
                      rails(context.state(dock_selection))),
                 card("QR code", qr_code()),
                 card("PIN field", pin(context.state(passcode))),
                 card("Tile row", tiles()))
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .content_margins(theme::space::sm)
      .frame({.max_width = infinity, .max_height = infinity});
}

}
