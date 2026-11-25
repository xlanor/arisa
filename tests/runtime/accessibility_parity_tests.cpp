#include <focus_fixture.hpp>

#include <arisa/components/accessibility_tree.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_action_category.hpp>
#include <arisa/composition/accessibility_action_kind.hpp>
#include <arisa/composition/accessibility_adjustment_direction.hpp>
#include <arisa/composition/accessibility_chart_descriptor.hpp>
#include <arisa/composition/accessibility_custom_content_key.hpp>
#include <arisa/composition/accessibility_direct_touch_options.hpp>
#include <arisa/composition/accessibility_focus_state.hpp>
#include <arisa/composition/accessibility_labeled_pair_role.hpp>
#include <arisa/composition/accessibility_rotor_entry.hpp>
#include <arisa/composition/accessibility_system_rotor.hpp>
#include <arisa/composition/accessibility_technologies.hpp>
#include <arisa/composition/accessibility_zoom_gesture_action.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class AccessibilityParityTest : public FocusFixture {
 protected:
  using Kind = composition::AccessibilityActionKind;
  using Category = composition::AccessibilityActionCategory;
  using Technologies = composition::AccessibilityTechnologies;
  using Traits = composition::AccessibilityTraits;
  using Trait = composition::AccessibilityTrait;
  using Element = composition::Element;

  static auto element(Application& application, std::string_view key)
      -> std::optional<components::AccessibilityElement> {
    const auto node = node_of(application, key);
    if (!node.has_value()) {
      ADD_FAILURE() << "no node keyed " << key;
      return std::nullopt;
    }
    return components::AccessibilityTree{application.root().tree()}.element(
        *node);
  }

  static auto german() -> text::Locale {
    return text::Locale::create("de-DE").value();
  }

  static auto catalog() -> text::StringCatalog {
    text::StringCatalog::Table table;
    table.emplace("Archive", text::StringCatalog::Entry{"Archivieren"});
    table.emplace("Play", text::StringCatalog::Entry{"Abspielen"});
    table.emplace("Starts the game",
                  text::StringCatalog::Entry{"Startet das Spiel"});
    table.emplace("Loud", text::StringCatalog::Entry{"Laut"});
    return text::StringCatalog{}.with(
        german(), text::StringCatalog::default_table, std::move(table));
  }

  static auto localised(Element content) -> Element {
    return std::move(content)
        .environment(components::Environment::locale, german())
        .environment(components::Environment::string_catalog,
                     std::optional<text::StringCatalog>{catalog()});
  }

  static constexpr composition::Namespace space{"accessibility-parity"};

  struct Chart {
    auto make_chart_descriptor() const
        -> composition::AccessibilityChartDescriptor {
      return composition::AccessibilityChartDescriptor{
          .title = "Sales",
          .summary = "Rising",
          .x_axis = {.title = "Month"},
          .y_axis = {.title = "Units"},
          .series = {{.name = "2026", .values = {1.0, 2.0, 4.0}}}};
    }
  };

  static auto board() -> Element {
    return components::ZStack()
        .frame({.width = 100.0F, .height = 50.0F})
        .accessibility_label("Board")
        .accessibility_activation_point(motion::UnitPoint::top_leading)
        .accessibility_drag_point(motion::UnitPoint::center, "Card")
        .accessibility_drag_point(motion::UnitPoint::center, "Off", false)
        .accessibility_drop_point(motion::UnitPoint::bottom, "Column", true)
        .accessibility_drop_point(motion::UnitPoint::top, "Pile")
        .accessibility_responds_to_user_interaction()
        .accessibility_ignores_invert_colors()
        .accessibility_direct_touch(
            true, composition::AccessibilityDirectTouchOptions::silent_on_touch)
        .accessibility_shows_large_content_viewer()
        .accessibility_chart_descriptor(Chart{})
        .accessibility_scroll_status("Page 2 of 3")
        .accessibility_custom_content(
            composition::AccessibilityCustomContentKey{"Owner", "owner"}, "Mia")
        .accessibility_text_content_type(
            composition::AccessibilityTextContentType::spreadsheet)
        .accessibility_input_labels({"Board", "Table"})
        .assistive_access_navigation_icon("square.grid.2x2")
        .id("board");
  }

  static auto point() -> Element {
    return components::ZStack()
        .frame({.width = 10.0F, .height = 10.0F})
        .accessibility_label("Point")
        .accessibility_activation_point(
            composition::Point{.x = 2.0F, .y = 3.0F}, true)
        .accessibility_activation_point(
            composition::Point{.x = 9.0F, .y = 9.0F}, false)
        .accessibility_activation_point(motion::UnitPoint::center, false)
        .accessibility_responds_to_user_interaction(false, true)
        .accessibility_input_labels({"Never"}, false)
        .accessibility_shows_large_content_viewer(
            [] { return Element{components::Text("Big")}; })
        .assistive_access_navigation_icon(
            Element{components::Image::system_name("star")})
        .id("point");
  }

  static auto pairs(composition::Namespace::ID id) -> Element {
    return components::VStack(
        components::Text("Name")
            .accessibility_labeled_pair(
                composition::AccessibilityLabeledPairRole::label, "name", id)
            .id("name-label"),
        components::ZStack()
            .frame({.width = 60.0F, .height = 20.0F})
            .accessibility_element()
            .accessibility_labeled_pair(
                composition::AccessibilityLabeledPairRole::content, "name", id)
            .accessibility_linked_group("pair", id)
            .id("name-field"),
        components::Text("Linked")
            .accessibility_linked_group("pair", id)
            .accessibility_rotor_entry("entry", id)
            .id("linked"));
  }

  static auto flagged(composition::Namespace::ID id) -> Element {
    return components::VStack(board(), point(), pairs(id));
  }

  static auto reference() -> std::map<std::string, std::string> {
    std::ifstream stream{
        std::filesystem::path{ARISA_TEST_FIXTURES}.parent_path() /
        "swiftui_reference" / "metrics_accessibility.tsv"};
    std::map<std::string, std::string> found;
    std::string line;
    while (std::getline(stream, line)) {
      if (line.empty() || line.front() == '#') {
        continue;
      }
      std::istringstream fields{line};
      std::string key;
      std::string light;
      std::getline(fields, key, '\t');
      std::getline(fields, light, '\t');
      found.emplace(key, light);
    }
    return found;
  }

  static auto joined(const std::vector<components::AccessibilityElement>& all)
      -> std::string {
    std::string text;
    for (const auto& each : all) {
      text += (text.empty() ? "" : "|") + std::string{each.label()};
    }
    return text;
  }

  static auto spoken(Element content) -> std::string {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, std::move(content));
    return joined(
        components::AccessibilityTree{application.root().tree()}.elements());
  }

  struct Mailbox {
    int id;
    std::string name;
  };
};

