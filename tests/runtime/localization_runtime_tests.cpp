#include <focus_fixture.hpp>

#include <arisa/components/accessibility_tree.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/calendar.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/time_zone.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/structure/file_browser.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class LocalizationRuntimeTest : public FocusFixture {
 protected:
  static void load_fonts(
      provider::headless::HeadlessProviderFactory& providers) {
    std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} /
                             "arisa/fonts/InterVariable.ttf",
                         std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    ASSERT_FALSE(bytes.empty());
    providers.assets().add(
        *provider::AssetPath::create("arisa/fonts/InterVariable.ttf"), bytes);
  }

  static auto box(float width, std::string key) -> composition::Element {
    return components::ZStack()
        .frame({.width = width, .height = 20.0F})
        .background(theme::Colour::hex(0x2060FF), theme::Shape::rectangle())
        .id(std::move(key));
  }

  static auto x_of(Application& application, std::string_view key) -> float {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame().x : -1.0F;
  }

  static auto rtl(composition::Element content) -> composition::Element {
    return std::move(content).environment(
        components::Environment::layout_direction,
        components::LayoutDirection::right_to_left);
  }

  static auto spoken(Application& application, std::string_view key)
      -> std::string {
    const auto node = node_of(application, key);
    if (!node.has_value()) {
      return {};
    }
    const auto element =
        components::AccessibilityTree{application.root().tree()}.element(*node);
    return element.has_value() ? std::string{element->label()} : std::string{};
  }

  static auto localized(provider::headless::HeadlessProviderFactory& providers,
                        composition::Element content, std::string_view tag)
      -> Application {
    auto application = Application::create(
        providers, std::move(content),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = {},
            .atlas = {},
            .interaction = {},
            .localization = LocalizationOptions{
                .locale = text::Locale::create(tag), .strings = "strings"}});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    frames(started, 3);
    return started;
  }
};

TEST_F(LocalizationRuntimeTest, TheSystemLanguageSetsLocaleAndDirection) {
  provider::headless::HeadlessProviderFactory providers;
  providers.system_info().set_languages({"ar-EG", "en-US"});
  Application application = start(
      providers, components::HStack(components::HStackOptions{.spacing = 10.0F},
                                    box(40.0F, "a"), box(60.0F, "b"))
                     .frame({.width = 300.0F,
                             .alignment = composition::Alignment::leading})
                     .id("row"));
  const float origin = x_of(application, "row");
  EXPECT_FLOAT_EQ(x_of(application, "a") - origin, 260.0F);
  EXPECT_FLOAT_EQ(x_of(application, "b") - origin, 190.0F);
}

TEST_F(LocalizationRuntimeTest, StacksPaddingAndFramesMirrorLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      rtl(components::VStack(
          components::HStack(components::HStackOptions{.spacing = 10.0F},
                             box(40.0F, "a"), box(60.0F, "b"), box(80.0F, "c"))
              .frame({.width = 300.0F,
                      .alignment = composition::Alignment::leading})
              .id("stack"),
          components::ZStack(box(40.0F, "padded"))
              .padding(theme::Insets{.leading = 20.0F})
              .frame({.width = 200.0F,
                      .alignment = composition::Alignment::leading})
              .id("padding"),
          box(40.0F, "framed")
              .frame({.max_width = 200.0F,
                      .alignment = composition::Alignment::leading})
              .id("frame"))));
  const float stack = x_of(application, "stack");
  EXPECT_FLOAT_EQ(x_of(application, "a") - stack, 260.0F);
  EXPECT_FLOAT_EQ(x_of(application, "b") - stack, 190.0F);
  EXPECT_FLOAT_EQ(x_of(application, "c") - stack, 100.0F);
  EXPECT_FLOAT_EQ(x_of(application, "padded") - x_of(application, "padding"),
                  140.0F);
  EXPECT_FLOAT_EQ(x_of(application, "framed") - x_of(application, "frame"),
                  160.0F);
}

