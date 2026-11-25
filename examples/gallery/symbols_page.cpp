#include <symbols_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/share_destination.hpp>
#include <arisa/components/share_preview.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/controls/number_format.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/share_link.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/slider_tick.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_field_link.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/tuple_slider_tick_content.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;

auto SymbolsPage::framed_pixels() -> std::shared_ptr<const provider::Image> {
  constexpr std::uint32_t extent = 24;
  std::vector<std::byte> bytes;
  bytes.reserve(std::size_t{extent} * extent * provider::Image::channels);
  for (std::uint32_t y = 0; y < extent; ++y) {
    for (std::uint32_t x = 0; x < extent; ++x) {
      const bool rim = x < 6 || y < 6 || x >= extent - 6 || y >= extent - 6;
      const std::array<std::uint8_t, 4> colour =
          rim ? std::array<std::uint8_t, 4>{0x2F, 0x6F, 0xD6, 0xFF}
              : std::array<std::uint8_t, 4>{0xE8, 0xF0, 0xFC, 0xFF};
      for (const std::uint8_t channel : colour) {
        bytes.push_back(static_cast<std::byte>(channel));
      }
    }
  }
  auto made = provider::Image::create(extent, extent, std::move(bytes));
  if (!made.has_value()) {
    return nullptr;
  }
  return std::make_shared<const provider::Image>(std::move(*made));
}

auto SymbolsPage::checker_pixels() -> std::shared_ptr<const provider::Image> {
  constexpr std::uint32_t extent = 8;
  std::vector<std::byte> bytes;
  bytes.reserve(std::size_t{extent} * extent * provider::Image::channels);
  for (std::uint32_t y = 0; y < extent; ++y) {
    for (std::uint32_t x = 0; x < extent; ++x) {
      const bool dark = (x < extent / 2) == (y < extent / 2);
      const std::uint8_t level = dark ? 0x9A : 0xE6;
      for (const std::uint8_t channel :
           std::array<std::uint8_t, 4>{level, level, level, 0xFF}) {
        bytes.push_back(static_cast<std::byte>(channel));
      }
    }
  }
  auto made = provider::Image::create(extent, extent, std::move(bytes));
  if (!made.has_value()) {
    return nullptr;
  }
  return std::make_shared<const provider::Image>(std::move(*made));
}

auto SymbolsPage::section(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto SymbolsPage::buttons(composition::StateHandle<int> pressed) -> Element {
  const auto row = [](auto... items) {
    return components::HStack(
        components::HStackOptions{
            .alignment = components::VerticalAlignment::center,
            .spacing = theme::space::md},
        std::move(items)...);
  };
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      row(components::Button(components::ButtonRole::close, [] {}),
          components::Button(components::ButtonRole::confirm, [] {}),
          components::Button("Add", components::Image::SystemName{"plus"},
                             [] {}),
          components::Button("Delete", components::Image::SystemName{"trash"},
                             components::ButtonRole::destructive, [] {})),
      row(components::Button("Filter", [] {})
              .button_style(components::AccessoryBarButtonStyle{}),
          components::Button(
              "Sort",
              components::Image::SystemName{"line.3.horizontal.decrease"},
              [] {})
              .button_style(components::AccessoryBarActionButtonStyle{}),
          components::Button("Open link", [] {})
              .button_style(components::LinkButtonStyle{}),
          components::Button("Card", [] {})
              .button_style(components::CardButtonStyle{})),
      row(components::Button("Press",
                             [pressed] { pressed.set(pressed.get() + 1); })
              .sensory_feedback(
                  composition::SensoryFeedback::press(
                      composition::SensoryFeedback::PressFeedback::button),
                  pressed.get())
              .id(std::string{press_button}),
          components::Button("Release",
                             [pressed] { pressed.set(pressed.get() + 1); })
              .sensory_feedback(
                  composition::SensoryFeedback::release(
                      composition::SensoryFeedback::ReleaseFeedback::slider),
                  pressed.get())));
}

auto SymbolsPage::menus(composition::StateHandle<int> chosen) -> Element {
  const auto items = [] {
    return Element{components::Group(components::Button("Copy", [] {}),
                                     components::Button("Paste", [] {}))};
  };
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::lg},
      controls::Menu("Edit", items, [chosen] { chosen.set(chosen.get() + 1); })
          .id(std::string{split_menu}),
      controls::Menu("Pull down", items)
          .menu_style(controls::MenuStyle::pull_down()),
      controls::Menu(
          "Share", components::Image::SystemName{"square.and.arrow.up"}, items),
      components::Text(std::format("Primary {}", chosen.get()))
          .foreground_style(theme::ColourRole::text_secondary)
          .id(std::string{primary_label}));
}

