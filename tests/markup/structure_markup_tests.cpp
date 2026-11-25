#include <arisa/components/index_set.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/system_status.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/text/utf8.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace arisa::markup {

using composition::Element;

TEST(IndexSetTest, MovesOffsetsLikeSwiftUI) {
  std::vector<char> items{'a', 'b', 'c', 'd', 'e'};
  IndexSet{1}.move_from(items, 4);
  EXPECT_EQ(items, (std::vector<char>{'a', 'c', 'd', 'b', 'e'}));
  IndexSet{3}.move_from(items, 0);
  EXPECT_EQ(items, (std::vector<char>{'b', 'a', 'c', 'd', 'e'}));
  IndexSet{0, 4}.move_from(items, 2);
  EXPECT_EQ(items, (std::vector<char>{'a', 'b', 'e', 'c', 'd'}));
  IndexSet{2}.move_from(items, 5);
  EXPECT_EQ(items, (std::vector<char>{'a', 'b', 'c', 'd', 'e'}));
}

TEST(IndexSetTest, RemovesOffsetsAndNormalises) {
  std::vector<int> items{10, 11, 12, 13};
  const IndexSet offsets{3, 1, 3};
  EXPECT_EQ(offsets.count(), 2U);
  EXPECT_TRUE(offsets.contains(1));
  EXPECT_FALSE(offsets.contains(2));
  offsets.remove_from(items);
  EXPECT_EQ(items, (std::vector<int>{10, 12}));
}

TEST(SystemSymbolsTest, NamesAreSortedAndUnique) {
  EXPECT_TRUE(std::ranges::is_sorted(SystemSymbols::names));
  EXPECT_EQ(std::ranges::adjacent_find(SystemSymbols::names),
            SystemSymbols::names.end());
}

TEST(SystemSymbolsTest, NamesMapToDenseSupplementaryCodepoints) {
  EXPECT_EQ(SystemSymbols::codepoint("account_circle"),
            SystemSymbols::first_codepoint);
  EXPECT_EQ(SystemSymbols::codepoint("add"),
            SystemSymbols::first_codepoint + 1);
  EXPECT_EQ(SystemSymbols::codepoint("zoom_out"),
            SystemSymbols::first_codepoint + SystemSymbols::names.size() - 1);
  EXPECT_FALSE(SystemSymbols::codepoint("not_a_symbol").has_value());
  EXPECT_FALSE(SystemSymbols::codepoint("").has_value());
  const auto text = SystemSymbols::text("wifi");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(text->size(), 4U);
  EXPECT_EQ(text::Utf8::decode(*text, 0).codepoint,
            *SystemSymbols::codepoint("wifi"));
}

TEST(SystemSymbolsTest, ImagesCarryTheirSymbolNameAndFont) {
  components::Image symbol =
      components::Image::system_name("star").font(theme::Font::title);
  const auto& content =
      static_cast<const components::Image::Content&>(symbol.content());
  EXPECT_TRUE(content.system());
  EXPECT_EQ(content.name(), "star");
  EXPECT_EQ(content.font(), theme::Font::title);
  Element tagged = Image(font::caption)(system_name{"wifi"});
  const auto& written =
      static_cast<const components::Image::Content&>(tagged.content());
  EXPECT_TRUE(written.system());
  EXPECT_EQ(written.font(), theme::Font::caption);
}

TEST(SystemFontTest, SystemSizesOverrideTheRole) {
  const theme::Font sized =
      theme::Font::system(13.0F, theme::Font::Weight::bold);
  EXPECT_EQ(sized.size(), 13.0F);
  EXPECT_EQ(sized.weight(), theme::Font::Weight::bold);
  EXPECT_FALSE(theme::Font::body.size().has_value());
  const text::TextStyle style =
      components::Text::text_style(components::Environment::theme.fallback(),
                                   sized, text::Locale::undetermined());
  EXPECT_FLOAT_EQ(style.size.value(), 13.0F);
  EXPECT_EQ(style.weight.value(), 700);
}

