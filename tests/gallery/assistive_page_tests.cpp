#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/components/accessibility_tree.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility_action_kind.hpp>
#include <arisa/composition/accessibility_technologies.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/screen_reader.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <assistive_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class ScreenReaderTest : public GalleryFixture {
 protected:
  using Element = composition::Element;

  struct Seen {
    bool enabled{false};
    bool voice_over{false};
    bool switch_control{false};
    bool invert{false};
    bool animated{true};
    bool cross_fade{false};
    bool highlighting{false};
    bool borders{false};
    bool assistive{false};
    bool large{false};
    composition::AccessibilityTechnologies technologies{};
  };

  class Reader final : public composition::Component {
   public:
    explicit Reader(std::shared_ptr<Seen> seen) : seen_(std::move(seen)) {}

    auto operator==(const Reader&) const -> bool { return true; }

    auto body(composition::Context& context) const -> Element override {
      using components::Environment;
      seen_->enabled = context.environment(Environment::accessibility_enabled);
      seen_->voice_over =
          context.environment(Environment::accessibility_voice_over_enabled);
      seen_->switch_control = context.environment(
          Environment::accessibility_switch_control_enabled);
      seen_->invert =
          context.environment(Environment::accessibility_invert_colors);
      seen_->animated =
          context.environment(Environment::accessibility_play_animated_images);
      seen_->cross_fade = context.environment(
          Environment::accessibility_prefers_cross_fade_transitions);
      seen_->highlighting = context.environment(
          Environment::accessibility_reduce_highlighting_effects);
      seen_->borders =
          context.environment(Environment::accessibility_show_borders);
      seen_->assistive = context.environment(
          Environment::accessibility_assistive_access_enabled);
      seen_->large = context.environment(
          Environment::accessibility_large_content_viewer_enabled);
      seen_->technologies =
          context.environment(Environment::accessibility_technologies);
      return components::Text("Reader");
    }

   private:
    std::shared_ptr<Seen> seen_;
  };

  static auto content(std::shared_ptr<int> deleted) -> Element {
    return components::VStack(
        components::Text("Mail").accessibility_heading(
            composition::AccessibilityHeadingLevel::h1),
        Element{components::Button("First", [] {})}.id("first"),
        Element{components::Button("Report", [] {})}
            .accessibility_action(composition::AccessibilityActionKind::delete_,
                                  [deleted] { ++*deleted; })
            .accessibility_action(composition::AccessibilityActionKind::escape,
                                  [deleted] { *deleted += 100; })
            .accessibility_hint("Opens the report")
            .speech_adjusted_pitch(1.25)
            .accessibility_shows_large_content_viewer()
            .id("report"));
  }

  auto open(provider::headless::HeadlessProviderFactory& providers,
            Element shown, appearance::AppearanceSettings defaults)
      -> Application {
    load_fonts(providers);
    auto created = Application::create(
        providers, std::move(shown),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = {},
            .atlas = {},
            .interaction = {},
            .appearance = AppearanceOptions{
                .policy = appearance::AppearancePolicy{.defaults = defaults},
                .storage = std::ref(storage_)}});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    run(providers, application, 4);
    return application;
  }

  template <class Change>
  static auto settings(Change change) -> appearance::AppearanceSettings {
    appearance::AppearanceSettings made;
    change(made);
    return made;
  }

  static void tap(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, provider::Button button) {
    providers.input().push(pad({button}));
    run(providers, application, 1);
    providers.input().push(pad({}));
    run(providers, application, 3);
  }

  static auto last(const Application& application) -> std::string {
    const auto spoken = application.screen_reader().spoken();
    return spoken.empty() ? std::string{} : spoken.back().text;
  }

  provider::headless::MemoryStorage storage_;
};

TEST_F(ScreenReaderTest, AssistiveSettingsReachTheEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<Seen>();
  Application application =
      open(providers, Reader{seen},
           settings([](appearance::AppearanceSettings& held) {
             held.dynamic_type_size = theme::DynamicTypeSize::accessibility1;
             held.voice_over = true;
             held.switch_control = true;
             held.invert_colors = true;
             held.play_animated_images = false;
             held.prefers_cross_fade_transitions = true;
             held.reduce_highlighting_effects = true;
             held.show_borders = true;
             held.assistive_access = true;
           }));
  EXPECT_TRUE(seen->enabled);
  EXPECT_TRUE(seen->voice_over);
  EXPECT_TRUE(seen->switch_control);
  EXPECT_TRUE(seen->invert);
  EXPECT_FALSE(seen->animated);
  EXPECT_TRUE(seen->cross_fade);
  EXPECT_TRUE(seen->highlighting);
  EXPECT_TRUE(seen->borders);
  EXPECT_TRUE(seen->assistive);
  EXPECT_TRUE(seen->large);
  EXPECT_EQ(seen->technologies,
            composition::AccessibilityTechnologies::voice_over |
                composition::AccessibilityTechnologies::switch_control);
  provider::headless::HeadlessProviderFactory plain_providers;
  auto plain = std::make_shared<Seen>();
  Application quiet = open(plain_providers, Reader{plain}, {});
  EXPECT_FALSE(plain->enabled);
  EXPECT_FALSE(plain->voice_over);
  EXPECT_TRUE(plain->animated);
  EXPECT_FALSE(plain->large);
}

