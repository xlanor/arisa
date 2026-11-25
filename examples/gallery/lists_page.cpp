#include <lists_page.hpp>
#include <row_actions_page.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;

const composition::StateKey<std::string> ListsPage::name{"lists-name",
                                                         std::string{}};
const composition::StateKey<std::vector<std::string>> ListsPage::fruits{
    "lists-fruits", ListsPage::initial_fruits()};
const composition::StateKey<std::vector<ListsPage::Detail>> ListsPage::path{
    "lists-path", {}};

auto ListsPage::initial_fruits() -> std::vector<std::string> {
  return {"Apple", "Banana", "Cherry", "Damson", "Elderberry"};
}

auto ListsPage::quality_name(Quality value) -> std::string_view {
  switch (value) {
    case Quality::low:
      return "Low";
    case Quality::balanced:
      return "Balanced";
    case Quality::high:
      return "High";
  }
  return "Balanced";
}

auto ListsPage::detail(Detail shown) -> Element {
  const std::string_view title = shown == Detail::about ? "About" : "Storage";
  return VStack(
      alignment::leading, spacing(theme::space::md),
      padding(theme::space::lg))(Text(font::title2)(std::string{title}));
}

auto ListsPage::Settings::body(composition::Context& context) const -> Element {
  const auto wifi_on = context.state(wifi);
  const auto bluetooth_on = context.state(bluetooth);
  const auto level = context.state(volume);
  const auto tint = tint_;
  const auto chosen = context.state(quality);
  const auto typed = context.state(name);
  const auto actions = context.state(row_actions);
  constexpr std::array qualities{Quality::low, Quality::balanced,
                                 Quality::high};
  return Form(id(settings_form))(
      Section(header(Text("Toggle rows")))(Toggle("Wi-Fi", wifi_on),
                                           Toggle("Bluetooth", bluetooth_on)),
      Section(header(Text("Detail rows")))(
          NavigationLink(Detail::about,
                         Element{LabeledContent("About", "arisa 0.0.1")}),
          NavigationLink(Detail::storage,
                         Element{LabeledContent("Storage", "12.4 GB free")})),
      Section(header(Text("Input rows")))(
          TextField("Name", typed),
          Picker("Accent", tint,
                 [] {
                   return Element{Group(Text(tag(Accent::blue))("Blue"),
                                        Text(tag(Accent::green))("Green"),
                                        Text(tag(Accent::orange))("Orange"))};
                 }),
          LabeledContent("Volume",
                         Element{Slider(level).frame({.width = 220.0F})})),
      Section(header(Text("Radio rows")))(ForEach(
          qualities, [](Quality value) { return static_cast<int>(value); },
          [chosen](Quality value) {
            const bool on = chosen.get() == value;
            return Button(action([chosen, value] { chosen.set(value); }))(
                Element{LabeledContent(
                    std::string{quality_name(value)},
                    on ? Element{Image(font::body,
                                       foreground_style(ColourRole::accent))(
                             system_name("check"))}
                       : Element{Spacer()})});
          })),
      Section(header(Text("More rows")))(
          Button(action([actions] { actions.set(true); }),
                 id(row_actions_button), full_screen_cover(actions, [actions] {
                   return Element{RowActionsPage{actions}};
                 }))("Row actions")));
}

auto ListsPage::fruit_rows(composition::Context& context) -> Element {
  const auto shown = context.state(fruits);
  const auto edit = context.state(editing);
  const auto count = context.state(added);
  static constexpr std::array pool{"Fig",   "Grape", "Kiwi",
                                   "Lemon", "Mango", "Nectarine",
                                   "Olive", "Peach", "Quince"};
  const auto add = [shown, count] {
    with_animation(Animation::smooth(0.35), [&] {
      const auto index = static_cast<std::size_t>(count.get());
      shown.update([index](std::vector<std::string>& items) {
        items.insert(items.begin(),
                     std::format("{} {}", pool[index % pool.size()],
                                 (index / pool.size()) + 1));
      });
      count.set(count.get() + 1);
    });
  };
  const auto remove = [shown] {
    with_animation(Animation::smooth(0.35), [&] {
      shown.update([](std::vector<std::string>& items) {
        if (!items.empty()) {
          items.erase(items.begin());
        }
      });
    });
  };
  const auto shuffle = [shown] {
    with_animation(Animation::snappy(0.4), [&] {
      shown.update(
          [](std::vector<std::string>& items) { std::ranges::reverse(items); });
    });
  };
  return List(id(fruit_list),
              environment(components::Environment::edit_mode,
                          edit.get() ? EditMode::active : EditMode::inactive))(
      Section(header(Text("Reorder and delete")))(
          ForEach(
              shown.get(), [](const std::string& item) { return item; },
              [](const std::string& item) {
                return Label(item, Element{Image(system_name{"label"})});
              })
              .on_move([shown](const IndexSet& from, std::size_t to) {
                shown.update([&from, to](std::vector<std::string>& items) {
                  from.move_from(items, to);
                });
              })
              .on_delete([shown](const IndexSet& offsets) {
                shown.update([&offsets](std::vector<std::string>& items) {
                  offsets.remove_from(items);
                });
              })),
      Section(header(Text("Insert and remove")))(
          Toggle("Edit mode", edit),
          HStack(spacing(theme::space::sm), wrap,
                 button_style(ButtonStyle::bordered()))(
              Button(action(add), id(add_button))("Add"),
              Button(action(remove), id(remove_button))("Remove"),
              Button(action(shuffle), id(shuffle_button))("Reverse"))),

      Section(header(Text("Long list")))(ForEach(
          std::views::iota(1, 1001), [](int row) { return 100000 + row; },
          [](int row) {
            return LabeledContent(std::format("Row {}", row),
                                  std::format("#{}", row));
          })));
}

auto ListsPage::colour_of(Accent value) noexcept -> theme::Colour {
  switch (value) {
    case Accent::blue:
      return theme::Colour::hex(0x007AFF);
    case Accent::green:
      return theme::Colour::hex(0x34C759);
    case Accent::orange:
      return theme::Colour::hex(0xFF9500);
  }
  return theme::Colour::hex(0x007AFF);
}

auto ListsPage::body(composition::Context& context) const -> Element {
  const auto pushed = context.state(path);
  const auto tint = context.state(accent);
  return HStack(
      alignment::top, spacing(theme::space::lg),
      frame({.max_width = infinity,
             .max_height = infinity,
             .alignment = composition::Alignment::top}),
      environment(components::Environment::tint,
                  std::optional<graphics::ShapeStyle>{colour_of(tint.get())}))(
      VStack(frame({.max_width = infinity, .max_height = infinity}))(
          NavigationStack(
              pushed,
              [tint] {
                return Element{Settings{tint}}.navigation_destination(
                    for_type<Detail>,
                    [](const Detail& shown) { return detail(shown); });
              })),
      VStack(frame({.max_width = infinity, .max_height = infinity}))(
          fruit_rows(context)));
}

}
