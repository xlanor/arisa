#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toast_options.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using provider::Button;

class PresentationTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<bool>> sheet;
    std::optional<composition::StateHandle<bool>> alert;
    std::optional<composition::StateHandle<bool>> nested;
    std::optional<composition::StateHandle<bool>> present;
    std::optional<composition::StateHandle<bool>> toast;
    std::vector<std::string> log;
    bool locked{false};
    int exits{0};
    int dismissals{0};
    int taps{0};
  };

  static constexpr composition::StateKey<bool> sheet_key{"sheet", false};
  static constexpr composition::StateKey<bool> alert_key{"alert", false};
  static constexpr composition::StateKey<bool> nested_key{"nested", false};
  static constexpr composition::StateKey<bool> present_key{"present", true};
  static constexpr composition::StateKey<bool> toast_key{"toast", false};

  static auto box(std::string key) -> Element {
    return components::Button(key, [] {})
        .frame({.width = 120.0F, .height = 40.0F})
        .id(std::move(key));
  }

  static auto actions(Probe* probe) -> Element {
    return components::Group(
        components::Button("Delete", components::ButtonRole::destructive,
                           [probe] { probe->log.emplace_back("delete"); })
            .frame({.width = 100.0F, .height = 40.0F})
            .id("delete"),
        components::Button("Keep", components::ButtonRole::cancel,
                           [probe] { probe->log.emplace_back("keep"); })
            .frame({.width = 100.0F, .height = 40.0F})
            .id("keep"));
  }

  class Sheets final : public composition::Component {
   public:
    explicit Sheets(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Sheets&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(sheet_key);
      const auto nested = context.state(nested_key);
      const auto present = context.state(present_key);
      probe_->sheet = shown;
      probe_->nested = nested;
      probe_->present = present;
      Probe* probe = probe_;
      std::vector<Element> rows;
      if (present.get()) {
        rows.push_back(box("open").sheet(
            shown, [probe] { ++probe->dismissals; },
            [probe, nested] {
              return components::VStack(box("inside-1"), box("inside-2"))
                  .interactive_dismiss_disabled(probe->locked)
                  .alert("Nested", nested, [probe] { return actions(probe); });
            }));
      }
      rows.push_back(box("other"));
      return components::VStack(std::move(rows))
          .on_exit_command("Outer", [probe] { ++probe->exits; });
    }

   private:
    Probe* probe_;
  };

  struct Seen {
    std::vector<bool> presented;
    std::vector<std::optional<theme::ColorScheme>> schemes;
  };

  class Reader final : public composition::Component {
   public:
    Reader(Seen* seen, std::string key) noexcept
        : seen_(seen), key_(std::move(key)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      seen_->presented.push_back(
          context.environment(components::Environment::is_presented));
      seen_->schemes.push_back(
          context.environment(components::Environment::colour_scheme));
      return box(key_);
    }

   private:
    Seen* seen_;
    std::string key_;
  };

  class Alerts final : public composition::Component {
   public:
    explicit Alerts(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Alerts&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(alert_key);
      probe_->alert = shown;
      Probe* probe = probe_;
      return components::VStack(
          box("ask").alert(
              "Delete?", shown, [probe] { return actions(probe); },
              [] { return components::Text("Gone for good"); }),
          box("other"));
    }

   private:
    Probe* probe_;
  };

  class Popovers final : public composition::Component {
   public:
    explicit Popovers(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Popovers&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(sheet_key);
      probe_->sheet = shown;
      return components::ZStack(
                 components::ZStackOptions{
                     .alignment = composition::Alignment::top_leading},
                 box("anchor")
                     .scale_effect(1.5F, motion::UnitPoint::top_leading)
                     .offset(300.0F, 120.0F)
                     .popover(shown,
                              composition::PopoverAttachmentAnchor::rect(),
                              theme::Edge::top,
                              [] {
                                return components::ZStack().frame(
                                    {.width = 200.0F, .height = 80.0F});
                              }))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity});
    }

   private:
    Probe* probe_;
  };

  class Toasts final : public composition::Component {
   public:
    explicit Toasts(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Toasts&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(toast_key);
      probe_->toast = shown;
      Probe* probe = probe_;
      return components::Button("under", [probe] { ++probe->taps; })
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .id("under")
          .toast(shown, 2s, [] {
            return components::ZStack().frame(
                {.width = 400.0F, .height = 60.0F});
          });
    }

   private:
    Probe* probe_;
  };

  class PlacedToast final : public composition::Component {
   public:
    explicit PlacedToast(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const PlacedToast&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(toast_key);
      probe_->toast = shown;
      return components::ZStack()
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .toast(shown, 2s,
                 composition::ToastOptions{
                     .alignment = composition::Alignment::top,
                     .tint = theme::ColourValue{theme::ColourRole::accent}},
                 [] {
                   return components::ZStack()
                       .frame({.width = 300.0F, .height = 40.0F})
                       .id("placed-toast");
                 });
    }

   private:
    Probe* probe_;
  };

  class Phase final : public composition::Component {
   public:
    auto operator==(const Phase&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const provider::ScenePhase phase =
          context.environment(components::Environment::scene_phase);
      return components::Text(phase == provider::ScenePhase::background
                                  ? "background"
                                  : "foreground");
    }
  };

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) {
    for (int index = 0; index < 40; ++index) {
      step(providers, application, 32ms);
    }
  }

  static auto hinted(Application& application, Button button,
                     std::string_view label) -> bool {
    return std::ranges::any_of(
        application.interaction().hints(),
        [button, label](const composition::CommandHint& hint) {
          return hint.button == button && hint.label == label;
        });
  }

  static auto layers(const composition::Node& node) -> std::size_t {
    std::size_t found =
        node.content_as<composition::Layer::Content>().has_value() ? 1U : 0U;
    for (const composition::Node& child : node.children()) {
      found += layers(child);
    }
    return found;
  }

  static auto bounds_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value() || !node->get().presented().has_value()) {
      ADD_FAILURE() << "node keyed " << key << " is not presented";
      return composition::Rect{};
    }
    return node->get().presented()->bounds;
  }
};