TEST_F(LocalizationRuntimeTest, GridColumnsMirrorInRightToLeft) {
  const auto grid = [] {
    return components::Grid(
               components::GridOptions{.horizontal_spacing = 10.0F},
               components::GridRow(box(40.0F, "a"), box(60.0F, "b"),
                                   box(80.0F, "c")))
        .frame({.width = 300.0F, .alignment = composition::Alignment::leading})
        .id("grid");
  };
  provider::headless::HeadlessProviderFactory ltr_providers;
  Application ltr = start(ltr_providers, grid());
  provider::headless::HeadlessProviderFactory rtl_providers;
  Application rtl_application = start(rtl_providers, rtl(grid()));
  const float ltr_origin = x_of(ltr, "grid");
  const float rtl_origin = x_of(rtl_application, "grid");
  for (const auto& [key, width] :
       {std::pair{"a", 40.0F}, std::pair{"b", 60.0F}, std::pair{"c", 80.0F}}) {
    EXPECT_FLOAT_EQ(x_of(rtl_application, key) - rtl_origin,
                    300.0F - (x_of(ltr, key) - ltr_origin) - width);
  }
  EXPECT_LT(x_of(rtl_application, "c"), x_of(rtl_application, "b"));
  EXPECT_LT(x_of(rtl_application, "b"), x_of(rtl_application, "a"));
}

TEST_F(LocalizationRuntimeTest,
       FixedBehaviourFollowsTheEnvironmentLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  const auto row = [](composition::LayoutDirectionBehavior behavior,
                      std::string prefix) {
    return components::HStack(components::HStackOptions{.spacing = 10.0F},
                              box(40.0F, prefix + "a"),
                              box(60.0F, prefix + "b"))
        .layout_direction_behavior(behavior)
        .frame({.width = 200.0F, .alignment = composition::Alignment::leading})
        .id(prefix + "row");
  };
  Application application = start(
      providers,
      rtl(components::VStack(
          row(composition::LayoutDirectionBehavior::fixed(), "fixed-"),
          row(composition::LayoutDirectionBehavior::mirrors(), "mirrors-"))));
  const float fixed = x_of(application, "fixed-row");
  EXPECT_FLOAT_EQ(x_of(application, "fixed-a") - fixed, 160.0F);
  EXPECT_FLOAT_EQ(x_of(application, "fixed-b") - fixed, 90.0F);
  const float mirrored = x_of(application, "mirrors-row");
  EXPECT_FLOAT_EQ(x_of(application, "mirrors-a") - mirrored, 0.0F);
  EXPECT_FLOAT_EQ(x_of(application, "mirrors-b") - mirrored, 50.0F);
}

TEST_F(LocalizationRuntimeTest, StringsAndFormatsResolveInTheChosenLocale) {
  provider::headless::HeadlessProviderFactory providers;
  providers.assets().add(
      *provider::AssetPath::create("strings/de.toml"),
      std::string_view{"\"Hello\" = \"Hallo\"\n"
                       "\"{0} files\" = { one = \"{0} Datei\", other = \"{0} "
                       "Dateien\" }\n"});
  Application application = localized(
      providers,
      components::VStack(
          components::Text(text::LocalizedStringKey{"Hello"}).id("hello"),
          components::Text(text::LocalizedStringKey{"{0} files", 1}).id("one"),
          components::Text(text::LocalizedStringKey{"{0} files", 3}).id("many"),
          components::Text(1234567.891, text::NumberFormatStyle::number())
              .id("number"),
          components::Text(text::LocalizedStringKey{"Untranslated"})
              .id("missing")),
      "de-DE");
  EXPECT_EQ(spoken(application, "hello"), "Hallo");
  EXPECT_EQ(spoken(application, "one"), "1 Datei");
  EXPECT_EQ(spoken(application, "many"), "3 Dateien");
  EXPECT_EQ(spoken(application, "number"), "1.234.567,891");
  EXPECT_EQ(spoken(application, "missing"), "Untranslated");
}

