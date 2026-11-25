#include <animators_page.hpp>
#include <assistive_page.hpp>
#include <canvas_page.hpp>
#include <controls_page.hpp>
#include <documents_page.hpp>
#include <graphics_page.hpp>
#include <groupings_page.hpp>
#include <input_block_demo.hpp>
#include <profile_form.hpp>
#include <profile_summary.hpp>
#include <shapes_page.hpp>
#include <springs_page.hpp>
#include <text_input_demo.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <format>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::string> ControlsPage::name{"controls-name",
                                                            std::string{}};
const composition::StateKey<std::string> ControlsPage::secret{"controls-secret",
                                                              std::string{}};
const composition::StateKey<
    std::shared_ptr<composition::Observable<GalleryProfile>>>
    ControlsPage::profile{"controls-profile", nullptr};

auto ControlsPage::CapsuleStyle::make_body(
    components::ButtonStyle::Configuration configuration) const -> Element {
  return Element{VStack(std::move(configuration.label))}
      .padding(theme::Insets::symmetric(theme::space::xl, theme::space::sm))
      .background(
          configuration.is_focused ? ColourRole::success : ColourRole::accent,
          theme::Capsule())
      .foreground_style(ColourRole::on_accent)
      .scale_effect(configuration.is_pressed ? 0.94F : 1.0F)
      .opacity(configuration.is_enabled ? 1.0F : ButtonStyle::disabled_opacity)
      .animation(Animation::bouncy(0.3), configuration.is_pressed)
      .animation(Animation::smooth(0.25), configuration.is_focused);
}

auto ControlsPage::card(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::lg),
                frame({.width = card_width,
                       .alignment = composition::Alignment::leading}),
                flex_grow(1.0F),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(theme::Elevation::low))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto ControlsPage::flavour_name(Flavour value) -> std::string_view {
  switch (value) {
    case Flavour::vanilla:
      return "Vanilla";
    case Flavour::chocolate:
      return "Chocolate";
    case Flavour::strawberry:
      return "Strawberry";
  }
  return "Vanilla";
}

auto ControlsPage::flavours() -> Element {
  return Group(Text(tag(Flavour::vanilla))("Vanilla"),
               Text(tag(Flavour::chocolate))("Chocolate"),
               Text(tag(Flavour::strawberry))("Strawberry"));
}

auto ControlsPage::moods() -> Element {
  return Group(
      components::Image::system_name("star").tag(Mood::star),
      components::Image::system_name("favorite").tag(Mood::favorite),
      components::Image::system_name("bolt").tag(Mood::bolt),
      components::Image::system_name("rocket_launch").tag(Mood::rocket));
}

auto ControlsPage::sizes() -> Element {
  return Group(Text(tag(Size::small))("Small"),
               Text(tag(Size::medium))("Medium"),
               Text(tag(Size::large))("Large"));
}

auto ControlsPage::button_styles(composition::Context& context) -> Element {
  const auto pressed = context.state(presses);
  const auto press = [pressed] { pressed.set(pressed.get() + 1); };
  return card(
      "Button styles",
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}),
             id(style_demos))(
          HStack(spacing(theme::space::sm))(
              Button(action(press), id(first_button))("Automatic"),
              Button(action(press),
                     button_style(ButtonStyle::bordered()))("Bordered")),
          HStack(spacing(theme::space::sm))(
              Button(
                  action(press),
                  button_style(ButtonStyle::bordered_prominent()))("Prominent"),
              Button(action(press),
                     button_style(ButtonStyle::borderless()))("Borderless"),
              Button(action(press),
                     button_style(ButtonStyle::plain()))("Plain")),
          Text(font::footnote, foreground_style(ColourRole::text_secondary))(
              std::format("Pressed {} times", pressed.get()))));
}