TEST(StatusSymbolsTest, BatteryShowsBarsAndCharging) {
  using provider::BatteryPercent;
  using provider::BatteryState;
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<100>(), false}),
            "battery_full");
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<3>(), false}),
            "battery_0_bar");
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<50>(), false}),
            "battery_3_bar");
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<90>(), false}),
            "battery_6_bar");
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<10>(), true}),
            "battery_0_bar");
  EXPECT_EQ(Battery::symbol(BatteryState{BatteryPercent::of<84>(), true}),
            "battery_5_bar");
  for (const std::string_view name :
       {Battery::symbol(BatteryState{BatteryPercent::of<0>(), false}),
        Battery::symbol(BatteryState{BatteryPercent::of<94>(), false})}) {
    EXPECT_TRUE(SystemSymbols::codepoint(name).has_value()) << name;
  }
}

TEST(StatusSymbolsTest, WirelessShowsKindAndBars) {
  using provider::NetworkKind;
  using provider::NetworkState;
  using provider::SignalBars;
  EXPECT_EQ(Wireless::symbol(NetworkState{}), "wifi_off");
  EXPECT_EQ(Wireless::symbol(NetworkState{.kind = NetworkKind::ethernet,
                                          .signal_bars = SignalBars::of<3>(),
                                          .address = {}}),
            "lan");
  for (const auto bars : {SignalBars::of<0>(), SignalBars::of<1>(),
                          SignalBars::of<2>(), SignalBars::of<3>()}) {
    const std::string_view name = Wireless::symbol(NetworkState{
        .kind = NetworkKind::wifi, .signal_bars = bars, .address = {}});
    EXPECT_TRUE(SystemSymbols::codepoint(name).has_value()) << name;
  }
}

TEST(StatusSymbolsTest, ClockFormatsLocalTimeAndWaitsForTheNextMinute) {
  const auto at = std::chrono::local_days{std::chrono::year{2026} /
                                          std::chrono::March / 4} +
                  std::chrono::hours{7} + std::chrono::minutes{5} +
                  std::chrono::seconds{48};
  EXPECT_EQ(Clock::text(at), "07:05");
  EXPECT_EQ(Clock::until_next_minute(at), std::chrono::seconds{12});
}

TEST(SystemStatusTest, AbsentStatusReportsNothing) {
  const components::SystemStatus absent;
  EXPECT_FALSE(absent.battery().has_value());
  EXPECT_FALSE(absent.network().has_value());
  EXPECT_FALSE(absent.local_time().has_value());
  EXPECT_EQ(absent, components::SystemStatus{});
}

TEST(NamedStylesTest, NamedTypesMatchTheirFactories) {
  EXPECT_EQ(ButtonStyle{BorderedButtonStyle{}}, ButtonStyle::bordered());
  EXPECT_EQ(ButtonStyle{AutomaticButtonStyle{}}, ButtonStyle::automatic());
  EXPECT_TRUE(ButtonStyle{AutomaticButtonStyle{}}.is_automatic());
  EXPECT_EQ(ToggleStyle{CheckboxToggleStyle{}}, ToggleStyle::checkbox());
  EXPECT_EQ(ToggleStyle{DefaultToggleStyle{}}, ToggleStyle::automatic());
  EXPECT_NE(ToggleStyle{SwitchToggleStyle{}}, ToggleStyle::checkbox());
  EXPECT_NE(ToggleStyle{SwitchToggleStyle{}}, ToggleStyle::automatic());
  EXPECT_EQ(PickerStyle{SegmentedPickerStyle{}}, PickerStyle::segmented());
  EXPECT_NE(PickerStyle{InlinePickerStyle{}}, PickerStyle::radio_group());
  EXPECT_EQ(ProgressViewStyle{LinearProgressViewStyle{}},
            ProgressViewStyle::linear());
  EXPECT_EQ(LabelStyle{IconOnlyLabelStyle{}}, LabelStyle::icon_only());
  EXPECT_EQ(TextFieldStyle{RoundedBorderTextFieldStyle{}},
            TextFieldStyle::rounded_border());
  EXPECT_EQ(ListStyle{SidebarListStyle{}}, ListStyle::sidebar());
  EXPECT_EQ(ListStyle{DefaultListStyle{}}, ListStyle::inset_grouped());
  EXPECT_EQ(TabViewStyle{SidebarAdaptableTabViewStyle{}},
            TabViewStyle::sidebar_adaptable());
  EXPECT_EQ(CellStyle{DefaultCellStyle{}}, CellStyle::automatic());
}

