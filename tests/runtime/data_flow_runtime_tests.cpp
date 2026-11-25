#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/link.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/scene_storage.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/structure/scaffold.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class DataFlowRuntimeTest : public ScrollFixture {
 protected:
  using Selection = std::optional<composition::Anchor<composition::Rect>>;

  static const composition::PreferenceKey<Selection> selection;

  class Titled final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"titled-count", 0};

    auto operator==(const Titled&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto taps = context.state(count);
      return components::VStack(
                 components::Text("Body"),
                 presentation::NavigationLink(5, "Open").id("open"))
          .navigation_title("Nested title")
          .toolbar(composition::ToolbarItem(
              components::Button(std::format("Count {}", taps.get()),
                                 [taps] { taps.set(taps.get() + 1); })
                  .id("count")))
          .navigation_destination(
              composition::for_type<int>, [](const int& value) {
                return components::Text(std::format("Detail {}", value));
              });
    }
  };

  class TileLabel final : public composition::Component {
   public:
    explicit TileLabel(int index) noexcept : index_(index) {}

    auto operator==(const TileLabel&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const bool focused =
          context.environment(components::Environment::is_focused);
      Element label =
          components::Text(focused ? std::format("Focused {}", index_)
                                   : std::format("Tile {}", index_))
              .frame({.width = 80.0F, .height = 40.0F})
              .id(std::format("label-{}", index_));
      if (focused) {
        label = std::move(label).anchor_preference(
            selection, composition::AnchorSource<composition::Rect>::bounds(),
            [](const composition::Anchor<composition::Rect>& anchor) {
              return Selection{anchor};
            });
      }
      return label;
    }

   private:
    int index_;
  };

  static auto tiles() -> Element {
    std::vector<Element> row;
    for (int index = 0; index < 3; ++index) {
      row.push_back(components::Button([] {}, TileLabel{index})
                        .id(std::format("tile-{}", index)));
    }
    return components::HStack(components::HStackOptions{.spacing = 20.0F},
                              std::move(row))
        .padding(theme::space::lg)
        .overlay_preference_value(selection, [](const Selection& chosen) {
          return components::GeometryReader(
              [chosen](const composition::GeometryProxy& proxy) -> Element {
                if (!chosen.has_value()) {
                  return components::ZStack();
                }
                const composition::Rect ring = proxy[*chosen];
                return components::ZStack()
                    .frame({.width = ring.width, .height = ring.height})
                    .border(theme::ColourRole::accent)
                    .id("ring")
                    .position(ring.x + (ring.width * 0.5F),
                              ring.y + (ring.height * 0.5F))
                    .allows_hit_testing(false);
              });
        });
  }

  class Stored final : public composition::Component {
   public:
    static constexpr composition::AppStorage<bool> hints{"hints", false};
    static constexpr composition::SceneStorage<int> tab{"tab", 0};

    auto operator==(const Stored&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const composition::Binding<bool> shown = context.app_storage(hints);
      const composition::Binding<int> selected = context.scene_storage(tab);
      return components::VStack(
          components::Text(
              std::format("hints {} tab {}", shown.get(), selected.get())),
          components::Button("Flip", [shown, selected] {
            shown.set(!shown.get());
            selected.set(selected.get() + 1);
          }).id("flip"));
    }
  };

  class Traits final : public composition::Component {
   public:
    auto operator==(const Traits&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto horizontal =
          context.environment(components::Environment::horizontal_size_class);
      return components::Text(std::format(
          "scale {:.2f} pixel {:.2f} {}",
          context.environment(components::Environment::display_scale),
          context.environment(components::Environment::pixel_length),
          horizontal == components::UserInterfaceSizeClass::compact
              ? "compact"
              : "regular"));
    }
  };

  class Pulse final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> lit{"pulse-lit", false};
    static constexpr composition::StateKey<float> width{"pulse-width", 0.0F};

    auto operator==(const Pulse&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto on = context.state(lit);
      const auto measured = context.state(width);
      return components::VStack(
          components::ZStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .opacity(on.get() ? 1.0F : 0.2F)
              .on_appear([on] {
                motion::with_animation(motion::Animation::linear(1.0),
                                       [on] { on.set(true); });
              }),
          components::ZStack()
              .frame({.height = 10.0F, .max_width = composition::infinity})
              .on_geometry_change(
                  std::type_identity<float>{},
                  [](const composition::GeometryProxy& proxy) {
                    return proxy.size().width;
                  },
                  [measured](const float& value) { measured.set(value); }),
          components::ZStack()
              .frame({.width = measured.get() * 0.5F, .height = 10.0F})
              .id("half"));
    }
  };

  static auto activate_within(Root& root, const composition::Node& node)
      -> bool {
    for (const composition::Node& child : node.children()) {
      if (root.activate(child.id()) || activate_within(root, child)) {
        return true;
      }
    }
    return false;
  }

  static auto activate_inside(Application& application, std::string_view key)
      -> bool {
    const auto id = node_of(application, key);
    if (!id.has_value()) {
      return false;
    }
    return activate_within(application.root(),
                           application.root().tree().node(*id)->get());
  }

  static auto stack() -> Element {
    return presentation::NavigationStack(
        [] { return components::VStack(Element{Titled{}}); });
  }
};

