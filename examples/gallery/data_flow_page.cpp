#include <data_flow_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/link.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

const composition::PreferenceKey<DataFlowPage::Selection>
    DataFlowPage::selection{"gallery.selection", std::nullopt,
                            [](Selection& value, Selection next) {
                              if (!value.has_value()) {
                                value = next;
                              }
                            }};

auto DataFlowPage::tile_key(int index) -> std::string {
  return std::format("data-flow-tile-{}", index);
}

auto DataFlowPage::section(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::md)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(
          ColourRole::surface,
          theme::RoundedRectangle(theme::radius::card,
                                  theme::RoundedCornerStyle::continuous));
}

auto DataFlowPage::Inbox::body(composition::Context& context) const -> Element {
  const auto unread = context.state(unread_key);
  std::vector<Element> links;
  for (int index = 1; index <= 3; ++index) {
    links.push_back(Element{
        presentation::NavigationLink(index, std::format("Message {}", index))}
                        .id(std::format("data-flow-message-{}", index)));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             std::move(links))
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .navigation_title("Inbox")
      .toolbar(composition::ToolbarItem(
          components::Button(
              std::format("Unread {}", unread.get()),
              [unread] { unread.set(unread.get() > 0 ? unread.get() - 1 : 3); })
              .id(std::string{unread_button})))
      .navigation_destination(composition::for_type<int>, [](const int& index) {
        return components::Text(std::format("Message {}", index))
            .font(theme::Font::title3)
            .navigation_title(std::format("Message {}", index));
      });
}

auto DataFlowPage::titles() -> Element {
  return presentation::NavigationStack(
             [] { return components::VStack(Element{Inbox{}}); })
      .frame({.height = stack_height, .max_width = infinity})
      .clip_shape(theme::RoundedRectangle(theme::radius::control));
}

auto DataFlowPage::Tile::body(composition::Context& context) const -> Element {
  const bool focused = context.environment(components::Environment::is_focused);
  Element label =
      components::Text(focused ? "Focused" : std::format("Tile {}", index_ + 1))
          .frame({.width = tile_width, .height = tile_height});
  if (focused) {
    label = std::move(label).anchor_preference(
        selection, composition::AnchorSource<composition::Rect>::bounds(),
        [](const composition::Anchor<composition::Rect>& anchor) {
          return Selection{anchor};
        });
  }
  return label;
}

auto DataFlowPage::marker(const Selection& chosen) -> Element {
  return components::GeometryReader(
      [chosen](const composition::GeometryProxy& proxy) -> Element {
        if (!chosen.has_value()) {
          return components::ZStack();
        }
        const composition::Rect at = proxy[*chosen];
        return components::ZStack()
            .frame({.width = at.width, .height = 4.0F})
            .background(ColourRole::accent, theme::Capsule())
            .id(std::string{marker_view})
            .position(at.x + (at.width * 0.5F), at.y + at.height + 8.0F)
            .allows_hit_testing(false);
      });
}

auto DataFlowPage::tiles() -> Element {
  std::vector<Element> row;
  for (int index = 0; index < tile_count; ++index) {
    row.push_back(components::Button([] {}, Tile{index}).id(tile_key(index)));
  }
  return components::HStack(
             components::HStackOptions{.spacing = theme::space::md},
             std::move(row))
      .padding(theme::Insets{.top = theme::space::xs,
                             .leading = theme::space::none,
                             .bottom = theme::space::lg,
                             .trailing = theme::space::none})
      .focus_section()
      .id(std::string{tiles_view})
      .overlay_preference_value(
          selection, [](const Selection& chosen) { return marker(chosen); });
}

auto DataFlowPage::stored(composition::Context& context) -> Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      Element{controls::Toggle("Show hints", context.app_storage(hints_key))}
          .frame({.width = 360.0F})
          .id(std::string{hints_toggle}),
      Element{controls::Picker("Tab", context.scene_storage(tab_key),
                               [] {
                                 return components::Group(
                                     components::Text("One").tag(0),
                                     components::Text("Two").tag(1),
                                     components::Text("Three").tag(2));
                               })}
          .picker_style(controls::SegmentedPickerStyle{})
          .frame({.width = 360.0F})
          .id(std::string{tab_picker}));
}

auto DataFlowPage::Traits::row(std::string_view name, std::string value)
    -> Element {
  return components::GridRow(components::Text(std::string{name})
                                 .foreground_style(ColourRole::text_secondary),
                             components::Text(std::move(value)));
}

auto DataFlowPage::Traits::body(composition::Context& context) const
    -> Element {
  using components::Environment;
  const auto size_class = [](components::UserInterfaceSizeClass value) {
    return std::string{value == components::UserInterfaceSizeClass::compact
                           ? "compact"
                           : "regular"};
  };
  const auto mode = [](provider::DisplayMode value) {
    switch (value) {
      case provider::DisplayMode::handheld:
        return std::string{"handheld"};
      case provider::DisplayMode::docked:
        return std::string{"docked"};
      case provider::DisplayMode::desktop:
        break;
    }
    return std::string{"desktop"};
  };
  const auto control = [](components::ControlSize value) {
    switch (value) {
      case components::ControlSize::mini:
        return std::string{"mini"};
      case components::ControlSize::small:
        return std::string{"small"};
      case components::ControlSize::regular:
        break;
      case components::ControlSize::large:
        return std::string{"large"};
      case components::ControlSize::extra_large:
        return std::string{"extra large"};
    }
    return std::string{"regular"};
  };
  return components::Grid(
      components::GridOptions{.alignment = composition::Alignment::leading,
                              .horizontal_spacing = theme::space::lg,
                              .vertical_spacing = theme::space::xs},
      row("Display mode", mode(context.environment(Environment::display_mode))),
      row("Display scale",
          std::format("{:.2f}",
                      context.environment(Environment::display_scale))),
      row("Pixel length",
          std::format("{:.3f}",
                      context.environment(Environment::pixel_length))),
      row("Horizontal size class",
          size_class(context.environment(Environment::horizontal_size_class))),
      row("Vertical size class",
          size_class(context.environment(Environment::vertical_size_class))),
      row("Time zone",
          context.environment(Environment::time_zone).identifier()),
      row("Calendar",
          std::string{context.environment(Environment::calendar).name()}),
      row("Control size",
          control(context.environment(Environment::control_size))),
      row("Luminance reduced",
          context.environment(Environment::is_luminance_reduced) ? "yes"
                                                                 : "no"));
}

auto DataFlowPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 components::Button("Close", [shown] { shown.set(false); })
                     .button_style(components::ButtonStyle::bordered())
                     .id(std::string{close_button}),
                 section("Titles from nested views", titles()),
                 section("Anchor preference", tiles()),
                 components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::top,
                         .spacing = theme::space::md},
                     section("Stored values", stored(context)),
                     section("Link",
                             Element{components::Link("Open example.com",
                                                      "https://example.com")}
                                 .id(std::string{link_view})))
                     .frame({.max_width = infinity}),
                 section("Environment", Element{Traits{}}))
                 .padding(theme::space::lg)
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::top_leading}))
      .frame({.max_width = infinity, .max_height = infinity})
      .background(ColourRole::system_grouped_background,
                  theme::Shape::rectangle());
}

auto DataFlowPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Data flow", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{DataFlowPage{shown}}; });
}

}