TEST_F(LocalizationRuntimeTest, FrameworkStringsComeFromTheBundledTables) {
  provider::headless::HeadlessProviderFactory providers;
  providers.assets().add(*provider::AssetPath::create("arisa/strings/ja.toml"),
                         std::string_view{"\"Done\" = \"完了\"\n"});
  Application application = localized(
      providers, components::Text(text::LocalizedStringKey{"Done"}).id("done"),
      "ja");
  EXPECT_EQ(spoken(application, "done"), "完了");
}

TEST_F(LocalizationRuntimeTest, LinksStepInReadingOrderRightToLeft) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, rtl(components::VStack(button("above", 120.0F, 30.0F),
                                        components::Text::markdown(
                                            "[one](https://example.com/1) and "
                                            "[two](https://example.com/2)")
                                            .id("links"))
                         .frame({.width = 600.0F})));
  press(providers, application, provider::Button::dpad_down);
  ASSERT_EQ(focused_key(application), "links");
  const auto node = node_named(application, "links");
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get().selected_link(), 0U);
  press(providers, application, provider::Button::dpad_left);
  EXPECT_EQ(node->get().selected_link(), 1U);
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(node->get().selected_link(), 0U);
}

TEST_F(LocalizationRuntimeTest, EnteringFromTheTrailingSideSelectsTheLastLink) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::HStack(components::Text::markdown(
                                        "[one](https://example.com/1) and "
                                        "[two](https://example.com/2)")
                                        .id("links"),
                                    button("after", 80.0F, 30.0F))
                     .frame({.width = 600.0F}));
  press(providers, application, provider::Button::dpad_right);
  press(providers, application, provider::Button::dpad_right);
  press(providers, application, provider::Button::dpad_right);
  ASSERT_EQ(focused_key(application), "after");
  press(providers, application, provider::Button::dpad_left);
  ASSERT_EQ(focused_key(application), "links");
  const auto node = node_named(application, "links");
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get().selected_link(), 1U);
}

TEST_F(LocalizationRuntimeTest, SlidersRunFromTheRightInRightToLeft) {
  static constexpr composition::StateKey<double> level{"rtl-slider", 0.5};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return rtl(composition::Element{
          controls::Slider(composition::Binding<double>{context.state(level)})}
                     .frame({.width = 200.0F})
                     .id("slider"));
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Host{});
  press(providers, application, provider::Button::dpad_down);
  ASSERT_EQ(focused_key(application), "slider");
  const auto before = spoken(application, "slider");
  press(providers, application, provider::Button::dpad_right);
  const auto element =
      components::AccessibilityTree{application.root().tree()}.element(
          *node_of(application, "slider"));
  ASSERT_TRUE(element.has_value());
  ASSERT_TRUE(element->value().has_value());
  EXPECT_EQ(*element->value(), "45%");
  static_cast<void>(before);
}