auto ControlsPage::button_roles(composition::Context& context) -> Element {
  const auto pressed = context.state(presses);
  const auto press = [pressed] { pressed.set(pressed.get() + 1); };
  return card(
      "Roles and custom style",
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}),
             id(role_demos))(
          HStack(spacing(theme::space::sm), wrap,
                 button_style(ButtonStyle::bordered()))(
              Button(role(ButtonRole::destructive), action(press))("Delete"),
              Button(role(ButtonRole::cancel), action(press))("Cancel"),
              Button(action(press), disabled(true))("Disabled")),
          HStack(spacing(theme::space::sm))(
              Button(role(ButtonRole::destructive), action(press),
                     button_style(ButtonStyle::bordered_prominent()))("Erase"),
              Button(action(press), button_style(CapsuleStyle{}))("Capsule"),
              Button(action(press), button_style(CapsuleStyle{}),
                     disabled(true), id(disabled_capsule))("Off"))));
}

auto ControlsPage::toggles(composition::Context& context) -> Element {
  const auto wifi_on = context.state(wifi);
  const auto bluetooth_on = context.state(bluetooth);
  const auto sync_on = context.state(sync);
  const auto locked_on = context.state(locked);
  return card(
      "Toggles",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Toggle("Wi-Fi", wifi_on),
          Toggle(toggle_style(CheckboxToggleStyle{}))("Bluetooth",
                                                      bluetooth_on),
          HStack(spacing(theme::space::sm))(
              Toggle(toggle_style(ButtonToggleStyle{}))("Sync", sync_on),
              Toggle(toggle_style(ButtonToggleStyle{}))("Lock sliders",
                                                        locked_on))));
}

auto ControlsPage::sliders(composition::Context& context) -> Element {
  const auto locked_on = context.state(locked);
  const auto volume_level = context.state(volume);
  const auto brightness_level = context.state(brightness);
  const auto count = context.state(quantity);
  return card(
      "Slider and stepper",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}),
             disabled(locked_on.get()))(
          Text(font::footnote, foreground_style(ColourRole::text_secondary))(
              std::format("Volume {:.0f}%", volume_level.get() * 100)),
          Slider(volume_level),
          Text(font::footnote, foreground_style(ColourRole::text_secondary))(
              std::format("Brightness {:.1f}", brightness_level.get())),
          Slider(brightness_level,
                 ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
                 0.1),
          Stepper(std::format("Quantity {}", count.get()), count,
                  ClosedRange<int>{.lower_bound = 0, .upper_bound = 9})));
}

auto ControlsPage::pickers(composition::Context& context) -> Element {
  const auto taste = context.state(flavour);
  const auto measure = context.state(size);
  const auto cupped = context.state(cup);
  const auto felt = context.state(mood);
  return card(
      "Pickers",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Picker(picker_style(SegmentedPickerStyle{}))("Size", measure,
                                                       [] { return sizes(); }),
          Picker("Flavour", taste, [] { return flavours(); }),
          Picker(picker_style(NavigationLinkPickerStyle{}))(
              "Cup", cupped, [] { return sizes(); }),
          Picker(picker_style(PalettePickerStyle{}), id(palette_picker))(
              "Mood", felt, [] { return moods(); })));
}

auto ControlsPage::tint_card(composition::Context& context) -> Element {
  const auto volume_level = context.state(volume);
  const auto tinted_on = context.state(tinted);
  const auto pressed = context.state(presses);
  const auto press = [pressed] { pressed.set(pressed.get() + 1); };
  return card("Tint",
              VStack(alignment::leading, spacing(theme::space::md),
                     frame({.max_width = infinity,
                            .alignment = composition::Alignment::leading}),
                     id(tint_demo), tint(theme::Colour::hex(0xFF2D55)))(
                  Toggle("Tinted", tinted_on), Slider(volume_level),
                  Button(action(press),
                         button_style(ButtonStyle::bordered()))("Tinted")));
}

auto ControlsPage::inline_pickers(composition::Context& context) -> Element {
  const auto scooped = context.state(scoop);
  return card("Inline pickers",
              HStack(alignment::top, spacing(theme::space::lg),
                     frame({.max_width = infinity,
                            .alignment = composition::Alignment::top}))(
                  Picker(picker_style(InlinePickerStyle{}))(
                      "Scoop", scooped, [] { return flavours(); }),
                  Picker(picker_style(RadioGroupPickerStyle{}))(
                      "Again", scooped, [] { return flavours(); })));
}

