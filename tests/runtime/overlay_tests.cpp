#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/seed.hpp>
#include <arisa/theme/theme.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using provider::Button;

class OverlayTest : public ScrollFixture {
 protected:
  struct Seen {
    std::optional<theme::ColorScheme> scheme;
    std::optional<theme::Colour> accent;
    std::optional<int> depth;
  };

  static constexpr composition::EnvironmentKey<int> depth{"overlay-test-depth",
                                                          1};
  static constexpr std::uint64_t system_overlay = 1'000'000;

  class Probe final : public composition::Component {
   public:
    explicit Probe(std::shared_ptr<Seen> seen) noexcept
        : seen_(std::move(seen)) {}

    auto operator==(const Probe&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      seen_->scheme =
          context.environment(components::Environment::colour_scheme);
      seen_->accent =
          context.environment(components::Environment::theme)
              .colour(theme::ColourRole::accent, theme::ColorScheme::dark);
      seen_->depth = context.environment(depth);
      return toast("probe");
    }

   private:
    std::shared_ptr<Seen> seen_;
  };

  static auto branded() -> theme::Theme {
    return theme::Theme::from(
        theme::Seed{.accent = theme::Colour::hex(0xFF2D55),
                    .scheme = theme::ColorScheme::dark});
  }

  static auto layered(std::shared_ptr<Seen> seen) -> Element {
    std::vector<Element> children;
    children.emplace_back(Probe{std::move(seen)});
    return composition::Layer(composition::OverlayLayer::dialog,
                              composition::OverlayInput::passthrough,
                              composition::Alignment::bottom,
                              std::move(children));
  }

  static auto toast(std::string key) -> Element {
    return components::ZStack()
        .frame({.width = 300.0F, .height = 50.0F})
        .id(std::move(key));
  }

  static auto shown(Application& application, std::string_view key) -> bool {
    const auto found = node_of(application, key);
    return found.has_value() &&
           application.root().tree().node(*found)->get().phase() !=
               composition::NodeLifecyclePhase::departing;
  }

  static void settle_frames(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) {
    for (int index = 0; index < 30; ++index) {
      step(providers, application, 32ms);
    }
  }

  static auto tip_shown(const composition::Node& node) -> bool {
    if (const auto layer = node.content_as<composition::Layer::Content>();
        layer.has_value() &&
        layer->get().layer() == composition::OverlayLayer::toast &&
        node.phase() != composition::NodeLifecyclePhase::departing) {
      return true;
    }
    for (const composition::Node& child : node.children()) {
      if (tip_shown(child)) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(OverlayTest, HandleKeepsTheOverlayUntilReleased) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  std::optional<OverlayHandle> handle = application.root().overlays().show(
      composition::OverlayLayer::toast, toast("toast"));
  frames(application, 2);
  EXPECT_TRUE(handle->active());
  EXPECT_TRUE(shown(application, "toast"));
  EXPECT_EQ(application.root().tree().overlays(), 1U);
  EXPECT_EQ(focused_key(application), "under");
  handle.reset();
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_FALSE(shown(application, "toast"));
  EXPECT_EQ(application.root().tree().overlays(), 0U);
}

TEST_F(OverlayTest, OverlaysStackAndLeaveIndependently) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  Overlays& overlays = application.root().overlays();
  OverlayHandle first =
      overlays.show(composition::OverlayLayer::toast, toast("first"));
  {
    OverlayHandle second = overlays.show(
        composition::OverlayLayer::hud, toast("second"),
        OverlayBehaviour{.alignment = composition::Alignment::top});
    frames(application, 2);
    EXPECT_TRUE(shown(application, "first"));
    EXPECT_TRUE(shown(application, "second"));
  }
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_TRUE(shown(application, "first"));
  EXPECT_FALSE(shown(application, "second"));
}

TEST_F(OverlayTest, ADurationRemovesTheOverlayWhileTheHandleLives) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::toast, toast("brief"),
      OverlayBehaviour{.duration = 200ms});
  frames(application, 2);
  EXPECT_TRUE(shown(application, "brief"));
  settle_frames(providers, application);
  EXPECT_FALSE(shown(application, "brief"));
  handle.release();
  frames(application, 2);
  EXPECT_EQ(application.root().tree().overlays(), 0U);
}