TEST_F(LocalizationRuntimeTest, AccessibilityMetadataReachesTheTree) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::Text("Settings")
              .accessibility_heading(composition::AccessibilityHeadingLevel::h1)
              .id("heading"),
          components::Button("Go", [] {})
              .accessibility_label("Start game")
              .accessibility_hint("Launches the selected title")
              .accessibility_identifier("start"),
          components::HStack(components::Image::system_name("language"),
                             components::Text("12"), components::Text("files"))
              .accessibility_element(
                  composition::AccessibilityChildBehavior::combine)
              .accessibility_identifier("count"),
          components::Text("secret").accessibility_hidden().id("hidden"),
          components::Text("plain")
              .accessibility_add_traits(
                  composition::AccessibilityTrait::is_summary_element)
              .id("plain")));
  const components::AccessibilityTree tree{application.root().tree()};
  const auto heading = tree.element(*node_of(application, "heading"));
  ASSERT_TRUE(heading.has_value());
  EXPECT_TRUE(
      heading->traits().contains(composition::AccessibilityTrait::is_header));
  EXPECT_EQ(heading->heading(), composition::AccessibilityHeadingLevel::h1);
  const auto start_button = tree.find("start");
  ASSERT_TRUE(start_button.has_value());
  EXPECT_EQ(start_button->label(), "Start game");
  EXPECT_EQ(start_button->hint(), "Launches the selected title");
  EXPECT_TRUE(start_button->traits().contains(
      composition::AccessibilityTrait::is_button));
  const auto counted = tree.find("count");
  ASSERT_TRUE(counted.has_value());
  EXPECT_EQ(counted->label(), "language, 12, files");
  EXPECT_FALSE(tree.element(*node_of(application, "hidden")).has_value());
  const auto plain = tree.element(*node_of(application, "plain"));
  ASSERT_TRUE(plain.has_value());
  EXPECT_TRUE(plain->traits().contains(
      composition::AccessibilityTrait::is_summary_element));
  EXPECT_TRUE(plain->traits().contains(
      composition::AccessibilityTrait::is_static_text));
  for (const components::AccessibilityElement& each : tree.elements()) {
    EXPECT_NE(each.label(), "secret");
  }
}

TEST_F(LocalizationRuntimeTest, BoldTextWidensTextLikeSemibold) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Application application =
      start(providers,
            components::VStack(
                components::Text("Bold Text Sample").id("regular"),
                components::Text("Bold Text Sample")
                    .environment(components::Environment::legibility_weight,
                                 composition::LegibilityWeight::bold)
                    .id("bold"),
                components::Text("Bold Text Sample")
                    .font_weight(theme::Font::Weight::semibold)
                    .id("semibold")));
  const auto width = [&application](std::string_view key) {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame().width : 0.0F;
  };
  EXPECT_GT(width("bold"), width("regular"));
  EXPECT_FLOAT_EQ(width("bold"), width("semibold"));
}

TEST_F(LocalizationRuntimeTest, HighContrastIsABundledThemeWithAStrongHighlight) {
  const theme::Theme contrasted = theme::Theme::high_contrast();
  EXPECT_EQ(contrasted.name(), theme::Theme::high_contrast_name);
  EXPECT_EQ(contrasted.focus_ring().style,
            theme::Theme::FocusRing::Style::highlight);
  EXPECT_EQ(contrasted.focus_ring().highlight,
            theme::Theme::high_contrast_highlight);
  EXPECT_GT(contrasted.focus_ring().highlight.fill,
            theme::Theme::defaults().focus_ring().highlight.fill);
  EXPECT_TRUE(contrasted.focus_ring().halo);
  EXPECT_FLOAT_EQ(contrasted.focus_ring().width,
                  theme::Theme::high_contrast_ring_width);
  const theme::Theme standard = theme::Theme::defaults();
  const auto role = theme::ColourRole::label;
  EXPECT_EQ(contrasted.palette(role).light,
            standard.variants(role).high_contrast_light);
  EXPECT_FALSE(standard.focus_ring().halo);
}