TEST_F(AccessibilityParityTest, ActionKindsAndCategoriesCompare) {
  EXPECT_EQ(Kind::default_, Kind::default_);
  EXPECT_NE(Kind::default_, Kind::escape);
  EXPECT_NE(Kind::magic_tap, Kind::delete_);
  EXPECT_NE(Kind::show_menu, Kind::delete_);
  EXPECT_EQ(Kind::escape.kind(), Kind::Kind::escape);
  EXPECT_EQ(Kind::magic_tap.kind(), Kind::Kind::magic_tap);
  EXPECT_EQ(Kind::delete_.kind(), Kind::Kind::remove);
  EXPECT_EQ(Kind::show_menu.kind(), Kind::Kind::show_menu);
  const Kind named{"Archive"};
  EXPECT_EQ(named.kind(), Kind::Kind::named);
  EXPECT_EQ(named, Kind{"Archive"});
  EXPECT_NE(named, Kind{"Flag"});
  EXPECT_TRUE(Category::edit.is_edit());
  EXPECT_FALSE(Category::default_.is_edit());
  EXPECT_NE(Category::edit, Category::default_);
  const Category custom{"Sharing"};
  EXPECT_EQ(custom, Category{"Sharing"});
  EXPECT_EQ(custom.name()->verbatim(), "Sharing");
}

TEST_F(AccessibilityParityTest, SetAlgebraCoversTechnologiesAndTraits) {
  Technologies running;
  EXPECT_TRUE(running.empty());
  const auto inserted = running.insert(Technologies::voice_over);
  EXPECT_TRUE(inserted.first);
  EXPECT_FALSE(running.insert(Technologies::voice_over).first);
  EXPECT_TRUE(running.contains(Technologies::voice_over));
  EXPECT_FALSE(running.contains(Technologies::switch_control));
  const Technologies both = running.union_with(Technologies::switch_control);
  EXPECT_EQ(both.intersection(Technologies::switch_control),
            Technologies::switch_control);
  EXPECT_EQ(both.symmetric_difference(Technologies::voice_over),
            Technologies::switch_control);
  Technologies formed = both;
  formed.form_intersection(Technologies::voice_over);
  EXPECT_EQ(formed, Technologies::voice_over);
  formed.form_symmetric_difference(Technologies::switch_control);
  EXPECT_EQ(formed, both);
  formed.form_union(Technologies::voice_over);
  EXPECT_EQ(formed, both);
  EXPECT_EQ(formed.remove(Technologies::voice_over), Technologies::voice_over);
  EXPECT_EQ(formed.remove(Technologies::voice_over), std::nullopt);
  EXPECT_EQ(formed.update(Technologies::switch_control),
            Technologies::switch_control);
  EXPECT_EQ(formed.update(Technologies::voice_over), std::nullopt);
  const Traits traits{Trait::is_button, Trait::is_tab_bar};
  EXPECT_TRUE(traits.contains(Trait::is_tab_bar));
  EXPECT_EQ(traits.intersection(Trait::is_tab_bar), Traits{Trait::is_tab_bar});
  EXPECT_EQ(traits.symmetric_difference(Trait::is_button),
            Traits{Trait::is_tab_bar});
  Traits held = traits;
  held.form_intersection(Trait::is_button);
  EXPECT_EQ(held, Traits{Trait::is_button});
  held.form_symmetric_difference(Traits{Trait::is_button, Trait::is_header});
  EXPECT_EQ(held, Traits{Trait::is_header});
  EXPECT_EQ(Traits::name(Trait::is_tab_bar), "tab bar");
}