const composition::PreferenceKey<DataFlowRuntimeTest::Selection>
    DataFlowRuntimeTest::selection{"selection", std::nullopt,
                                   [](Selection& value, Selection next) {
                                     if (!value.has_value()) {
                                       value = next;
                                     }
                                   }};

TEST_F(DataFlowRuntimeTest, ANestedComponentTitlesItsScreenInTheFirstFrame) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, stack());
  ASSERT_TRUE(application.has_value());
  ASSERT_TRUE(application->frame().has_value());
  EXPECT_TRUE(shows(*application, "Nested title"));
  EXPECT_TRUE(shows(*application, "Count 0"));
}

TEST_F(DataFlowRuntimeTest, NestedToolbarItemsFollowTheirComponentsState) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, stack());
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"count"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "Count 1"));
  EXPECT_FALSE(shows(application, "Count 0"));
}

TEST_F(DataFlowRuntimeTest, ANestedDestinationResolvesAPushedValue) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, stack());
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"open"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "Detail 5"));
  EXPECT_FALSE(shows(application, "No destination for this value"));
}

TEST_F(DataFlowRuntimeTest, AScaffoldTakesItsTitleFromANestedComponent) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, structure::Scaffold(components::VStack(Element{Titled{}})));
  EXPECT_TRUE(shows(application, "Nested title"));
}

TEST_F(DataFlowRuntimeTest, AnAnchorPreferenceRingFollowsTheFocusedTile) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, tiles());
  frames(application, 2);
  const auto ring_on = [&application](std::string_view tile) {
    const composition::Rect ring = frame_of(application, "ring");
    const composition::Rect target = frame_of(application, tile);
    return std::abs(ring.x - target.x) < 0.5F &&
           std::abs(ring.y - target.y) < 0.5F &&
           std::abs(ring.width - target.width) < 0.5F;
  };
  ASSERT_TRUE(node_of(application, "ring").has_value());

  EXPECT_EQ(focused_key(application), "tile-0");
  EXPECT_TRUE(shows(application, "Focused 0"));
  EXPECT_TRUE(ring_on("label-0"));
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), "tile-1");
  EXPECT_TRUE(shows(application, "Focused 1"));
  EXPECT_TRUE(shows(application, "Tile 0"));
  EXPECT_TRUE(ring_on("label-1"));
}

TEST_F(DataFlowRuntimeTest, ASettledAnchorPageDoesNoWork) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, tiles());
  frames(application, 4);
  const std::uint64_t resolved =
      application.root().tree().preference_resolutions();
  frames(application, 4);
  EXPECT_EQ(application.root().tree().preference_resolutions(), resolved);
  EXPECT_EQ(application.last_frame().evaluations, 0U);
  EXPECT_FALSE(application.last_frame().laid_out);
}

TEST_F(DataFlowRuntimeTest, StoredValuesSurviveARestart) {
  provider::headless::HeadlessProviderFactory providers;
  provider::headless::MemoryStorage storage;
  const auto options = [&storage] {
    return ApplicationOptions{.executors = std::nullopt,
                              .fonts = {},
                              .atlas = {},
                              .interaction = {},
                              .storage = StorageOptions{.provider = storage}};
  };
  {
    auto application = Application::create(providers, Stored{}, options());
    ASSERT_TRUE(application.has_value());
    frames(*application, 2);
    EXPECT_TRUE(shows(*application, "hints false tab 0"));
    ASSERT_TRUE(application->root().activate(composition::ElementKey{"flip"}));
    frames(*application, 2);
    EXPECT_TRUE(shows(*application, "hints true tab 1"));
  }
  const auto app_file = provider::AssetPath::create("app_storage.toml");
  ASSERT_TRUE(app_file.has_value());
  EXPECT_NE(storage.text(*app_file).find("hints = true"), std::string::npos);
  auto relaunched = Application::create(providers, Stored{}, options());
  ASSERT_TRUE(relaunched.has_value());
  frames(*relaunched, 2);
  EXPECT_TRUE(shows(*relaunched, "hints true tab 1"));
}