TEST_F(PresentationTest, IsPresentedAndPreferredSchemeBelongToThePresentation) {
  provider::headless::HeadlessProviderFactory providers;
  Seen outside;
  Seen inside;
  std::optional<composition::StateHandle<bool>> sheet;
  class Host final : public composition::Component {
   public:
    Host(Seen* outer, Seen* inner,
         std::optional<composition::StateHandle<bool>>* handle) noexcept
        : outer_(outer), inner_(inner), handle_(handle) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(sheet_key);
      *handle_ = shown;
      Seen* inner = inner_;
      return components::VStack(
          Element{Reader{outer_, "outside"}}.sheet(shown, [inner] {
            return Element{Reader{inner, "inside"}}.preferred_color_scheme(
                theme::ColorScheme::dark);
          }));
    }

   private:
    Seen* outer_;
    Seen* inner_;
    std::optional<composition::StateHandle<bool>>* handle_;
  };
  Application application = start(providers, Host{&outside, &inside, &sheet});
  ASSERT_FALSE(outside.presented.empty());
  EXPECT_FALSE(outside.presented.back());
  sheet->set(true);
  frames(application, 6);
  ASSERT_FALSE(inside.presented.empty());
  EXPECT_TRUE(inside.presented.back());
  EXPECT_EQ(inside.schemes.back(), std::optional{theme::ColorScheme::dark});
  EXPECT_NE(outside.schemes.back(), std::optional{theme::ColorScheme::dark});
}

TEST_F(PresentationTest, ASchemeDeclaredDeepInAStatelessSheetStaysInTheSheet) {
  provider::headless::HeadlessProviderFactory providers;
  Seen outside;
  Seen inside;
  std::optional<composition::StateHandle<bool>> sheet;
  class Host final : public composition::Component {
   public:
    Host(Seen* outer, Seen* inner,
         std::optional<composition::StateHandle<bool>>* handle) noexcept
        : outer_(outer), inner_(inner), handle_(handle) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(sheet_key);
      *handle_ = shown;
      Seen* inner = inner_;
      return components::VStack(
          Element{Reader{outer_, "outside"}}.sheet(shown, [inner] {
            return Element{components::VStack(
                Element{Reader{inner, "inside"}},
                components::VStack(
                    components::VStack(box("deep"))
                        .preferred_color_scheme(theme::ColorScheme::dark)))};
          }));
    }

   private:
    Seen* outer_;
    Seen* inner_;
    std::optional<composition::StateHandle<bool>>* handle_;
  };
  Application application = start(providers, Host{&outside, &inside, &sheet});
  sheet->set(true);
  frames(application, 6);
  ASSERT_FALSE(inside.schemes.empty());
  EXPECT_EQ(inside.schemes.back(), std::optional{theme::ColorScheme::dark});
  EXPECT_NE(outside.schemes.back(), std::optional{theme::ColorScheme::dark});
}