TEST_F(ScreenReaderTest, TheReaderSpeaksFocusAndDrivesTheRotor) {
  provider::headless::HeadlessProviderFactory providers;
  auto deleted = std::make_shared<int>(0);
  Application application =
      open(providers, content(deleted),
           settings([](appearance::AppearanceSettings& held) {
             held.voice_over = true;
           }));
  const ScreenReader& reader = application.screen_reader();
  ASSERT_TRUE(reader.voice_over());
  run(providers, application, 2);
  EXPECT_EQ(application.focused(), node_of(application, "first"));
  EXPECT_EQ(last(application), "First, button");
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 2);
  EXPECT_EQ(application.focused(), node_of(application, "report"));
  EXPECT_EQ(last(application), "Report, button. Opens the report");
  EXPECT_DOUBLE_EQ(reader.spoken().back().pitch, 1.25);
  ASSERT_FALSE(providers.speech().spoken().empty());
  EXPECT_EQ(providers.speech().spoken().back().text,
            "Report, button. Opens the report");
  EXPECT_EQ(reader.rotor(), ScreenReader::actions_rotor);
  tap(providers, application, ScreenReader::next_item);
  EXPECT_EQ(reader.selection(), "Delete");
  EXPECT_EQ(last(application), "Delete");
  tap(providers, application, provider::Button::a);
  EXPECT_EQ(*deleted, 1);
  tap(providers, application, provider::Button::b);
  EXPECT_EQ(*deleted, 101);
  tap(providers, application, ScreenReader::next_rotor);
  EXPECT_EQ(reader.rotor(), "Headings");
  tap(providers, application, ScreenReader::next_item);
  EXPECT_EQ(last(application), "Mail, header, static text");
  EXPECT_TRUE(shows(application, "Mail, header, static text"));
}

TEST_F(ScreenReaderTest, LargeContentFollowsFocusAtAccessibilitySizes) {
  provider::headless::HeadlessProviderFactory providers;
  auto deleted = std::make_shared<int>(0);
  Application application =
      open(providers, content(deleted),
           settings([](appearance::AppearanceSettings& held) {
             held.dynamic_type_size = theme::DynamicTypeSize::accessibility2;
           }));
  for (int step = 0;
       step < 4 && application.focused() != node_of(application, "report");
       ++step) {
    press(providers, application, provider::Button::dpad_down);
  }
  run(providers, application, 2);
  ASSERT_EQ(application.focused(), node_of(application, "report"));
  EXPECT_TRUE(application.screen_reader().showing_large_content());
  press(providers, application, provider::Button::dpad_up);
  run(providers, application, 2);
  EXPECT_EQ(application.focused(), node_of(application, "first"));
  EXPECT_FALSE(application.screen_reader().showing_large_content());
  EXPECT_TRUE(application.screen_reader().spoken().empty());
}

TEST_F(ScreenReaderTest, AnOffReaderCostsNothingPerFrame) {
  provider::headless::HeadlessProviderFactory providers;
  auto deleted = std::make_shared<int>(0);
  Application application = open(providers, content(deleted), {});
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 10);
  std::uint64_t used = 0;
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
  EXPECT_EQ(application.screen_reader().narrations(), 0U);
  EXPECT_TRUE(application.screen_reader().spoken().empty());
  EXPECT_TRUE(providers.speech().spoken().empty());
}

TEST_F(ScreenReaderTest, ARunningReaderSettlesWithoutAllocating) {
  provider::headless::HeadlessProviderFactory providers;
  auto deleted = std::make_shared<int>(0);
  Application application =
      open(providers, content(deleted),
           settings([](appearance::AppearanceSettings& held) {
             held.voice_over = true;
           }));
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 30);
  const std::uint64_t narrated = application.screen_reader().narrations();
  std::uint64_t used = 0;
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
  EXPECT_EQ(application.screen_reader().narrations(), narrated);
}

class GalleryAssistiveTest : public GalleryFixture {
 protected:
  using Page = examples::AssistivePage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"assistive-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };
};

TEST_F(GalleryAssistiveTest, ThePageShowsEveryDemoAndExposesItsActions) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  providers.surface().resize({.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
  Application application = start(providers, Host{});
  run(providers, application, 30);
  for (const char* title : {"Custom actions", "Adjustable", "Rotors",
                            "Accessibility focus", "Appearance", "Inbox"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  const components::AccessibilityTree tree{application.root().tree()};
  const auto mail = tree.element(*node_of(application, Page::Demos::mail_row));
  ASSERT_TRUE(mail.has_value());
  std::vector<std::string> names;
  for (const auto& action : mail->actions()) {
    names.emplace_back(action.name());
  }
  EXPECT_EQ(names, (std::vector<std::string>{"Flag", "Delete", "Archive"}));
  const auto rating =
      tree.element(*node_of(application, Page::Demos::rating_row));
  ASSERT_TRUE(rating.has_value());
  EXPECT_EQ(rating->value(), std::optional<std::string>{"3 of 5"});
  EXPECT_TRUE(
      rating->adjust(application.root().tree(),
                     composition::AccessibilityAdjustmentDirection::increment));
  run(providers, application, 3);
  EXPECT_EQ(components::AccessibilityTree{application.root().tree()}
                .element(*node_of(application, Page::Demos::rating_row))
                ->value(),
            std::optional<std::string>{"4 of 5"});
  const auto volume =
      tree.element(*node_of(application, Page::Demos::volume_row));
  ASSERT_TRUE(volume.has_value());
  EXPECT_EQ(volume->label(), "Volume");
  std::uint64_t used = 0;
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

}