TEST_F(OverlayTest, AModalOverlayConfinesFocusUntilReleased) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, components::VStack(button("first"), button("second")));
  EXPECT_EQ(focused_key(application), "first");
  std::optional<OverlayHandle> handle = application.root().overlays().show(
      composition::OverlayLayer::dialog, button("blocking"),
      OverlayBehaviour{.input = composition::OverlayInput::modal,
                       .alignment = composition::Alignment::center});
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_EQ(focused_key(application), "blocking");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "blocking");
  handle.reset();
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_EQ(focused_key(application), "first");
}

TEST_F(OverlayTest, BackDismissesAnOverlayThatAsksForIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::dialog, button("menu"),
      OverlayBehaviour{.input = composition::OverlayInput::modal,
                       .alignment = composition::Alignment::center,
                       .dismiss_on_back = true});
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_EQ(focused_key(application), "menu");
  press(providers, application, Button::b);
  settle_frames(providers, application);
  EXPECT_FALSE(shown(application, "menu"));
  EXPECT_EQ(focused_key(application), "under");
  handle.release();
  frames(application, 2);
  EXPECT_EQ(application.root().tree().overlays(), 0U);
}

TEST_F(OverlayTest, OverlaysShownAndReleasedOnAnotherThread) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  auto handle = std::make_shared<std::optional<OverlayHandle>>();
  Root& root = application.root();
  {
    auto worker = providers.threading().start(
        provider::ThreadSpec{.name = "overlay-owner",
                             .role = provider::ThreadRole::background},
        [handle, &root](const std::stop_token&) {
          handle->emplace(root.overlays().show(composition::OverlayLayer::toast,
                                               toast("remote")));
        });
    ASSERT_TRUE(worker.has_value());
  }
  frames(application, 2);
  EXPECT_TRUE(shown(application, "remote"));
  {
    auto worker = providers.threading().start(
        provider::ThreadSpec{.name = "overlay-owner",
                             .role = provider::ThreadRole::background},
        [handle](const std::stop_token&) { handle->reset(); });
    ASSERT_TRUE(worker.has_value());
  }
  frames(application, 2);
  settle_frames(providers, application);
  EXPECT_FALSE(shown(application, "remote"));
}

TEST_F(OverlayTest, HelpShowsAfterTheFocusDelayAndHidesOnMove) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(button("helped").help("Explains"), button("plain")));
  EXPECT_NE(focused_key(application), "plain");
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
  for (int index = 0; index < 20; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
  for (int index = 0; index < 30; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_TRUE(tip_shown(application.root().tree().root()));
  EXPECT_TRUE(shows(application, "Explains"));
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "plain");
  settle_frames(providers, application);
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
}

TEST_F(OverlayTest, HelpShowsOnLongPressWithoutActivating) {
  provider::headless::HeadlessProviderFactory providers;
  int taps = 0;
  Application application =
      start(providers,
            components::VStack(button("plain"),
                               components::Button("helped", [&taps] { ++taps; })
                                   .frame({.width = 100.0F, .height = 50.0F})
                                   .id("helped")
                                   .help("Explains")));
  const provider::PixelPoint point = centre(application, "helped");
  touch_down(providers, application, point);
  for (int index = 0; index < 30; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_TRUE(tip_shown(application.root().tree().root()));
  touch_up(providers, application, point);
  EXPECT_EQ(taps, 0);
  for (int index = 0; index < 90; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
}

TEST_F(OverlayTest, ImperativeOverlaysInheritTheContentAppearance) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            button("under").theme_override(branded()).environment(
                components::Environment::colour_scheme,
                std::optional<theme::ColorScheme>{theme::ColorScheme::dark}));
  const auto seen = std::make_shared<Seen>();
  const OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::dialog, Probe{seen});
  frames(application, 2);
  EXPECT_TRUE(handle.active());
  EXPECT_TRUE(shown(application, "probe"));
  EXPECT_EQ(seen->scheme, theme::ColorScheme::dark);
  EXPECT_EQ(seen->accent, branded().colour(theme::ColourRole::accent,
                                           theme::ColorScheme::dark));
}