TEST_F(AccessibilityParityTest, ValueTypesCarryTheirFields) {
  EXPECT_NE(composition::AccessibilityAdjustmentDirection::increment,
            composition::AccessibilityAdjustmentDirection::decrement);
  EXPECT_NE(composition::AccessibilityLabeledPairRole::label,
            composition::AccessibilityLabeledPairRole::content);
  const composition::AccessibilityCustomContentKey keyed{"Age", "age"};
  EXPECT_EQ(keyed.id(), "age");
  EXPECT_EQ(composition::AccessibilityCustomContentKey{"Age"}.id(), "Age");
  using Touch = composition::AccessibilityDirectTouchOptions;
  const Touch options = Touch::silent_on_touch | Touch::requires_activation;
  EXPECT_TRUE(options.contains(Touch::silent_on_touch));
  EXPECT_TRUE(options.contains(Touch::requires_activation));
  EXPECT_FALSE(Touch{}.contains(Touch::silent_on_touch));
  const composition::AccessibilityZoomGestureAction zoom{
      .direction =
          composition::AccessibilityZoomGestureAction::Direction::zoom_out,
      .location = motion::UnitPoint::top_leading,
      .point = {.x = 4.0F, .y = 8.0F}};
  EXPECT_EQ(zoom.direction,
            composition::AccessibilityZoomGestureAction::Direction::zoom_out);
  EXPECT_NE(zoom.direction,
            composition::AccessibilityZoomGestureAction::Direction::zoom_in);
  EXPECT_EQ(zoom.location, motion::UnitPoint::top_leading);
  EXPECT_FLOAT_EQ(zoom.point.y, 8.0F);
  using Rotor = composition::AccessibilitySystemRotor;
  EXPECT_EQ(Rotor::links().name(), "Links");
  EXPECT_EQ(Rotor::links(true).name(), "Visited links");
  EXPECT_EQ(Rotor::headings().name(), "Headings");
  EXPECT_EQ(Rotor::headings(composition::AccessibilityHeadingLevel::h2).name(),
            "Heading level 2");
  for (const auto& [rotor, name] :
       {std::pair{Rotor::bold_text, "Bold text"},
        std::pair{Rotor::italic_text, "Italic text"},
        std::pair{Rotor::underline_text, "Underlined text"},
        std::pair{Rotor::misspelled_words, "Misspelled words"},
        std::pair{Rotor::images, "Images"},
        std::pair{Rotor::text_fields, "Text fields"},
        std::pair{Rotor::tables, "Tables"}, std::pair{Rotor::lists, "Lists"},
        std::pair{Rotor::landmarks, "Landmarks"}}) {
    EXPECT_EQ(rotor.name(), name);
  }
  EXPECT_TRUE(Rotor::headings(composition::AccessibilityHeadingLevel::h2)
                  .matches(Traits{Trait::is_header},
                           composition::AccessibilityHeadingLevel::h2));
  EXPECT_FALSE(Rotor::headings(composition::AccessibilityHeadingLevel::h2)
                   .matches(Traits{Trait::is_header},
                            composition::AccessibilityHeadingLevel::h1));
  EXPECT_TRUE(Rotor::images.matches(Traits{Trait::is_image}, std::nullopt));
  bool prepared = false;
  const composition::AccessibilityRotorEntry plain{
      "Inbox", 7, std::nullopt, [&prepared] { prepared = true; }};
  plain.prepare();
  EXPECT_TRUE(prepared);
  EXPECT_EQ(plain.id(), composition::ElementKey{7});
  EXPECT_FALSE(plain.space().has_value());
  const composition::AccessibilityRotorEntry spaced{"Sent", "sent", space.id()};
  EXPECT_EQ(spaced.space(), space.id());
  const composition::AccessibilityRotorEntry ranged{
      std::nullopt, composition::AccessibilityRotorEntry::TextRange{2, 5}};
  EXPECT_FALSE(ranged.id().has_value());
  EXPECT_EQ(ranged.text_range()->upper, 5U);
}