TEST_F(LocalizationRuntimeTest, TheFileImporterBrowsesAndPicksAFile) {
  static constexpr composition::StateKey<bool> open{"files-open", true};
  static constexpr composition::StateKey<std::string> picked{"files-picked",
                                                             std::string{}};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto chosen = context.state(picked);
      return components::Text(chosen.get())
          .id("picked")
          .file_importer(composition::Binding<bool>{context.state(open)},
                         {composition::UTType::plain_text()},
                         [chosen](structure::FileBrowser::Result result) {
                           chosen.set(result.has_value()
                                          ? std::string{result->path()}
                                          : result.error().explanation());
                         });
    }
  };
  provider::headless::MemoryStorage card;
  card.add(*provider::AssetPath::create("switch/notes/readme.txt"), "hello");
  card.add(*provider::AssetPath::create("switch/notes/photo.bin"), "x");
  card.add(*provider::AssetPath::create("atmosphere/config.ini"), "y");
  provider::headless::HeadlessProviderFactory providers;
  auto created =
      Application::create(providers, Host{},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 30);
  ASSERT_TRUE(
      node_of(application,
              structure::FileBrowser::row_key("atmosphere").description())
          .has_value());
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::row_key("switch")));
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::row_key("notes")));
  frames(application, 30);
  const auto binary = node_named(
      application, structure::FileBrowser::row_key("photo.bin").description());
  ASSERT_TRUE(binary.has_value());
  ASSERT_TRUE(application.root().activate(
      structure::FileBrowser::row_key("readme.txt")));
  frames(application, 40);
  EXPECT_EQ(spoken(application, "picked"), "switch/notes/readme.txt");
}

TEST_F(LocalizationRuntimeTest, TheFileExporterWritesIntoTheChosenFolder) {
  static constexpr composition::StateKey<bool> open{"export-open", true};
  static constexpr composition::StateKey<std::string> saved{"export-saved",
                                                            std::string{}};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto written = context.state(saved);
      return components::Text(written.get())
          .id("saved")
          .file_exporter(composition::Binding<bool>{context.state(open)},
                         std::vector<std::byte>{std::byte{'o'}, std::byte{'k'}},
                         composition::UTType::plain_text(), "out.txt",
                         [written](structure::FileBrowser::Result result) {
                           written.set(result.has_value()
                                           ? std::string{result->path()}
                                           : result.error().explanation());
                         });
    }
  };
  provider::headless::MemoryStorage card;
  card.add(*provider::AssetPath::create("exports/old.txt"), "old");
  provider::headless::HeadlessProviderFactory providers;
  auto created =
      Application::create(providers, Host{},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::row_key("exports")));
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::action_key()));
  frames(application, 40);
  EXPECT_EQ(spoken(application, "saved"), "exports/out.txt");
  EXPECT_EQ(card.text(*provider::AssetPath::create("exports/out.txt")), "ok");
}

TEST_F(LocalizationRuntimeTest, TheFileMoverMovesTheFileIntoTheChosenFolder) {
  static constexpr composition::StateKey<bool> open{"move-open", true};
  static constexpr composition::StateKey<std::string> moved{"move-moved",
                                                            std::string{}};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto landed = context.state(moved);
      return components::Text(landed.get())
          .id("moved")
          .file_mover(composition::Binding<bool>{context.state(open)},
                      composition::FileURL::create("inbox/notes.txt"),
                      [landed](structure::FileBrowser::Result result) {
                        landed.set(result.has_value()
                                       ? std::string{result->path()}
                                       : result.error().explanation());
                      });
    }
  };
  provider::headless::MemoryStorage card;
  card.add(*provider::AssetPath::create("inbox/notes.txt"), "notes");
  card.add(*provider::AssetPath::create("archive/old.txt"), "old");
  provider::headless::HeadlessProviderFactory providers;
  auto created =
      Application::create(providers, Host{},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::row_key("archive")));
  frames(application, 30);
  ASSERT_TRUE(
      application.root().activate(structure::FileBrowser::action_key()));
  frames(application, 40);
  EXPECT_EQ(spoken(application, "moved"), "archive/notes.txt");
  EXPECT_EQ(card.text(*provider::AssetPath::create("archive/notes.txt")),
            "notes");
  EXPECT_FALSE(card.text(*provider::AssetPath::create("inbox/notes.txt"))
                   .starts_with("notes"));
}