auto SymbolsPage::mixed(composition::StateHandle<bool> left,
                        composition::StateHandle<bool> right,
                        composition::StateHandle<Tint> first,
                        composition::StateHandle<Tint> second) -> Element {
  const std::vector<composition::Binding<bool>> switches{
      composition::Binding<bool>{left}, composition::Binding<bool>{right}};
  const auto tints = [] {
    return Element{components::Group(components::Text("Red").tag(Tint::red),
                                     components::Text("Green").tag(Tint::green),
                                     components::Text("Blue").tag(Tint::blue))};
  };
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             controls::Toggle("Select all", switches)
                 .id(std::string{select_all}),
             controls::Toggle("Wi-Fi", components::Image::SystemName{"wifi"},
                              composition::Binding<bool>{left}),
             controls::Toggle("Bluetooth", composition::Binding<bool>{right}),
             controls::Picker("Tint",
                              std::vector<composition::Binding<Tint>>{
                                  composition::Binding<Tint>{first},
                                  composition::Binding<Tint>{second}},
                              tints)
                 .picker_style(controls::PickerStyle::menu()),
             controls::Picker("Tabs", composition::Binding<Tint>{first}, tints)
                 .picker_style(controls::PickerStyle::tabs())
                 .frame({.width = 320.0F}))
      .frame({.width = 420.0F});
}

auto SymbolsPage::sliders(composition::StateHandle<double> level,
                          composition::StateHandle<int> count) -> Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      controls::Slider(
          composition::Binding<double>{level},
          controls::ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
          controls::Slider::Parts{
              .step = 0.1,
              .neutral_value = 0.5,
              .enabled_bounds =
                  controls::ClosedRange<double>{.lower_bound = 0.1,
                                                .upper_bound = 0.9},
              .label = components::Text("Balance"),
              .current_value_label =
                  components::Text(std::format("{:.1f}", level.get())),
              .ticks =
                  controls::TupleSliderTickContent{
                      controls::SliderTick{std::string{"Left"}, 0.0},
                      controls::SliderTick{std::string{"Centre"}, 0.5},
                      controls::SliderTick{std::string{"Right"}, 1.0}}})
          .frame({.width = 420.0F})
          .id(std::string{balance_slider}),
      controls::Slider(
          composition::Binding<double>{level},
          controls::ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
          components::Text("Scrubber"))
          .slider_thumb_visibility(composition::Visibility::hidden)
          .frame({.width = 420.0F}),
      controls::Stepper(
          "Copies", composition::Binding<int>{count},
          controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 20}, 1,
          controls::NumberFormat::number())
          .frame({.width = 420.0F}),
      controls::ProgressView(
          level.get(), 1.0, components::Text("Download"),
          components::Text(std::format(
              "{} of 10", static_cast<int>(level.get() * 10.0 + 0.5))))
          .frame({.width = 420.0F}));
}

auto SymbolsPage::sharing(composition::StateHandle<std::string> name)
    -> Element {
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::lg},
      controls::ShareLink(
          std::string{"Share note"}, std::string{"arisa gallery note"},
          controls::ShareLinkOptions{
              .subject = "Gallery note",
              .message = "Sent from the symbols page",
              .preview =
                  components::SharePreview{
                      "Gallery note",
                      components::Image::system_name("doc.text")}})
          .share_destination(components::ShareDestination{
              "Album", components::Image::SystemName{"photo"},
              [](const components::ShareDestination::Request&) {}})
          .id(std::string{share_link}),
      controls::TextFieldLink(
          "Rename", std::string{"New name"},
          [name](std::string entered) { name.set(std::move(entered)); }),
      components::Text(name.get().empty() ? std::string{"Unnamed"} : name.get())
          .foreground_style(theme::ColourRole::text_secondary));
}