TEST(ToolbarTest, ItemsAreDeclaredWithTheirPlacements) {
  Element declared =
      Element{Text("Body")}
          .navigation_title("Title")
          .toolbar(
              ToolbarItem(ToolbarItemPlacement::top_bar_leading, Text("Lead")),
              ToolbarItem(Text("Auto")))
          .toolbar(ToolbarItem(ToolbarItemPlacement::principal, Text("Mid")));
  const auto& items = declared.navigation().toolbar;
  ASSERT_EQ(items.size(), 3U);
  EXPECT_EQ(items[0]->placement(), ToolbarItemPlacement::top_bar_leading);
  EXPECT_FALSE(items[0]->trailing());
  EXPECT_TRUE(items[1]->trailing());
  EXPECT_EQ(items[2]->placement(), ToolbarItemPlacement::principal);
  Element tagged = Text(toolbar(ToolbarItem(Text("x"))))("y");
  EXPECT_EQ(tagged.navigation().toolbar.size(), 1U);
}

TEST(StructureMarkupTest, TagsBuildTheStructureElements) {
  Element section = Section(header(Text("Head")), footer(Text("Foot")))(
      Text("One"), Text("Two"));
  const auto found = structure::Section::content(section);
  ASSERT_TRUE(found.has_value());
  EXPECT_NE(found->get().header(), nullptr);
  EXPECT_NE(found->get().footer(), nullptr);
  EXPECT_EQ(found->get().rows().size(), 2U);
  Element titled = Section("Title", Text("Row"));
  ASSERT_TRUE(structure::Section::content(titled).has_value());
  EXPECT_NE(structure::Section::content(titled)->get().header(), nullptr);
  Element list = List(list_style(ListStyle::plain()))(
      Text("Loose"), Section("A", Text("a")), Text("Tail"));
  const auto& rows =
      static_cast<const structure::List::Content&>(list.content());
  EXPECT_EQ(rows.section_count(), 3U);
  Element form = Form(Text("x"));
  EXPECT_EQ(form.type(),
            composition::ElementType::of<structure::List::Content>());
  Element labeled = LabeledContent("Name", "Value");
  EXPECT_EQ(
      labeled.type(),
      composition::ElementType::of<components::LabeledContent::Content>());
  Element tabs = TabView(tab_view_style(SidebarAdaptableTabViewStyle{}))(
      Tab("One", "star", 1, [] { return Element{Text("1")}; }),
      Tab("Two", "wifi", 2, [] { return Element{Text("2")}; }));
  EXPECT_EQ(
      static_cast<const structure::TabView::Content&>(tabs.content()).count(),
      2U);
  Element shell = Scaffold(Element{Text("Content")}.navigation_title("Shell"));
  EXPECT_EQ(shell.type(),
            composition::ElementType::of<structure::Scaffold::Content>());
}

