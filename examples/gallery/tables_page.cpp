#include <tables_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_section_spacing.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/disclosure_table_row.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/swipe_row.hpp>
#include <arisa/structure/table.hpp>
#include <arisa/structure/table_column.hpp>
#include <arisa/structure/table_header_row_content.hpp>
#include <arisa/structure/table_row.hpp>
#include <arisa/structure/table_style.hpp>
#include <arisa/structure/tuple_table_row_content.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using structure::KeyPathComparator;
using structure::TableColumn;
using structure::TableColumnAlignment;
using theme::ColourRole;

const composition::StateKey<std::set<int>> TablesPage::chosen{"tables-chosen",
                                                              {}};
const composition::StateKey<
    std::vector<structure::KeyPathComparator<TablesPage::File>>>
    TablesPage::order{"tables-order", {KeyPathComparator<File>{&File::name}}};
const composition::StateKey<structure::TableColumnCustomization>
    TablesPage::arranged{"tables-arranged", {}};

auto TablesPage::files() -> std::vector<File> {
  static constexpr std::array kinds{"Folder", "Image", "Text", "Movie",
                                    "Audio"};
  static constexpr std::array stems{"Alpha", "Bravo",   "Charlie", "Delta",
                                    "Echo",  "Foxtrot", "Golf",    "Hotel"};
  std::vector<File> made;
  made.reserve(file_count);
  for (std::size_t index = 0; index < file_count; ++index) {
    made.push_back(File{
        .id = static_cast<int>(index),
        .name = std::format("{} {}", stems[index % stems.size()], index + 1),
        .kind = kinds[(index * 3) % kinds.size()],
        .size = static_cast<int>(((index * 7919) % 9000) + 12),
        .children = std::nullopt});
  }
  return made;
}

auto TablesPage::tree() -> std::vector<File> {
  return {File{.id = 1000,
               .name = "Documents",
               .kind = "Folder",
               .size = 0,
               .children = std::vector<File>{File{.id = 1001,
                                                  .name = "Report",
                                                  .kind = "Text",
                                                  .size = 120,
                                                  .children = std::nullopt},
                                             File{.id = 1002,
                                                  .name = "Budget",
                                                  .kind = "Text",
                                                  .size = 88,
                                                  .children = std::nullopt}}},
          File{.id = 1003,
               .name = "Pictures",
               .kind = "Folder",
               .size = 0,
               .children = std::vector<File>{File{.id = 1004,
                                                  .name = "Beach",
                                                  .kind = "Image",
                                                  .size = 4200,
                                                  .children = std::nullopt}}},
          File{.id = 1005,
               .name = "Notes",
               .kind = "Text",
               .size = 9,
               .children = std::nullopt}};
}

auto TablesPage::sorted(std::vector<File> rows,
                        const std::vector<KeyPathComparator<File>>& by)
    -> std::vector<File> {
  std::ranges::stable_sort(rows, [&by](const File& left, const File& right) {
    for (const KeyPathComparator<File>& comparator : by) {
      const std::weak_ordering found = comparator.compare(left, right);
      if (found != 0) {
        return found < 0;
      }
    }
    return false;
  });
  return rows;
}

auto TablesPage::files_demo(
    composition::StateHandle<std::set<int>> picked,
    composition::StateHandle<std::vector<KeyPathComparator<File>>> by,
    composition::StateHandle<structure::TableColumnCustomization> layout,
    composition::StateHandle<int> count) -> Element {
  return structure::Table(
             sorted(files(), by.get()), &File::id, picked, by, layout,
             TableColumn<File>("Name", &File::name).customization_id("name"),
             TableColumn<File>("Kind", &File::kind)
                 .customization_id("kind")
                 .width(110.0F),
             TableColumn<File>("Size", &File::size,
                               [](const File& file) {
                                 return components::Text(
                                     std::format("{} KB", file.size));
                               })
                 .customization_id("size")
                 .width(80.0F)
                 .alignment(structure::TableColumnAlignment::numeric))
      .context_menu(
          std::type_identity<int>{},
          [](const std::set<int>& ids) {
            return Element{components::Group(
                components::Button(ids.size() > 1 ? "Open All" : "Open", [] {}),
                components::Button("Duplicate", [] {}))};
          },
          [count](const std::set<int>& ids) {
            count.set(count.get() + static_cast<int>(ids.size()));
          })
      .table_style(structure::InsetTableStyle{})
      .id(std::string{files_table});
}

