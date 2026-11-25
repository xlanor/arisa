#include <row_actions_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/edit_actions.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <ranges>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

const composition::StateKey<std::vector<RowActionsPage::Save>>
    RowActionsPage::saves_key{"row-actions-saves",
                              RowActionsPage::initial_saves()};
const composition::StateKey<std::set<int>> RowActionsPage::chosen_key{
    "row-actions-chosen", std::set<int>{}};
const composition::StateKey<std::vector<int>> RowActionsPage::tiles_key{
    "row-actions-tiles", RowActionsPage::initial_tiles()};

auto RowActionsPage::initial_saves() -> std::vector<Save> {
  constexpr std::array games{"Breath", "Odyssey", "Kart", "Splat",
                             "Pikmin", "Metroid", "Xeno", "Smash"};
  std::vector<Save> found;
  found.reserve(save_count);
  for (int index = 0; index < save_count; ++index) {
    found.push_back(
        Save{.id = index,
             .title = std::format(
                 "{} {}", games[static_cast<std::size_t>(index) % games.size()],
                 (index / static_cast<int>(games.size())) + 1),
             .slots = (index % 5),
             .pinned = false});
  }
  return found;
}

auto RowActionsPage::initial_tiles() -> std::vector<int> {
  return std::views::iota(1, tile_count + 1) | std::ranges::to<std::vector>();
}

auto RowActionsPage::files() -> std::vector<Entry> {
  return {
      Entry{
          .name = "switch",
          .children = std::vector<
              Entry>{Entry{.name = "arisa",
                           .children =
                               std::vector<Entry>{
                                   Entry{.name = "arisa.nro",
                                         .children = std::nullopt},
                                   Entry{.name = "config.toml",
                                         .children = std::nullopt}}},
                     Entry{.name = "checkpoint",
                           .children = std::vector<Entry>{Entry{
                               .name = "saves",
                               .children =
                                   std::vector<Entry>{Entry{.name = "slot1.bin",
                                                            .children = std::nullopt},
                                                      Entry{.name = "slot2.bin",
                                                            .children = std::
                                                                nullopt}}}}}}},
      Entry{.name = "atmosphere",
            .children =
                std::vector<Entry>{
                    Entry{.name = "config", .children = std::vector<Entry>{}},
                    Entry{.name = "contents", .children = std::nullopt}}},
      Entry{.name = "hbmenu.nro", .children = std::nullopt}};
}

auto RowActionsPage::title_name(int index) -> std::string {
  const auto letter = static_cast<char>('A' + (index % 26));
  return std::format("{} title {}", letter, (index / 26) + 1);
}

auto RowActionsPage::save_row(const Save& save,
                              composition::StateHandle<std::vector<Save>> all)
    -> Element {
  const int id = save.id;
  const bool pinned = save.pinned;
  const auto remove = [all, id] {
    motion::with_animation(motion::Animation::smooth(0.3), [&all, id] {
      all.update([id](std::vector<Save>& every) {
        std::erase_if(every, [id](const Save& each) { return each.id == id; });
      });
    });
  };
  const auto pin = [all, id] {
    all.update([id](std::vector<Save>& every) {
      for (Save& each : every) {
        if (each.id == id) {
          each.pinned = !each.pinned;
        }
      }
    });
  };
  return Element{
      components::Label(save.title, components::Image::system_name(
                                        pinned ? "bookmark" : "description"))}
      .tag(id)
      .badge(save.slots)
      .swipe_actions(
          composition::HorizontalEdge::trailing, true,
          [remove] {
            return Element{components::Button(
                "Delete", components::ButtonRole::destructive, remove)};
          })
      .swipe_actions(
          composition::HorizontalEdge::leading, false,
          [pin, pinned] {
            return Element{components::Button(pinned ? "Unpin" : "Pin", pin)};
          })
      .context_menu([remove, pin, pinned] {
        return components::Group(
            components::Button(pinned ? "Unpin" : "Pin", pin),
            components::Button("Delete", components::ButtonRole::destructive,
                               remove));
      });
}

auto RowActionsPage::saves(composition::Context& context) -> Element {
  const auto all = context.state(saves_key);
  const auto chosen = context.state(chosen_key);
  const auto editing = context.state(editing_key);
  const bool edit = editing.get();
  const std::size_t picked = chosen.get().size();
  Element controls =
      components::HStack(
          components::HStackOptions{.spacing = theme::space::sm},
          components::Button(edit ? "Done" : "Edit",
                             [editing] { editing.set(!editing.get()); })
              .id(edit_button),
          components::Button(
              "Delete", components::ButtonRole::destructive,
              [all, chosen] {
                const std::set<int> gone = chosen.get();
                motion::with_animation(motion::Animation::smooth(0.3), [&] {
                  all.update([&gone](std::vector<Save>& every) {
                    std::erase_if(every, [&gone](const Save& each) {
                      return gone.contains(each.id);
                    });
                  });
                  chosen.set(std::set<int>{});
                });
              })
              .disabled(picked == 0)
              .id(delete_button),
          components::Text(std::format("{}", picked))
              .foreground_style(ColourRole::text_secondary))
          .button_style(components::ButtonStyle::bordered());
  Element list =
      structure::List(
          chosen,
          structure::Section(
              "Saves",
              components::ForEach(
                  all.get(), [](const Save& save) { return save.id; },
                  [all](const Save& save) { return save_row(save, all); })
                  .on_delete([all](const components::IndexSet& offsets) {
                    all.update([&offsets](std::vector<Save>& every) {
                      offsets.remove_from(every);
                    });
                  })))
          .environment(components::Environment::edit_mode,
                       edit ? components::EditMode::active
                            : components::EditMode::inactive)
          .id(saves_list);
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      std::move(controls), std::move(list));
}