TEST_F(PresentationTest, APreferredSchemeAtTheRootReachesTheWholeWindow) {
  provider::headless::HeadlessProviderFactory providers;
  Seen seen;
  class Window final : public composition::Component {
   public:
    explicit Window(Seen* seen) noexcept : seen_(seen) {}

    auto operator==(const Window&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return components::VStack(
          Element{Reader{seen_, "first"}},
          components::VStack(box("deep"))
              .preferred_color_scheme(theme::ColorScheme::dark));
    }

   private:
    Seen* seen_;
  };
  Application application = start(providers, Window{&seen});
  frames(application, 4);
  ASSERT_FALSE(seen.schemes.empty());
  EXPECT_EQ(seen.schemes.back(), std::optional{theme::ColorScheme::dark});
}

TEST_F(PresentationTest, SheetConfinesFocusAndRestoresItOnDismiss) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Sheets{&probe});
  EXPECT_EQ(focused_key(application), "open");
  probe.sheet->set(true);
  frames(application, 2);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "inside-1");
  EXPECT_TRUE(hinted(application, Button::b, "Close"));
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "inside-2");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "inside-2");
  press(providers, application, Button::b);
  EXPECT_FALSE(probe.sheet->get());
  EXPECT_EQ(probe.exits, 0);
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "open");
  EXPECT_EQ(probe.dismissals, 1);
  settle(providers, application);
  EXPECT_EQ(layers(application.root().tree().root()), 0U);
  EXPECT_EQ(application.root().tree().departing(), 0U);
}

TEST_F(PresentationTest, InteractiveDismissDisabledSwallowsBack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  probe.locked = true;
  Application application = start(providers, Sheets{&probe});
  probe.sheet->set(true);
  frames(application, 2);
  settle(providers, application);
  EXPECT_FALSE(hinted(application, Button::b, "Close"));
  EXPECT_FALSE(hinted(application, Button::b, "Outer"));
  press(providers, application, Button::b);
  EXPECT_TRUE(probe.sheet->get());
  EXPECT_EQ(probe.exits, 0);
  EXPECT_EQ(focused_key(application), "inside-1");
}

TEST_F(PresentationTest, NestedPresentationsStackAndDismissInOrder) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Sheets{&probe});
  probe.sheet->set(true);
  frames(application, 2);
  settle(providers, application);
  probe.nested->set(true);
  frames(application, 2);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "keep");
  EXPECT_TRUE(hinted(application, Button::b, "Keep"));
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_up);
  const std::string inside = focused_key(application);
  EXPECT_TRUE(inside == "keep" || inside == "delete");
  press(providers, application, Button::b);
  EXPECT_FALSE(probe.nested->get());
  EXPECT_TRUE(probe.sheet->get());
  EXPECT_EQ(probe.log, std::vector<std::string>{"keep"});
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "inside-1");
  press(providers, application, Button::b);
  EXPECT_FALSE(probe.sheet->get());
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "open");
}

TEST_F(PresentationTest, AlertButtonsRunTheirRoleAndDismiss) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Alerts{&probe});
  probe.alert->set(true);
  frames(application, 2);
  settle(providers, application);
  EXPECT_TRUE(hinted(application, Button::b, "Keep"));
  EXPECT_EQ(focused_key(application), "keep");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "delete");
  press(providers, application, Button::a);
  EXPECT_EQ(probe.log, std::vector<std::string>{"delete"});
  EXPECT_FALSE(probe.alert->get());
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "ask");
  probe.alert->set(true);
  frames(application, 2);
  settle(providers, application);
  press(providers, application, Button::b);
  EXPECT_EQ(probe.log, (std::vector<std::string>{"delete", "keep"}));
  EXPECT_FALSE(probe.alert->get());
}

TEST_F(PresentationTest, ScrimBlocksTouchesBelowAModal) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Alerts{&probe});
  const provider::PixelPoint other = centre(application, "other");
  probe.alert->set(true);
  frames(application, 2);
  settle(providers, application);
  tap(providers, application, other);
  EXPECT_TRUE(probe.alert->get());
  EXPECT_TRUE(probe.log.empty());
  tap(providers, application, centre(application, "keep"));
  EXPECT_EQ(probe.log, std::vector<std::string>{"keep"});
  EXPECT_FALSE(probe.alert->get());
}

TEST_F(PresentationTest, PresenterDepartingDismissesCleanly) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Sheets{&probe});
  probe.sheet->set(true);
  frames(application, 2);
  settle(providers, application);
  EXPECT_EQ(focused_key(application), "inside-1");
  probe.present->set(false);
  frames(application, 2);
  EXPECT_FALSE(probe.sheet->get());
  EXPECT_EQ(probe.dismissals, 1);
  EXPECT_EQ(focused_key(application), "other");
  settle(providers, application);
  EXPECT_EQ(layers(application.root().tree().root()), 0U);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  press(providers, application, Button::b);
  EXPECT_EQ(probe.exits, 1);
}