auto TablesPage::outline_demo() -> Element {
  const std::vector<File> roots = tree();
  return structure::Table(
             std::type_identity<File>{}, &File::id,
             TableColumn<File>("Name", &File::name),
             TableColumn<File>("Kind", &File::kind).width(90.0F),
             structure::TupleTableRowContent<File>(
                 structure::TableHeaderRowContent<File>(
                     "Folders",
                     structure::DisclosureTableRow<File>(
                         roots[0],
                         structure::TableRow<File>{(*roots[0].children)[0]},
                         structure::TableRow<File>{(*roots[0].children)[1]}),
                     structure::DisclosureTableRow<File>(
                         roots[1],
                         structure::TableRow<File>{(*roots[1].children)[0]})),
                 structure::TableHeaderRowContent<File>(
                     "Loose", structure::TableRow<File>{roots[2]})))
      .table_style(structure::BorderedTableStyle{})
      .id(std::string{outline_table});
}

auto TablesPage::sections_demo(composition::StateHandle<bool> inbox)
    -> Element {
  return structure::List(
             structure::Section(
                 "Inbox", inbox,
                 components::Text("Unread").badge(12).badge_prominence(
                     composition::BadgeProminence::increased),
                 components::Text("Flagged").badge(3),
                 components::Text("Archive").badge(40).badge_prominence(
                     composition::BadgeProminence::decreased))
                 .section_actions(
                     [] { return Element{components::Button("Add", [] {})}; }),
             structure::Section(
                 "Folders",
                 structure::DisclosureGroup("Work",
                                            [] {
                                              return Element{components::Group(
                                                  components::Text("Projects"),
                                                  components::Text("Reviews"))};
                                            }),
                 components::Text("Personal"))
                 .collapsible(true)
                 .list_section_separator(composition::Visibility::visible),
             structure::Section("Margins", components::Text("Inset row"))
                 .list_section_margins(theme::Edge::horizontal, 48.0F))
      .list_section_spacing(composition::ListSectionSpacing::compact)
      .default_min_list_header_height(32.0F)
      .id(std::string{sections_list});
}

auto TablesPage::swipe_demo() -> Element {
  static constexpr std::array names{"Inbox", "Drafts", "Sent", "Junk"};
  return components::ScrollView(
             components::LazyVStack(
                 components::LazyVStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::xs},
                 components::ForEach(
                     names, [](const char* name) { return std::string{name}; },
                     [](const char* name) {
                       return components::Text(std::string{name})
                           .padding(theme::space::md)
                           .frame(
                               {.max_width = infinity,
                                .alignment = composition::Alignment::leading})
                           .background(
                               ColourRole::surface_raised,
                               theme::Shape::rounded(theme::radius::card, true))
                           .swipe_actions([] {
                             return Element{components::Button(
                                 "Delete", components::ButtonRole::destructive,
                                 [] {})};
                           });
                     })))
      .swipe_actions_container()
      .frame({.width = 320.0F, .height = 220.0F})
      .id(std::string{swipe_stack});
}

auto TablesPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const auto which = context.state(demo);
  const auto picked = context.state(chosen);
  const auto by = context.state(order);
  const auto layout = context.state(arranged);
  const auto count = context.state(opened);
  const auto inbox = context.state(inbox_open);
  return presentation::NavigationStack([=] {
    Element shown_demo = components::Group();
    switch (which.get()) {
      case Demo::files:
        shown_demo = files_demo(picked, by, layout, count);
        break;
      case Demo::outline:
        shown_demo = outline_demo();
        break;
      case Demo::sections:
        shown_demo = components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::top,
                .spacing = theme::space::lg},
            sections_demo(inbox), swipe_demo());
        break;
    }
    return components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = theme::space::md},
               components::HStack(
                   components::HStackOptions{
                       .alignment = components::VerticalAlignment::center,
                       .spacing = theme::space::lg},
                   controls::Picker(
                       "Demo", which,
                       [] {
                         return Element{components::Group(
                             components::Text("Files").tag(Demo::files),
                             components::Text("Outline").tag(Demo::outline),
                             components::Text("Sections").tag(Demo::sections))};
                       })
                       .picker_style(controls::PickerStyle::segmented())
                       .labels_hidden()
                       .frame({.width = 360.0F})
                       .id(std::string{demo_picker}),
                   components::Text(std::format("Opened {}", count.get()))
                       .foreground_style(ColourRole::text_secondary)
                       .id(std::string{opened_label})),
               std::move(shown_demo)
                   .frame({.max_width = infinity, .max_height = infinity}))
        .padding(theme::space::lg)
        .frame({.max_width = infinity, .max_height = infinity})
        .navigation_title("Tables")
        .toolbar(composition::ToolbarItem(
            composition::ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] {
              shown_flag.set(false);
            }).id(std::string{close_button})));
  });
}

auto TablesPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Tables", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown, [shown] { return Element{TablesPage{shown}}; });
}

}