TEST_F(OverlayTest, ImperativeOverlaysKeepTheirOwnEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            button("under").environment(
                components::Environment::colour_scheme,
                std::optional<theme::ColorScheme>{theme::ColorScheme::dark}));
  const auto seen = std::make_shared<Seen>();
  const OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::dialog,
      Element{Probe{seen}}.environment(
          components::Environment::colour_scheme,
          std::optional<theme::ColorScheme>{theme::ColorScheme::light}));
  frames(application, 2);
  EXPECT_TRUE(handle.active());
  EXPECT_EQ(seen->scheme, theme::ColorScheme::light);
  EXPECT_EQ(seen->accent,
            theme::Theme::defaults().colour(theme::ColourRole::accent,
                                            theme::ColorScheme::dark));
}

TEST_F(OverlayTest, ImperativeOverlaysWithoutContentAppearanceUseTheDefaults) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, button("under"));
  const auto seen = std::make_shared<Seen>();
  const OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::toast, Probe{seen});
  frames(application, 2);
  EXPECT_TRUE(handle.active());
  EXPECT_NE(seen->scheme, theme::ColorScheme::dark);
  EXPECT_EQ(seen->accent,
            theme::Theme::defaults().colour(theme::ColourRole::accent,
                                            theme::ColorScheme::dark));
}

TEST_F(OverlayTest, SystemOverlaysDoNotInheritTheContentEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            button("under").environment(depth, 10).environment(
                components::Environment::colour_scheme,
                std::optional<theme::ColorScheme>{theme::ColorScheme::dark}));
  const auto system = std::make_shared<Seen>();
  application.root().tree().present_overlay(system_overlay, layered(system));
  const auto app = std::make_shared<Seen>();
  const OverlayHandle handle = application.root().overlays().show(
      composition::OverlayLayer::dialog, Probe{app});
  frames(application, 2);
  EXPECT_TRUE(handle.active());
  EXPECT_EQ(system->depth, 1);
  EXPECT_NE(system->scheme, theme::ColorScheme::dark);
  EXPECT_EQ(app->depth, 10);
  EXPECT_EQ(app->scheme, theme::ColorScheme::dark);
}

TEST_F(OverlayTest, InheritedOverlayTransformsApplyOverTheContentOverride) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, button("under").environment(depth, 10));
  const auto seen = std::make_shared<Seen>();
  application.root().tree().present_overlay(
      system_overlay,
      layered(seen).transform_environment(depth, [](int& value) { ++value; }),
      composition::Tree::OverlayEnvironment::inherited);
  frames(application, 2);
  EXPECT_EQ(seen->depth, 11);
}

TEST_F(OverlayTest, InheritedOverlayTransformsComposeWithTheContentTransforms) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, button("under").transform_environment(
                           depth, [](int& value) { value *= 3; }));
  const auto seen = std::make_shared<Seen>();
  application.root().tree().present_overlay(
      system_overlay,
      layered(seen).transform_environment(depth, [](int& value) { ++value; }),
      composition::Tree::OverlayEnvironment::inherited);
  frames(application, 2);
  EXPECT_EQ(seen->depth, 4);
}

TEST_F(OverlayTest, HelpWithATitleShowsTitleAndBody) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(button("helped").help("Title", "Body text"),
                                    button("plain")));
  for (int index = 0; index < 20; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
  for (int index = 0; index < 30; ++index) {
    step(providers, application, 32ms);
  }
  EXPECT_TRUE(tip_shown(application.root().tree().root()));
  EXPECT_TRUE(shows(application, "Title"));
  EXPECT_TRUE(shows(application, "Body text"));
  press(providers, application, Button::dpad_down);
  settle_frames(providers, application);
  EXPECT_FALSE(tip_shown(application.root().tree().root()));
}

}