TEST_F(DataFlowRuntimeTest, AnUnreadableStoreIsKeptAndReported) {
  provider::headless::HeadlessProviderFactory providers;
  provider::headless::MemoryStorage storage;
  const auto app_file = provider::AssetPath::create("app_storage.toml");
  ASSERT_TRUE(app_file.has_value());
  storage.add(*app_file, "hints = = true");
  {
    auto application = Application::create(
        providers, Stored{},
        ApplicationOptions{.executors = std::nullopt,
                           .fonts = {},
                           .atlas = {},
                           .interaction = {},
                           .storage = StorageOptions{.provider = storage}});
    ASSERT_TRUE(application.has_value());
    frames(*application, 2);
    ASSERT_TRUE(application->root().activate(composition::ElementKey{"flip"}));
    frames(*application, 2);
  }
  EXPECT_EQ(storage.text(*app_file), "hints = = true");
  EXPECT_FALSE(providers.diagnostics().snapshot().empty());
}

TEST_F(DataFlowRuntimeTest, DisplayTraitsFollowTheDisplayMode) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().set_display_mode(provider::DisplayMode::handheld);
  Application application = start(providers, Traits{});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "scale 1.25 pixel 0.80 compact"));
  providers.surface().set_display_mode(provider::DisplayMode::docked);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "scale 1.25 pixel 0.80 regular"));
}

TEST_F(DataFlowRuntimeTest, DefaultAppStorageRedirectsASubtree) {
  provider::headless::HeadlessProviderFactory providers;
  const composition::UserDefaults store;
  store.set("hints", true);
  Application application =
      start(providers,
            components::VStack(Element{Stored{}}.default_app_storage(store)));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "hints true tab 0"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"flip"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "hints false tab 1"));
  EXPECT_EQ(store.get<bool>("hints"), std::optional<bool>{false});
}

TEST_F(DataFlowRuntimeTest, VerticalClassAndLuminanceReachViews) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().set_display_mode(provider::DisplayMode::handheld);
  auto seen = std::make_shared<std::vector<std::string>>();
  class Vertical final : public composition::Component {
   public:
    explicit Vertical(std::shared_ptr<std::vector<std::string>> seen)
        : seen_(std::move(seen)) {}

    auto operator==(const Vertical&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const bool compact =
          context.environment(components::Environment::vertical_size_class) ==
          components::UserInterfaceSizeClass::compact;
      const bool dimmed =
          context.environment(components::Environment::is_luminance_reduced);
      seen_->push_back(std::format("{} {}", compact ? "compact" : "regular",
                                   dimmed ? "dimmed" : "bright"));
      return components::Text("vertical");
    }

   private:
    std::shared_ptr<std::vector<std::string>> seen_;
  };
  Application application = start(providers, Vertical{seen});
  frames(application, 2);
  ASSERT_FALSE(seen->empty());
  EXPECT_EQ(seen->back(), "compact bright");
  providers.surface().set_display_mode(provider::DisplayMode::docked);
  frames(application, 3);
  EXPECT_EQ(seen->back(), "regular bright");
}

TEST_F(DataFlowRuntimeTest, ALinkOpensItsDestinationThroughTheBrowser) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            Element{components::Link("Learn more", "https://example.com")}.id(
                "link"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"link"}) ||
              activate_inside(application, "link"));
  frames(application, 1);
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().front().value(),
            "https://example.com");
}

TEST_F(DataFlowRuntimeTest, ACustomOpenUrlActionCanFallBackToTheSystem) {
  provider::headless::HeadlessProviderFactory providers;
  auto handled = std::make_shared<std::vector<std::string>>();
  Application application = start(
      providers,
      components::VStack(
          Element{components::Link("Kept", "https://kept.example")}.id("kept"),
          Element{components::Link("Passed", "https://passed.example")}.id(
              "passed"))
          .environment(
              components::Environment::open_url,
              components::OpenURLAction{[handled](const provider::Url& url) {
                handled->emplace_back(url.value());
                return url.value() == "https://kept.example"
                           ? components::OpenURLAction::Result::handled
                           : components::OpenURLAction::Result::system_action;
              }}));
  ASSERT_TRUE(activate_inside(application, "kept"));
  ASSERT_TRUE(activate_inside(application, "passed"));
  frames(application, 1);
  EXPECT_EQ(handled->size(), 2U);
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().front().value(),
            "https://passed.example");
}

TEST_F(DataFlowRuntimeTest,
       GeometryCallbacksSettleInTheirFrameAndKeepAppearAnimations) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Pulse{});
  ASSERT_TRUE(application.has_value());
  ASSERT_TRUE(application->frame().has_value());
  const float width = frame_of(*application, "half").width;
  EXPECT_GT(width, 1.0F);
  ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
  ASSERT_TRUE(application->frame().has_value());
  EXPECT_TRUE(application->last_frame().animating);
}

}