TEST_F(LocalizationRuntimeTest, CalendarsAndTimeZonesNameThemselves) {
  const auto made_german = text::Locale::create("de_DE");
  const auto made_american = text::Locale::create("en_US");
  ASSERT_TRUE(made_german.has_value() && made_american.has_value());
  const text::Locale& german = *made_german;
  const text::Locale& american = *made_american;
  EXPECT_EQ(components::Calendar::for_locale(german).first_weekday(),
            std::chrono::Monday);
  EXPECT_EQ(components::Calendar::for_locale(american).first_weekday(),
            std::chrono::Sunday);
  EXPECT_EQ(components::Calendar::for_locale(american).identifier(),
            components::Calendar::Identifier::gregorian);
  EXPECT_EQ(components::Calendar{components::Calendar::Identifier::iso8601}
                .first_weekday(),
            std::chrono::Monday);
  EXPECT_EQ(
      components::Calendar{components::Calendar::Identifier::japanese}.name(),
      "japanese");
  EXPECT_EQ(components::TimeZone::gmt().identifier(), "GMT");
  EXPECT_EQ(components::TimeZone::seconds_from_gmt(-std::chrono::seconds{19800})
                .identifier(),
            "GMT-05:30");
  const components::TimeZone berlin =
      components::TimeZone::named("Europe/Berlin", std::chrono::hours{1});
  EXPECT_EQ(berlin.identifier(), "Europe/Berlin");
  EXPECT_EQ(berlin.offset(), std::chrono::seconds{3600});
}

TEST_F(LocalizationRuntimeTest, CalendarAndTimeZoneReachViews) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<components::Calendar> calendar;
  std::optional<components::TimeZone> zone;
  class Host final : public composition::Component {
   public:
    Host(std::optional<components::Calendar>* calendar,
         std::optional<components::TimeZone>* zone) noexcept
        : calendar_(calendar), zone_(zone) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      *calendar_ = context.environment(components::Environment::calendar);
      *zone_ = context.environment(components::Environment::time_zone);
      return components::Text("dates");
    }

   private:
    std::optional<components::Calendar>* calendar_;
    std::optional<components::TimeZone>* zone_;
  };
  Application application = start(
      providers,
      composition::Element{Host{&calendar, &zone}}
          .environment(
              components::Environment::time_zone,
              components::TimeZone::named("Asia/Tokyo", std::chrono::hours{9}))
          .environment(components::Environment::calendar,
                       components::Calendar{
                           components::Calendar::Identifier::buddhist}));
  ASSERT_TRUE(calendar.has_value() && zone.has_value());
  EXPECT_EQ(calendar->identifier(), components::Calendar::Identifier::buddhist);
  EXPECT_EQ(zone->identifier(), "Asia/Tokyo");
  EXPECT_EQ(zone->offset(), std::chrono::seconds{32400});
}

TEST_F(LocalizationRuntimeTest, SystemAppletsReachTheEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  providers.applets().set_account(
      provider::UserAccount{.identifier = "0001", .nickname = "Rin"});
  std::optional<components::SystemApplets> seen;
  class Host final : public composition::Component {
   public:
    explicit Host(std::optional<components::SystemApplets>* into) noexcept
        : into_(into) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      *into_ = context.environment(components::Environment::system_applets);
      return components::Text("applets");
    }

   private:
    std::optional<components::SystemApplets>* into_;
  };
  Application application = start(providers, Host{&seen});
  ASSERT_TRUE(seen.has_value());
  const auto account = seen->select_account();
  ASSERT_TRUE(account.has_value());
  ASSERT_TRUE(account->has_value());
  EXPECT_EQ((*account)->nickname, "Rin");
  const auto players = seen->select_players(provider::PlayerSelection{
      .minimum = 2, .maximum = 4, .single_player = false});
  ASSERT_TRUE(players.has_value());
  EXPECT_EQ(*players, 2U);
  ASSERT_TRUE(seen->show_error(provider::ErrorReport{
                                   .code = 7, .message = "m", .details = {}})
                  .has_value());
  EXPECT_EQ(providers.applets().errors().size(), 1U);
}

}