auto RowActionsPage::settings() -> Element {
  return structure::Form(
      structure::Section(
          "Settings",
          structure::DisclosureGroup(
              "Display",
              [] {
                return Element{components::Group(
                    controls::Toggle(
                        "Dark mode",
                        composition::Binding<bool>::constant(true)),
                    controls::Toggle(
                        "Reduce motion",
                        composition::Binding<bool>::constant(false)))};
              }),
          structure::DisclosureGroup(
              "Audio",
              [] {
                return Element{components::Group(controls::Toggle(
                    "Sounds", composition::Binding<bool>::constant(true)))};
              }),
          structure::DisclosureGroup(
              "Network",
              [] {
                return Element{components::Group(
                    controls::Toggle(
                        "Wi-Fi", composition::Binding<bool>::constant(true)),
                    structure::DisclosureGroup("Advanced", [] {
                      return Element{components::Group(controls::Toggle(
                          "Proxy",
                          composition::Binding<bool>::constant(false)))};
                    }))};
              }))
          .header_prominence(composition::Prominence::increased));
}

auto RowActionsPage::file_tree() -> Element {
  return structure::List(
      files(), [](const Entry& entry) { return entry.name; },
      [](const Entry& entry) { return entry.children; },
      [](const Entry& entry) {
        return components::Label(
            entry.name, components::Image::system_name(
                            entry.children.has_value() ? "folder" : "article"));
      });
}

auto RowActionsPage::titles() -> Element {
  std::vector<Element> sections;
  sections.reserve(26);
  for (int letter = 0; letter < 26; ++letter) {
    const std::string label(1, static_cast<char>('A' + letter));
    sections.push_back(
        structure::Section(
            label,
            components::ForEach(
                std::views::iota(0, title_count / 26) |
                    std::views::transform(
                        [letter](int row) { return letter + (row * 26); }),
                [](int index) { return index; },
                [](int index) { return components::Text(title_name(index)); }))
            .section_index_label(label));
  }
  return structure::List(std::move(sections))
      .list_style(structure::PlainListStyle{})
      .list_section_index_visibility(composition::Visibility::visible)
      .id(titles_list);
}

auto RowActionsPage::tiles(composition::Context& context) -> Element {
  const auto order = context.state(tiles_key);
  const auto editing = context.state(editing_key);
  const bool edit = editing.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      components::Button(edit ? "Done" : "Edit",
                         [editing] { editing.set(!editing.get()); })
          .button_style(components::ButtonStyle::bordered()),
      components::ScrollView(
          components::LazyVGrid(
              components::LazyVGridOptions{
                  .columns = {components::GridItem{
                      components::GridItem::Size::adaptive(120.0F)}},
                  .spacing = theme::space::md},
              components::ForEach(
                  composition::Binding<std::vector<int>>{order},
                  components::EditActions::move, [](int tile) { return tile; },
                  [](int tile) {
                    return components::Text(std::format("{}", tile))
                        .font(theme::Font::title2)
                        .frame({.height = 88.0F, .max_width = infinity})
                        .background(
                            ColourRole::surface_raised,
                            theme::Shape::rounded(theme::radius::card, true))
                        .focusable();
                  }))
              .id(tiles_grid))
          .frame({.max_width = infinity, .max_height = infinity})
          .environment(components::Environment::edit_mode,
                       edit ? components::EditMode::active
                            : components::EditMode::inactive));
}

auto RowActionsPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto panel = context.state(panel_key);
  Element content = saves(context);
  switch (panel.get()) {
    case Panel::saves:
      break;
    case Panel::settings:
      content = settings();
      break;
    case Panel::files:
      content = file_tree();
      break;
    case Panel::titles:
      content = titles();
      break;
    case Panel::tiles:
      content = tiles(context);
      break;
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::HStack(
                 components::HStackOptions{.spacing = theme::space::md},
                 components::Button("Close", [shown] { shown.set(false); })
                     .button_style(components::ButtonStyle::bordered())
                     .id(close_button),
                 controls::Picker(
                     "Panel", panel,
                     [] {
                       return Element{components::Group(
                           components::Text("Saves").tag(Panel::saves),
                           components::Text("Settings").tag(Panel::settings),
                           components::Text("Files").tag(Panel::files),
                           components::Text("Titles").tag(Panel::titles),
                           components::Text("Tiles").tag(Panel::tiles))};
                     })
                     .picker_style(controls::SegmentedPickerStyle{})),
             std::move(content).frame(
                 {.max_width = infinity, .max_height = infinity}))
      .padding(theme::space::lg)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .background(ColourRole::system_grouped_background,
                  theme::Shape::rectangle());
}

}