auto SymbolsPage::symbols(composition::StateHandle<double> strength,
                          composition::StateHandle<bool> animating) -> Element {
  constexpr std::array names{"xmark",      "checkmark",
                             "gearshape",  "magnifyingglass",
                             "trash",      "bell",
                             "heart.fill", "star.fill",
                             "folder",     "square.and.arrow.up"};
  std::vector<Element> named;
  for (const char* name : names) {
    named.push_back(
        components::Image::system_name(name).font(theme::Font::title2));
  }
  const auto modes = [](composition::SymbolRenderingMode mode) {
    return components::Image::system_name("plus.circle.fill")
        .font(theme::Font::title)
        .symbol_rendering_mode(mode)
        .foreground_style(graphics::ShapeStyle{theme::ColourRole::accent},
                          graphics::ShapeStyle{theme::ColourRole::orange});
  };
  Element wifi =
      components::Image{components::Image::SystemName{"wifi"}, strength.get()}
          .font(theme::Font::title);
  if (animating.get()) {
    wifi = std::move(wifi).symbol_effect(
        composition::SymbolEffect::variable_color(), true);
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::md},
          std::move(named)),
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::lg},
          modes(composition::SymbolRenderingMode::monochrome),
          modes(composition::SymbolRenderingMode::hierarchical),
          modes(composition::SymbolRenderingMode::palette),
          modes(composition::SymbolRenderingMode::multicolor),
          modes(composition::SymbolRenderingMode::multicolor)
              .symbol_color_rendering_mode(
                  composition::SymbolColorRenderingMode::gradient)),
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::lg},
          std::move(wifi),
          components::Image{components::Image::SystemName{"speaker.wave.3"},
                            strength.get()}
              .font(theme::Font::title)
              .symbol_variable_value_mode(
                  composition::SymbolVariableValueMode::draw),
          components::Image{components::Image::SystemName{"battery.100"},
                            strength.get()}
              .font(theme::Font::title),
          controls::Slider(composition::Binding<double>{strength},
                           controls::ClosedRange<double>{.lower_bound = 0.0,
                                                         .upper_bound = 1.0},
                           components::Text("Signal"))
              .frame({.width = 240.0F})
              .id(std::string{signal_slider}),
          controls::Toggle("Animate", composition::Binding<bool>{animating})
              .frame({.width = 160.0F})
              .id(std::string{pulse_toggle})));
}

auto SymbolsPage::images() -> Element {
  const std::shared_ptr<const provider::Image> framed = framed_pixels();
  const std::shared_ptr<const provider::Image> checker = checker_pixels();
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::top,
                 .spacing = theme::space::lg},
             components::ZStack(components::Text("Nine slice"))
                 .frame({.width = 180.0F, .height = 90.0F})
                 .background(components::Image{framed}.resizable(
                     theme::EdgeInsets::all(theme::Space{6.0F}))),
             components::ZStack(components::Text("Tiled"))
                 .frame({.width = 180.0F, .height = 90.0F})
                 .background(components::Image{framed}.resizable(
                     theme::EdgeInsets{},
                     components::Image::ResizingMode::tile)),
             components::ZStack(components::Text("Image paint"))
                 .frame({.width = 180.0F, .height = 90.0F})
                 .background(
                     graphics::ShapeStyle{graphics::ImagePaint{checker}},
                     theme::Shape::rectangle()))
      .id(std::string{panel});
}

auto SymbolsPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const auto left = context.state(left_on);
  const auto right = context.state(right_on);
  const auto first = context.state(front);
  const auto second = context.state(back);
  const auto level = context.state(balance);
  const auto strength = context.state(signal);
  const auto count = context.state(amount);
  const auto chosen = context.state(primaries);
  const auto pressed = context.state(presses);
  const auto animating = context.state(pulsing);
  const auto name = context.state(renamed);
  return presentation::NavigationStack([=] {
    return components::ScrollView(
               components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::xl},
                   section("Buttons", buttons(pressed)),
                   section("Menus", menus(chosen)),
                   section("Mixed state", mixed(left, right, first, second)),
                   section("Sliders and steppers", sliders(level, count)),
                   section("Sharing", sharing(name)),
                   section("Symbols", symbols(strength, animating)),
                   section("Images", images()))
                   .padding(theme::space::xl)
                   .frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))
        .frame({.max_width = infinity, .max_height = infinity})
        .navigation_title("Symbols")
        .toolbar(composition::ToolbarItem(
            composition::ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] {
              shown_flag.set(false);
            }).id(std::string{close_button})));
  });
}

auto SymbolsPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Symbols", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{SymbolsPage{shown}}; });
}

}