TEST_F(PresentationTest, PopoverFollowsTheAnchorsPresentedFrame) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Popovers{&probe});
  const composition::Rect anchor = bounds_of(application, "anchor");
  EXPECT_NEAR(anchor.width, 180.0F, 0.5F);
  probe.sheet->set(true);
  frames(application, 2);
  settle(providers, application);
  std::optional<composition::Rect> panel;
  const auto search = [&](const auto& self,
                          const composition::Node& node) -> void {
    if (const auto layer = node.content_as<composition::Layer::Content>();
        layer.has_value() && node.child_count() == 2) {
      panel = (*std::next(node.children().begin())).presented()->bounds;
    }
    for (const composition::Node& child : node.children()) {
      self(self, child);
    }
  };
  search(search, application.root().tree().root());
  ASSERT_TRUE(panel.has_value());
  EXPECT_GT(panel->y, anchor.y + anchor.height);
  EXPECT_NEAR(panel->x + (panel->width * 0.5F),
              anchor.x + (anchor.width * 0.5F), 1.0F);
  press(providers, application, Button::b);
  EXPECT_FALSE(probe.sheet->get());
}

TEST_F(PresentationTest, ToastPassesInputThroughAndExpires) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Toasts{&probe});
  probe.toast->set(true);
  frames(application, 2);
  step(providers, application, 400ms);
  EXPECT_EQ(layers(application.root().tree().root()), 1U);
  EXPECT_EQ(focused_key(application), "under");
  tap(providers, application, provider::PixelPoint{.x = 640.0F, .y = 600.0F});
  EXPECT_EQ(probe.taps, 1);
  EXPECT_TRUE(probe.toast->get());
  step(providers, application, 1000ms);
  EXPECT_TRUE(probe.toast->get());
  step(providers, application, 700ms);
  frames(application, 2);
  EXPECT_FALSE(probe.toast->get());
  settle(providers, application);
  EXPECT_EQ(layers(application.root().tree().root()), 0U);
}

TEST_F(PresentationTest, ToastOptionsPlaceTheToastAtTheTop) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, PlacedToast{&probe});
  probe.toast->set(true);
  frames(application, 2);
  step(providers, application, 400ms);
  const provider::PixelPoint middle = centre(application, "placed-toast");
  EXPECT_LT(middle.y, 200.0F);
  EXPECT_GT(middle.y, 60.0F);
}

TEST_F(PresentationTest, ScenePhaseFollowsTheLifecycleProvider) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Phase{});
  EXPECT_TRUE(shows(application, "foreground"));
  providers.lifecycle().set_phase(provider::ScenePhase::background);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "background"));
}

TEST_F(PresentationTest, OverlayAndBackgroundTakeTheBaseFrame) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::ZStack()
              .frame({.width = 200.0F, .height = 100.0F})
              .id("base")
              .overlay(composition::Alignment::top_trailing,
                       components::ZStack()
                           .frame({.width = 20.0F, .height = 20.0F})
                           .id("badge"))
              .background(components::ZStack()
                              .frame({.max_width = composition::infinity,
                                      .max_height = composition::infinity})
                              .id("fill"))));
  const auto frame_of_key = [&application](std::string_view key) {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  };
  const composition::Rect base = frame_of_key("base");
  const composition::Rect badge = frame_of_key("badge");
  const composition::Rect fill = frame_of_key("fill");
  EXPECT_FLOAT_EQ(fill.x, base.x);
  EXPECT_FLOAT_EQ(fill.y, base.y);
  EXPECT_FLOAT_EQ(fill.width, base.width);
  EXPECT_FLOAT_EQ(fill.height, base.height);
  EXPECT_FLOAT_EQ(badge.x + badge.width, base.x + base.width);
  EXPECT_FLOAT_EQ(badge.y, base.y);
}

TEST_F(PresentationTest, HudLayerEscapesClippingAndIgnoresTouches) {
  provider::headless::HeadlessProviderFactory providers;
  int taps = 0;
  Application application =
      start(providers,
            components::VStack(
                components::Button("under", [&taps] { ++taps; })
                    .frame({.width = 300.0F, .height = 300.0F})
                    .id("under")
                    .overlay(true, composition::OverlayLayer::hud,
                             components::ZStack()
                                 .frame({.width = 600.0F, .height = 600.0F})
                                 .id("hud")))
                .frame({.width = 300.0F, .height = 300.0F})
                .clip_shape(theme::Rectangle()));
  const composition::Rect hud = bounds_of(application, "hud");
  EXPECT_NEAR(hud.width, 600.0F, 0.5F);
  EXPECT_NEAR(hud.x + (hud.width * 0.5F), 640.0F, 0.5F);
  tap(providers, application, centre(application, "under"));
  EXPECT_EQ(taps, 1);
}

}