TEST_F(AccessibilityParityTest, LabelsValuesAndHintsAreLocalised) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      localised(components::VStack(
          Element{components::Button("x", [] {})}
              .accessibility_label(text::LocalizedStringKey{"Play"})
              .accessibility_hint(text::LocalizedStringKey{"Starts the game"})
              .accessibility_value(
                  components::Text(text::LocalizedStringKey{"Loud"}))
              .id("play"),
          Element{components::Button("y", [] {})}
              .accessibility_label("Kept", true)
              .accessibility_label("Ignored", false)
              .accessibility_hint("Hint", true)
              .accessibility_hint("Ignored hint", false)
              .accessibility_value("1", true)
              .accessibility_value("2", false)
              .accessibility_identifier("kept-id", true)
              .accessibility_identifier("ignored-id", false)
              .id("flags"),
          components::Text("hidden")
              .accessibility_hidden(true, true)
              .id("hidden"),
          components::Text("shown")
              .accessibility_hidden(true, false)
              .id("shown"),
          components::Text(1234.5, text::NumberFormatStyle::number())
              .id("number"))));
  const auto play = element(application, "play");
  ASSERT_TRUE(play.has_value());
  EXPECT_EQ(play->label(), "Abspielen");
  EXPECT_EQ(play->hint(), std::optional<std::string>{"Startet das Spiel"});
  EXPECT_EQ(play->value(), std::optional<std::string>{"Laut"});
  const auto flags = element(application, "flags");
  ASSERT_TRUE(flags.has_value());
  EXPECT_EQ(flags->label(), "Kept");
  EXPECT_EQ(flags->hint(), std::optional<std::string>{"Hint"});
  EXPECT_EQ(flags->value(), std::optional<std::string>{"1"});
  EXPECT_EQ(flags->identifier(), std::optional<std::string>{"kept-id"});
  EXPECT_FALSE(element(application, "hidden").has_value());
  EXPECT_TRUE(element(application, "shown").has_value());
  const auto number = element(application, "number");
  ASSERT_TRUE(number.has_value());
  EXPECT_EQ(number->label(), "1.234,5");
}

TEST_F(AccessibilityParityTest, FormattedTextIsReadBeforeLayout) {
  const composition::Element made =
      components::Text(1234.5, text::NumberFormatStyle::number());
  const composition::AccessibilityDefaults defaults =
      made.content().localized_accessibility(german(), text::StringCatalog{});
  EXPECT_EQ(defaults.label, std::optional<std::string>{"1.234,5"});
  const composition::AccessibilityText label{
      components::Text(text::LocalizedStringKey{"Play"})};
  EXPECT_EQ(label.resolve(german(), catalog()), "Abspielen");
}

TEST_F(AccessibilityParityTest, OrderAndSyntheticElementsMatchSwiftUI) {
  const std::map<std::string, std::string> measured = reference();
  ASSERT_FALSE(measured.empty());
  EXPECT_EQ(
      spoken(components::VStack(
          components::Text("Third"),
          components::Text("First").accessibility_sort_priority(2.0),
          components::VStack(
              components::Text("Inner low"),
              components::Text("Inner high").accessibility_sort_priority(1.0))
              .accessibility_element(
                  composition::AccessibilityChildBehavior::contain)
              .accessibility_sort_priority(1.0))),
      measured.at("accessibility.sort_priority"));
  EXPECT_EQ(
      spoken(components::VStack(
                 components::Text("Low"),
                 components::Text("High").accessibility_sort_priority(5.0))
                 .accessibility_element(
                     composition::AccessibilityChildBehavior::contain)),
      measured.at("accessibility.sort_priority_contained"));
  EXPECT_EQ(
      spoken(components::Text("Inbox").accessibility_label([](Element label) {
        return Element{
            components::HStack(std::move(label), components::Text("3 unread"))};
      })),
      measured.at("accessibility.label_content"));
  EXPECT_EQ(
      spoken(components::ZStack()
                 .frame({.width = 80.0F, .height = 40.0F})
                 .accessibility_element(
                     composition::AccessibilityChildBehavior::contain)
                 .accessibility_children([] {
                   return Element{components::HStack(
                       components::Text("North"), components::Text("South"))};
                 })),
      measured.at("accessibility.children"));
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::VStack(Element{components::Button("Mail", [] {})}
                                   .accessibility_action("Bravo", [] {})
                                   .accessibility_actions([] {
                                     return Element{components::VStack(
                                         components::Button("Alpha", [] {}),
                                         components::Button("Charlie", [] {}))};
                                   })
                                   .accessibility_action("Delta", [] {})
                                   .id("mail")));
  const auto mail = element(application, "mail");
  ASSERT_TRUE(mail.has_value());
  std::string order;
  for (const auto& action : mail->actions()) {
    order += (order.empty() ? "" : "|") + std::string{action.name()};
  }
  EXPECT_EQ(order, measured.at("accessibility.action_order"));
}