TEST(StructureMarkupTest, EditAttributesReachTheForEach) {
  int moved = 0;
  int deleted = 0;
  Element each =
      ForEach(on_move([&moved](const IndexSet&, std::size_t) { ++moved; }),
              on_delete([&deleted](const IndexSet&) { ++deleted; }))(
          std::vector<int>{1, 2}, [](int value) { return value; },
          [](int value) { return Text(std::to_string(value)); });
  const components::ForEach::Edits edits = components::ForEach::edits_of(each);
  ASSERT_TRUE(edits.move);
  ASSERT_TRUE(edits.remove);
  edits.move(IndexSet{0}, 2);
  edits.remove(IndexSet{1});
  EXPECT_EQ(moved, 1);
  EXPECT_EQ(deleted, 1);
}

}

namespace arisa::markup {

TEST(StructureMarkupTest, RowAttributesReachTheListRow) {
  Element row = Text(badge(3), list_row_insets(theme::Insets::all(4.0F)),
                     list_row_separator(Visibility::hidden, VerticalEdges::top),
                     selection_disabled, move_disabled, delete_disabled,
                     section_index_label("A"))("Row");
  const auto attributes = row.list_row();
  ASSERT_NE(attributes, nullptr);
  EXPECT_EQ(attributes->badge, std::optional<std::string>{"3"});
  EXPECT_EQ(attributes->insets, theme::Insets::all(4.0F));
  EXPECT_EQ(attributes->top.visibility, Visibility::hidden);
  EXPECT_EQ(attributes->bottom.visibility, Visibility::automatic);
  EXPECT_TRUE(attributes->selection_disabled);
  EXPECT_TRUE(attributes->move_disabled);
  EXPECT_TRUE(attributes->delete_disabled);
  EXPECT_EQ(attributes->index_label, std::optional<std::string>{"A"});
  Element zero = Text(badge(0))("Zero");
  ASSERT_NE(zero.list_row(), nullptr);
  EXPECT_FALSE(zero.list_row()->badge.has_value());
  Element group = DisclosureGroup("Group", [] { return Element{Text("x")}; });
  EXPECT_EQ(
      group.type(),
      composition::ElementType::of<structure::DisclosureGroup::Content>());
  EXPECT_EQ(ListStyle{GroupedListStyle{}}, ListStyle::grouped());
  EXPECT_EQ(ListStyle{InsetListStyle{}}, ListStyle::inset());
}

TEST(StructureMarkupTest, GroupingAndControlTagsBuildTheirViews) {
  Element box = GroupBox(group_box_style(GroupBoxStyle::automatic()))(
      "Title", Text("Inside"));
  EXPECT_EQ(box.type(),
            composition::ElementType::of<structure::GroupBox::Content>());
  Element group = ControlGroup(control_group_style(
      ControlGroupStyle::palette()))([] { return Element{Text("A")}; });
  EXPECT_EQ(group.type(),
            composition::ElementType::of<structure::ControlGroup::Content>());
  Element gauge = Gauge(gauge_style(GaugeStyle::accessory_circular()),
                        labels_hidden())(0.5, Text("Fuel"));
  EXPECT_EQ(gauge.type(),
            composition::ElementType::of<controls::Gauge::Content>());
  Element colour =
      ColorPicker("Accent", composition::Binding<theme::Colour>::constant(
                                theme::Colour::hex(0x007AFF)));
  EXPECT_EQ(colour.type(),
            composition::ElementType::of<controls::ColorPicker::Content>());
  Element date = DatePicker(date_picker_style(DatePickerStyle::graphical()))(
      "Day",
      composition::Binding<std::chrono::sys_seconds>::constant(
          std::chrono::sys_seconds{}),
      DatePickerComponents::date);
  EXPECT_EQ(date.type(),
            composition::ElementType::of<controls::DatePicker::Content>());
  Element form = Form(form_style(FormStyle::columns()))(Text("x"));
  EXPECT_EQ(form.type(),
            composition::ElementType::of<structure::List::Content>());
  Element menu = Menu(menu_style(MenuStyle::borderless_button()))(
      "Actions", [] { return Element{Text("One")}; });
  EXPECT_EQ(menu.type(),
            composition::ElementType::of<controls::Menu::Content>());
}

}