auto ControlsPage::menu_card(composition::Context& context) -> Element {
  const auto taste = context.state(flavour);
  const auto last = context.state(chosen);
  return card(
      "Menu",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Menu("Actions",
               [last] {
                 return Group(
                     Button(action([last] { last.set("duplicated"); }))(
                         "Duplicate"),
                     Button(action([last] { last.set("renamed"); }))("Rename"),
                     Button(role(ButtonRole::destructive),
                            action([last] { last.set("deleted"); }))("Delete"));
               }),
          Text(font::footnote, foreground_style(ColourRole::text_secondary))(
              std::format("Last action: {}  ·  {}", last.get(),
                          flavour_name(taste.get())))));
}

auto ControlsPage::progress_card(composition::Context& context) -> Element {
  const auto done = context.state(progress);
  return card(
      "Progress",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          HStack(spacing(theme::space::xl))(
              ProgressView(), ProgressView(progress_view_style(
                                  CircularProgressViewStyle{}))(done.get())),
          ProgressView("Downloading", done.get()),
          ProgressView(progress_view_style(LinearProgressViewStyle{}))(),
          Button(action([done] {
                   done.set(done.get() >= 1.0 ? 0.0 : done.get() + 0.1);
                 }),
                 button_style(ButtonStyle::bordered()))("Advance")));
}

auto ControlsPage::labels_card() -> Element {
  return card(
      "Labels, divider and images",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Label("Favourites", Glyph::checkmark(ColourRole::accent, 18.0F)),
          Label(label_style(TitleOnlyLabelStyle{}))(
              "Title only", Glyph::plus(ColourRole::accent, 18.0F)),
          Label(label_style(IconOnlyLabelStyle{}))(
              "Icon only", Glyph::chevron_right(ColourRole::accent, 18.0F)),
          Divider(),
          HStack(spacing(theme::space::md))(
              Image("arisa/images/landscape.png"), Divider(),
              Image("arisa/images/missing.png"))));
}

auto ControlsPage::text_fields(composition::Context& context) -> Element {
  const auto typed = context.state(name);
  const auto hidden = context.state(secret);
  return card(
      "Text fields",
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          TextField("Name", typed, TextFieldOptions{.prompt = "Required"}),
          SecureField("Password", hidden),
          TextField(text_field_style(PlainTextFieldStyle{}))("Plain field",
                                                             typed)));
}

auto ControlsPage::wheel_card(composition::Context& context) -> Element {
  const auto spun = context.state(wheel);
  return card("Wheel picker",
              VStack(alignment::leading, spacing(theme::space::md),
                     frame({.max_width = infinity,
                            .alignment = composition::Alignment::leading}))(
                  Picker(picker_style(WheelPickerStyle{}), id(wheel_picker))(
                      "Flavour", spun, [] { return flavours(); })));
}

auto ControlsPage::body(composition::Context& context) const -> Element {
  auto& model = context.retained(profile);
  if (model == nullptr) {
    model = std::make_shared<composition::Observable<GalleryProfile>>();
  }
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(HStack(
      alignment::top, spacing(theme::space::lg), wrap,
      frame({.max_width = infinity, .alignment = composition::Alignment::top}))(
      button_styles(context), button_roles(context), toggles(context),
      sliders(context), pickers(context), tint_card(context),
      card("Input block", InputBlockDemo{}), inline_pickers(context),
      menu_card(context), progress_card(context), labels_card(),
      text_fields(context), wheel_card(context),
      card("Bound model", ProfileForm{*model}),
      card("Same model", ProfileSummary{*model}),
      card("Text input", TextInputDemo{}),
      card("More controls", GroupingsPage::Opener{}),
      card("Visual effects", GraphicsPage::Opener{}),
      card("Shapes", ShapesPage::Opener{}),
      card("Canvas and colour", CanvasPage::Opener{}),
      card("Springs and transitions", SpringsPage::Opener{}),
      card("Assistive technology", AssistivePage::Opener{}),
      card("More motion", AnimatorsPage::Opener{}),
      card("Documents and data", DocumentsPage::Opener{})));
}

}