TEST_F(AccessibilityParityTest, CustomActionsPerformTheirHandlers) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<std::vector<std::string>>();
  const auto note = [log](std::string what) {
    return [log, what = std::move(what)] { log->push_back(what); };
  };
  Application application =
      start(providers,
            localised(components::VStack(
                Element{components::Button("Mail", [] {})}
                    .accessibility_action(note("default"))
                    .accessibility_action(Kind::escape, note("escape"))
                    .accessibility_action(Kind::magic_tap, note("magic"))
                    .accessibility_action(Kind::delete_, note("delete"))
                    .accessibility_action(Kind::show_menu, note("menu"))
                    .accessibility_action(text::LocalizedStringKey{"Archive"},
                                          note("archive"))
                    .accessibility_action(
                        note("labelled"),
                        [] { return Element{components::Text("Pin")}; })
                    .accessibility_actions([note] {
                      return Element{components::VStack(
                          components::Button("Flag", note("flag")),
                          components::Button("Move", note("move")))};
                    })
                    .accessibility_actions(Category::edit,
                                           [note] {
                                             return Element{components::Button(
                                                 "Rename", note("rename"))};
                                           })
                    .id("mail"))));
  const auto mail = element(application, "mail");
  ASSERT_TRUE(mail.has_value());
  std::vector<std::string> names;
  for (const auto& action : mail->actions()) {
    names.emplace_back(action.name());
  }
  EXPECT_EQ(names,
            (std::vector<std::string>{"Rename", "Move", "Flag", "Pin",
                                      "Archivieren", "Show menu", "Delete",
                                      "Magic tap", "Escape", "Activate"}));
  EXPECT_EQ(mail->actions()[0].category(), std::optional<std::string>{"Edit"});
  composition::Tree& tree = application.root().tree();
  for (const auto& action : mail->actions()) {
    EXPECT_TRUE(action.perform(tree)) << action.name();
  }
  EXPECT_EQ(*log, (std::vector<std::string>{
                      "rename", "move", "flag", "labelled", "archive", "menu",
                      "delete", "magic", "escape", "default"}));
  log->clear();
  EXPECT_TRUE(mail->activate(tree));
  EXPECT_EQ(*log, std::vector<std::string>{"default"});
}

TEST_F(AccessibilityParityTest, AdjustScrollAndZoomReachTheirHandlers) {
  provider::headless::HeadlessProviderFactory providers;
  auto adjusted = std::make_shared<int>(0);
  auto scrolled = std::make_shared<std::optional<theme::Edge>>();
  auto zoomed = std::make_shared<
      std::optional<composition::AccessibilityZoomGestureAction>>();
  Application application = start(
      providers,
      components::VStack(
          components::ZStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .accessibility_label("Stars")
              .accessibility_adjustable_action(
                  [adjusted](
                      composition::AccessibilityAdjustmentDirection direction) {
                    *adjusted +=
                        direction ==
                                composition::AccessibilityAdjustmentDirection::
                                    increment
                            ? 1
                            : -10;
                  })
              .accessibility_scroll_action(
                  [scrolled](theme::Edge edge) { *scrolled = edge; })
              .accessibility_zoom_action(
                  [zoomed](composition::AccessibilityZoomGestureAction action) {
                    *zoomed = action;
                  })
              .id("stars")));
  const auto stars = element(application, "stars");
  ASSERT_TRUE(stars.has_value());
  composition::Tree& tree = application.root().tree();
  EXPECT_TRUE(stars->adjustable());
  EXPECT_TRUE(stars->adjust(
      tree, composition::AccessibilityAdjustmentDirection::increment));
  EXPECT_TRUE(stars->adjust(
      tree, composition::AccessibilityAdjustmentDirection::decrement));
  EXPECT_EQ(*adjusted, -9);
  EXPECT_TRUE(stars->scroll(theme::Edge::bottom));
  EXPECT_EQ(*scrolled, theme::Edge::bottom);
  EXPECT_TRUE(stars->zoom(composition::AccessibilityZoomGestureAction{
      .direction =
          composition::AccessibilityZoomGestureAction::Direction::zoom_in}));
  ASSERT_TRUE(zoomed->has_value());
  EXPECT_EQ((*zoomed)->direction,
            composition::AccessibilityZoomGestureAction::Direction::zoom_in);
}

TEST_F(AccessibilityParityTest, ChildrenRepresentationAndLabelContent) {
  provider::headless::HeadlessProviderFactory providers;
  auto level = std::make_shared<double>(0.5);
  Application application = start(
      providers,
      components::VStack(
          components::ZStack()
              .frame({.width = 120.0F, .height = 12.0F})
              .accessibility_representation([level] {
                return Element{controls::Slider(composition::Binding<double>{
                                   [level] { return *level; },
                                   [level](double value) { *level = value; }})}
                    .accessibility_label("Volume");
              })
              .id("volume"),
          components::ZStack()
              .frame({.width = 80.0F, .height = 40.0F})
              .accessibility_element(
                  composition::AccessibilityChildBehavior::contain)
              .accessibility_children([] {
                return Element{components::HStack(components::Text("North"),
                                                  components::Text("South"))};
              })
              .id("map"),
          components::Text("Inbox")
              .accessibility_label([](Element label) {
                return Element{components::HStack(
                    std::move(label), components::Text("3 unread"))};
              })
              .id("inbox"),
          components::Button("Go", [] {}).id("focus-target")));
  const components::AccessibilityTree tree{application.root().tree()};
  std::vector<std::string> labels;
  for (const auto& each : tree.elements()) {
    labels.emplace_back(each.label());
  }
  EXPECT_EQ(labels, (std::vector<std::string>{"Volume", "North", "South",
                                              "Inbox, 3 unread", "Go"}));
  const auto volume = tree.element(*node_of(application, "volume"));
  ASSERT_TRUE(volume.has_value());
  EXPECT_EQ(volume->label(), "Volume");
  EXPECT_EQ(volume->value(), std::optional<std::string>{"50%"});
  EXPECT_TRUE(volume->adjustable());
  EXPECT_TRUE(
      volume->adjust(application.root().tree(),
                     composition::AccessibilityAdjustmentDirection::increment));
  EXPECT_GT(*level, 0.5);
  press(providers, application, provider::Button::dpad_down);
  const auto focused = application.focused();
  ASSERT_TRUE(focused.has_value());
  EXPECT_EQ(*focused, *node_of(application, "focus-target"));
}

TEST_F(AccessibilityParityTest, SortPriorityOrdersElementsInTheirContainer) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::Text("Third"),
          components::Text("First").accessibility_sort_priority(2.0),
          components::VStack(
              components::Text("Inner low"),
              components::Text("Inner high").accessibility_sort_priority(1.0))
              .accessibility_element(
                  composition::AccessibilityChildBehavior::contain)
              .accessibility_sort_priority(1.0)));
  std::vector<std::string> labels;
  for (const auto& each :
       components::AccessibilityTree{application.root().tree()}.elements()) {
    labels.emplace_back(each.label());
  }
  EXPECT_EQ(labels, (std::vector<std::string>{"First", "Inner high",
                                              "Inner low", "Third"}));
}

TEST_F(AccessibilityParityTest, PointsPairsGroupsAndFlagsReachTheElement) {
  provider::headless::HeadlessProviderFactory providers;
  const composition::Namespace::ID id = space.id();
  Application application = start(providers, flagged(id));
  const auto board = element(application, "board");
  ASSERT_TRUE(board.has_value());
  const composition::Rect frame = board->frame();
  ASSERT_TRUE(board->activation_point().has_value());
  EXPECT_FLOAT_EQ(board->activation_point()->x, frame.x);
  ASSERT_EQ(board->drag_points().size(), 1U);
  EXPECT_EQ(board->drag_points()[0].description, "Card");
  EXPECT_FLOAT_EQ(board->drag_points()[0].point.x, frame.x + 50.0F);
  ASSERT_EQ(board->drop_points().size(), 2U);
  EXPECT_EQ(board->drop_points()[0].description, "Column");
  EXPECT_TRUE(board->responds_to_user_interaction());
  EXPECT_TRUE(board->ignores_invert_colors());
  ASSERT_TRUE(board->direct_touch().has_value());
  EXPECT_TRUE(board->direct_touch()->contains(
      composition::AccessibilityDirectTouchOptions::silent_on_touch));
  EXPECT_TRUE(board->traits().contains(Trait::allows_direct_interaction));
  EXPECT_TRUE(board->shows_large_content_viewer());
  EXPECT_EQ(board->chart()->title, "Sales");
  EXPECT_EQ(board->scroll_status(), std::optional<std::string>{"Page 2 of 3"});
  ASSERT_EQ(board->custom_content().size(), 1U);
  EXPECT_EQ(board->custom_content()[0].value, "Mia");
  EXPECT_EQ(board->text_content_type(),
            composition::AccessibilityTextContentType::spreadsheet);
  EXPECT_EQ(board->input_labels(),
            (std::vector<std::string>{"Board", "Table"}));
  EXPECT_EQ(board->navigation_icon()->system_image, "square.grid.2x2");
  EXPECT_FALSE(board->navigation_icon()->resource);
  EXPECT_NE(board->description().find("Sales"), std::string::npos);
  const auto point = element(application, "point");
  ASSERT_TRUE(point.has_value());
  EXPECT_FLOAT_EQ(point->activation_point()->x, point->frame().x + 2.0F);
  EXPECT_FALSE(point->responds_to_user_interaction());
  EXPECT_TRUE(point->input_labels().empty());
  ASSERT_TRUE(point->large_content().has_value());
  EXPECT_TRUE(point->navigation_icon()->resource);
  const components::AccessibilityTree tree{application.root().tree()};
  const auto field = tree.find("name-field");
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->label(), "Name");
  EXPECT_EQ(tree.paired(*field)->label(), "Name");
  EXPECT_EQ(tree.linked(*field).size(), 2U);
  const auto entry =
      tree.target(composition::AccessibilityRotorEntry{"Entry", "entry", id});
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->label(), "Linked");
}

TEST_F(AccessibilityParityTest, RotorsResolveTheirEntries) {
  provider::headless::HeadlessProviderFactory providers;
  const std::vector<Mailbox> boxes{{1, "Inbox"}, {2, "Sent"}};
  Application application = start(
      providers,
      components::VStack(
          components::Text("Inbox").id(1), components::Text("Sent").id(2),
          components::Text("Long text body")
              .accessibility_rotor(
                  "Words",
                  std::vector<composition::AccessibilityRotorEntry::TextRange>{
                      {0, 4}, {5, 9}})
              .id("body"))
          .accessibility_element(
              composition::AccessibilityChildBehavior::contain)
          .accessibility_rotor("Boxes", boxes, &Mailbox::id, &Mailbox::name)
          .accessibility_rotor(composition::AccessibilitySystemRotor::links(),
                               boxes, &Mailbox::name)
          .accessibility_rotor(
              composition::AccessibilitySystemRotor::headings(), [] {
                return std::vector<composition::AccessibilityRotorEntry>{
                    composition::AccessibilityRotorEntry{"Top", 2}};
              }));
  const components::AccessibilityTree tree{application.root().tree()};
  const std::vector<composition::AccessibilityRotor> rotors = tree.rotors();
  ASSERT_EQ(rotors.size(), 4U);
  std::vector<std::string> names;
  for (const auto& rotor : rotors) {
    names.push_back(tree.label(rotor));
  }
  EXPECT_EQ(names.size(), 4U);
  for (const auto& rotor : rotors) {
    const auto entries = tree.entries(rotor);
    ASSERT_FALSE(entries.empty());
    if (tree.label(rotor) == "Words") {
      EXPECT_EQ(entries[1].text_range()->lower, 5U);
    } else if (tree.label(rotor) == "Headings") {
      EXPECT_EQ(tree.target(entries[0])->label(), "Sent");
    } else {
      EXPECT_EQ(tree.label(entries[0]), "Inbox");
      EXPECT_EQ(tree.target(entries[1])->label(), "Sent");
    }
  }
}

TEST_F(AccessibilityParityTest, SpeechModifiersReachTheElement) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(
                     components::Text("ABC")
                         .speech_spells_out_characters()
                         .speech_adjusted_pitch(1.5)
                         .speech_always_includes_punctuation()
                         .speech_announcements_queued()
                         .id("text"),
                     components::VStack(components::Text("Inner").id("inner"))
                         .speech_spells_out_characters()
                         .speech_adjusted_pitch(0.5)
                         .speech_always_includes_punctuation()
                         .speech_announcements_queued(),
                     components::Text("Plain").id("plain")));
  for (const std::string_view key : {"text", "inner"}) {
    const auto made = element(application, key);
    ASSERT_TRUE(made.has_value());
    EXPECT_TRUE(made->speech().spells_out()) << key;
    EXPECT_TRUE(made->speech().punctuated()) << key;
    EXPECT_TRUE(made->speech().queued()) << key;
  }
  EXPECT_DOUBLE_EQ(element(application, "text")->speech().pitch(), 1.5);
  EXPECT_DOUBLE_EQ(element(application, "inner")->speech().pitch(), 0.5);
  EXPECT_FALSE(element(application, "plain")->speech().spells_out());
  const composition::AccessibilityText label{
      components::Text("X").speech_spells_out_characters()};
  ASSERT_TRUE(label.speech().has_value());
  EXPECT_TRUE(label.speech()->spells_out());
}

TEST_F(AccessibilityParityTest, TabInputLabelsReachTheTabItem) {
  provider::headless::HeadlessProviderFactory providers;
  using structure::Tab;
  Application application = start(
      providers,
      structure::TabView(
          Tab("One", "house", 0, [] { return Element{components::Text("1")}; })
              .accessibility_identifier("tab-one")
              .accessibility_input_labels({"Home", "Start"}),
          Tab("Two", "star", 1, [] { return Element{components::Text("2")}; })
              .accessibility_identifier("tab-two")
              .accessibility_input_labels({"Never"}, false)));
  frames(application, 3);
  const components::AccessibilityTree tree{application.root().tree()};
  ASSERT_TRUE(tree.find("tab-one").has_value());
  EXPECT_EQ(tree.find("tab-one")->input_labels(),
            (std::vector<std::string>{"Home", "Start"}));
  EXPECT_TRUE(tree.find("tab-two")->input_labels().empty());
}

class FocusProbe final : public composition::Component {
 public:
  static constexpr composition::AccessibilityFocusState<bool> summary{
      "accessibility-summary"};
  static constexpr composition::AccessibilityFocusState<int> chosen{
      "accessibility-chosen",
      composition::AccessibilityTechnologies::voice_over};

  struct Seen {
    std::optional<composition::AccessibilityFocusBinding<bool>> summary;
    std::optional<composition::AccessibilityFocusBinding<int>> chosen;
  };

  explicit FocusProbe(std::shared_ptr<Seen> seen) : seen_(std::move(seen)) {}

  auto operator==(const FocusProbe&) const -> bool { return true; }

  auto body(composition::Context& context) const
      -> composition::Element override {
    const auto bound = context.state(summary);
    const auto picked = context.state(chosen);
    seen_->summary = bound;
    seen_->chosen = picked;
    return components::VStack(
        composition::Element{components::Button("A", [] {})}.id("a"),
        composition::Element{components::Button("B", [] {})}
            .accessibility_focused(bound)
            .id("b"),
        composition::Element{components::Button("C", [] {})}
            .accessibility_focused(picked, 3)
            .id("c"),
        composition::Element{components::Button("D", [] {})}
            .accessibility_default_focus(picked, 4)
            .accessibility_default_focus(bound, true)
            .id("d"));
  }

 private:
  std::shared_ptr<Seen> seen_;
};

TEST_F(AccessibilityParityTest, AccessibilityFocusFollowsTheRunningTechnology) {
  {
    provider::headless::HeadlessProviderFactory providers;
    auto seen = std::make_shared<FocusProbe::Seen>();
    Application application = start(providers, FocusProbe{seen});
    ASSERT_TRUE(seen->summary.has_value());
    EXPECT_FALSE(seen->summary->active());
    seen->summary->set(true);
    frames(application, 3);
    EXPECT_NE(application.focused(), node_of(application, "b"));
  }
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<FocusProbe::Seen>();
  Application application = start(
      providers,
      components::VStack(FocusProbe{seen})
          .environment(components::Environment::accessibility_technologies,
                       composition::AccessibilityTechnologies::voice_over));
  ASSERT_TRUE(seen->summary.has_value());
  EXPECT_TRUE(seen->summary->active());
  EXPECT_TRUE(seen->chosen->active());
  EXPECT_EQ(seen->summary->projected_value().identity(),
            seen->summary->identity());
  seen->summary->set(true);
  frames(application, 3);
  EXPECT_EQ(application.focused(), node_of(application, "b"));
  EXPECT_TRUE(seen->summary->get());
  seen->chosen->set(3);
  frames(application, 3);
  EXPECT_EQ(application.focused(), node_of(application, "c"));
  EXPECT_FALSE(seen->summary->get());
  EXPECT_EQ(seen->chosen->get(), std::optional<int>{3});
}

TEST_F(AccessibilityParityTest, ShowBordersOutlinesGlass) {
  const auto strokes = [](bool borders) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        components::ZStack()
            .frame({.width = 120.0F, .height = 40.0F})
            .glass_effect()
            .environment(components::Environment::accessibility_show_borders,
                         borders)
            .environment(components::Environment::
                             accessibility_reduce_highlighting_effects,
                         borders));
    std::size_t count = 0;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      count += vertex.kind == provider::ShapeKind::stroke ? 1U : 0U;
    }
    return count;
  };
  EXPECT_GT(strokes(true), strokes(false));
}

}
