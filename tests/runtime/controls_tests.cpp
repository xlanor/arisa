#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/bindable.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/controls/secure_field.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/introspection.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class ControlsTest : public FocusFixture {
 protected:
  enum class Shade : std::uint8_t {
    red,
    green,
    blue,
  };

  struct Probe {
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<double>> level;
    std::optional<composition::StateHandle<int>> count;
    std::optional<composition::StateHandle<Shade>> shade;
    std::optional<composition::StateHandle<std::string>> text;
    int actions{0};
  };

  struct Recording {
    int calls{0};
    bool pressed{false};
    bool focused{false};
    bool enabled{true};
    std::optional<components::ButtonRole> role;
  };

  struct RecordingStyle {
    Recording* log{nullptr};

    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> Element {
      ++log->calls;
      log->pressed = configuration.is_pressed;
      log->focused = configuration.is_focused;
      log->enabled = configuration.is_enabled;
      log->role = configuration.role;
      return std::move(configuration.label);
    }

    friend auto operator==(const RecordingStyle&, const RecordingStyle&)
        -> bool = default;
  };

  struct PressWrappingStyle {
    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> Element {
      if (!configuration.is_pressed) {
        return std::move(configuration.label);
      }
      return components::HStack(
          std::move(configuration.label)
              .background(components::Text("Pressed").id("halo")));
    }

    friend auto operator==(const PressWrappingStyle&, const PressWrappingStyle&)
        -> bool = default;
  };

  struct PressWrappingToggleStyle {
    auto make_body(controls::ToggleStyle::Configuration configuration) const
        -> Element {
      if (!configuration.is_pressed) {
        return components::VStack(std::move(configuration.label));
      }
      return components::VStack(
          components::Text("On").id("mark"),
          components::HStack(std::move(configuration.label)
                                 .overlay(components::Text("Ring"))));
    }

    friend auto operator==(const PressWrappingToggleStyle&,
                           const PressWrappingToggleStyle&) -> bool = default;
  };

  using Build = std::function<Element(Probe&, composition::Context&)>;

  class Host final : public composition::Component {
   public:
    Host(Probe* probe, const Build* build) noexcept
        : probe_(probe), build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(*probe_, context);
    }

   private:
    Probe* probe_;
    const Build* build_;
  };

  static constexpr composition::StateKey<bool> flag_key{"flag", false};
  static constexpr composition::StateKey<double> level_key{"level", 0.5};
  static constexpr composition::StateKey<int> count_key{"count", 2};
  static constexpr composition::StateKey<Shade> shade_key{"shade",
                                                          Shade::green};
  static const composition::StateKey<std::string> text_key;

  static auto shades() -> Element {
    return components::Group(components::Text("Red").tag(Shade::red),
                             components::Text("Green").tag(Shade::green),
                             components::Text("Blue").tag(Shade::blue));
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }

  static auto count_rasters(const composition::Node& node) -> int {
    int found =
        node.content_as<components::Image::Raster::Content>().has_value() ? 1
                                                                          : 0;
    for (const composition::Node& child : node.children()) {
      found += count_rasters(child);
    }
    return found;
  }

  static auto first_raster(const composition::Node& node)
      -> std::optional<composition::Rect> {
    if (node.content_as<components::Image::Raster::Content>().has_value()) {
      return node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (const auto found = first_raster(child)) {
        return found;
      }
    }
    return std::nullopt;
  }

  static auto automatic_button(const composition::Node& node)
      -> std::optional<bool> {
    if (const auto button = node.content_as<components::Button::Content>()) {
      return button->get().automatic();
    }
    for (const composition::Node& child : node.children()) {
      if (const auto found = automatic_button(child)) {
        return found;
      }
    }
    return std::nullopt;
  }

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) {
    for (int step = 0; step < 40; ++step) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  static void drag(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::PixelPoint from,
                   provider::PixelPoint to) {
    provider::InputFrame down;
    down.touches.push_back(
        provider::Touch{.id = provider::TouchId{5},
                        .position = from,
                        .phase = provider::TouchPhase::began});
    provider::InputFrame moved;
    moved.touches.push_back(
        provider::Touch{.id = provider::TouchId{5},
                        .position = to,
                        .phase = provider::TouchPhase::moved});
    provider::InputFrame up;
    up.touches.push_back(provider::Touch{.id = provider::TouchId{5},
                                         .position = to,
                                         .phase = provider::TouchPhase::ended});
    providers.input().push(down);
    providers.input().push(moved);
    providers.input().push(up);
    frames(application, 4);
  }

  static constexpr std::array<std::uint8_t, 76> square{
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00,
      0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
      0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D, 0x24,
      0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63,
      0xF8, 0xCF, 0xC0, 0xF0, 0x1F, 0x0C, 0x81, 0x34, 0x08, 0x34, 0x00,
      0x00, 0x49, 0x49, 0x09, 0x78, 0x9C, 0x51, 0x17, 0x92, 0x00, 0x00,
      0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
};

const composition::StateKey<std::string> ControlsTest::text_key{"text",
                                                                std::string{}};

TEST_F(ControlsTest, ToggleFlipsItsBindingOnActivateAndTap) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    return components::VStack(
        controls::Toggle("Wi-Fi", flag).frame({.width = 300.0F}).id("toggle"),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "toggle");
  press(providers, application, Button::a);
  EXPECT_TRUE(probe.flag->get());
  tap(providers, application, centre(application, "toggle"));
  EXPECT_FALSE(probe.flag->get());
}

TEST_F(ControlsTest, EveryToggleStyleStillToggles) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    return components::VStack(
        controls::Toggle("Check", flag)
            .toggle_style(controls::ToggleStyle::checkbox())
            .id("check"),
        controls::Toggle("Push", flag)
            .toggle_style(controls::ToggleStyle::button())
            .id("push"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  EXPECT_TRUE(probe.flag->get());
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "push");
  press(providers, application, Button::a);
  EXPECT_FALSE(probe.flag->get());
}

TEST_F(ControlsTest, DisabledToggleIsSkippedAndIgnoresTaps) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    return components::VStack(button("before"), controls::Toggle("Off", flag)
                                                    .frame({.width = 300.0F})
                                                    .disabled(true)
                                                    .id("toggle"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "before");
  tap(providers, application, centre(application, "toggle"));
  EXPECT_FALSE(probe.flag->get());
}

TEST_F(ControlsTest, SliderAdjustsSidewaysAndLetsVerticalFocusThrough) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto level = context.state(level_key);
    seen.level = level;
    return components::VStack(
        controls::Slider(level,
                         controls::ClosedRange<double>{.lower_bound = 0.0,
                                                       .upper_bound = 1.0},
                         0.1)
            .frame({.width = 224.0F})
            .id("slider"),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "slider");
  press(providers, application, Button::dpad_right);
  EXPECT_NEAR(probe.level->get(), 0.6, 1e-9);
  press(providers, application, Button::dpad_left);
  press(providers, application, Button::dpad_left);
  EXPECT_NEAR(probe.level->get(), 0.4, 1e-9);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "after");
  EXPECT_NEAR(probe.level->get(), 0.4, 1e-9);
}

TEST_F(ControlsTest, SliderFollowsTheFinger) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto level = context.state(level_key);
    seen.level = level;
    return components::VStack(
        controls::Slider(level).frame({.width = 224.0F}).id("slider"));
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Rect frame = frame_of(application, "slider");
  const float middle = frame.y + (frame.height * 0.5F);
  const float knob = controls::Slider::knob_extent * 0.5F;
  drag(providers, application,
       provider::PixelPoint{.x = frame.x + knob + 50.0F, .y = middle},
       provider::PixelPoint{.x = frame.x + frame.width - knob, .y = middle});
  EXPECT_NEAR(probe.level->get(), 1.0, 1e-6);
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + knob + 50.0F, .y = middle});
  EXPECT_NEAR(probe.level->get(),
              50.0 / (224.0 - controls::Slider::knob_extent), 1e-6);
}

TEST_F(ControlsTest, SliderValuesSnapAndClamp) {
  const controls::ClosedRange<double> bounds{.lower_bound = 10.0,
                                             .upper_bound = 20.0};
  EXPECT_DOUBLE_EQ(controls::Slider::snapped(13.4, bounds, 2.0), 14.0);
  EXPECT_DOUBLE_EQ(controls::Slider::snapped(25.0, bounds, std::nullopt), 20.0);
  EXPECT_DOUBLE_EQ(controls::Slider::fraction(15.0, bounds), 0.5);
  EXPECT_DOUBLE_EQ(
      controls::Slider::nudged(20.0, composition::MoveCommandDirection::right,
                               bounds, std::nullopt),
      20.0);
  EXPECT_DOUBLE_EQ(
      controls::Slider::nudged(15.0, composition::MoveCommandDirection::left,
                               bounds, std::nullopt),
      14.5);
}

TEST_F(ControlsTest, StepperRunsIncrementAndDecrementActions) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<std::vector<std::string>>();
  Probe probe;
  const Build build = [log](Probe&, composition::Context&) {
    return components::VStack(
        controls::Stepper(
            "Page", [log] { log->emplace_back("next"); },
            [log] { log->emplace_back("previous"); },
            [log](bool editing) {
              log->emplace_back(editing ? "begin" : "end");
            })
            .frame({.width = 300.0F})
            .id("pager"),
        controls::Stepper(
            "Forward only", [log] { log->emplace_back("forward"); }, nullptr)
            .frame({.width = 300.0F})
            .id("forward"));
  };
  Application application = start(providers, Host{&probe, &build});
  const std::string lower{controls::Stepper::lower_key};
  const std::string raise{controls::Stepper::raise_key};
  EXPECT_EQ(focused_key(application), lower);
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::a);
  EXPECT_EQ(*log, (std::vector<std::string>{"begin", "previous", "end", "begin",
                                            "next", "end"}));
  log->clear();
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  EXPECT_EQ(*log, std::vector<std::string>{"forward"});
}

TEST_F(ControlsTest, StepperHalvesAreIndependentTargets) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(
        controls::Stepper(
            "Quantity", count,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 6})
            .frame({.width = 300.0F})
            .id("stepper"),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  const std::string lower{controls::Stepper::lower_key};
  const std::string raise{controls::Stepper::raise_key};
  EXPECT_EQ(focused_key(application), lower);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), raise);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.count->get(), 3);
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(focused_key(application), lower);
  press(providers, application, Button::a);
  press(providers, application, Button::a);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.count->get(), 0);
  EXPECT_EQ(focused_key(application), raise);
  hold(providers, application, pad({Button::a}),
       std::chrono::milliseconds{2000});
  EXPECT_EQ(probe.count->get(), 6);
  EXPECT_EQ(focused_key(application), lower);
  const composition::Rect frame = frame_of(application, "stepper");
  const float middle = frame.y + (frame.height * 0.5F);
  const float right = frame.x + frame.width;
  tap(providers, application,
      provider::PixelPoint{
          .x = right - (controls::Stepper::segment_width * 1.5F) - 1.0F,
          .y = middle});
  EXPECT_EQ(probe.count->get(), 5);
  tap(providers, application,
      provider::PixelPoint{
          .x = right - (controls::Stepper::segment_width * 0.5F), .y = middle});
  EXPECT_EQ(probe.count->get(), 6);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "after");
}

TEST_F(ControlsTest, SegmentedPickerMovesTheSelectionSideways) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::segmented())
            .frame({.width = 300.0F}),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  press(providers, application, Button::dpad_left);
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "after");
}

TEST_F(ControlsTest, DisabledSegmentedPickerDimsAndIsSkipped) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        button("before"),
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::segmented())
            .frame({.width = 300.0F})
            .disabled(true),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "before");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "after");
  std::optional<float> dimmed;
  const std::function<void(const composition::Node&)> visit =
      [&](const composition::Node& node) {
        if (node.content_as<controls::PickerStyle::Segmented::Content>()
                .has_value()) {
          dimmed = node.modifiers().opacity;
        }
        for (const composition::Node& child : node.children()) {
          visit(child);
        }
      };
  visit(application.root().tree().root());
  ASSERT_TRUE(dimmed.has_value());
  EXPECT_FLOAT_EQ(*dimmed, 0.4F);
}

TEST_F(ControlsTest, MenuPickerChoosesFromAPopover) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(controls::Picker("Shade", shade, [] {
                                return shades();
                              }).frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(texts(application).size(), 3U);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Red"));
  EXPECT_TRUE(shows(application, "Blue"));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  EXPECT_FALSE(shows(application, "Blue"));
}

TEST_F(ControlsTest, NavigationLinkPickerChoosesFromASheet) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::navigation_link())
            .frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FALSE(shows(application, "Red"));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Red"));
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  EXPECT_FALSE(shows(application, "Red"));
}

TEST_F(ControlsTest, NavigationLinkPickerPushesOntoTheEnclosingStack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return presentation::NavigationStack([shade] {
      return components::VStack(
          controls::Picker("Shade", shade, [] { return shades(); })
              .picker_style(controls::PickerStyle::navigation_link())
              .frame({.width = 300.0F}));
    });
  };
  Application application = start(providers, Host{&probe, &build});
  const std::optional<composition::NodeId> trigger =
      application.interaction().focused();
  ASSERT_TRUE(trigger.has_value());
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Red"));
  std::size_t sheets = 0;
  const std::function<void(const composition::Node&)> count =
      [&](const composition::Node& node) {
        if (const auto layer = node.content_as<composition::Layer::Content>();
            layer.has_value() &&
            layer->get().layer() == composition::OverlayLayer::sheet) {
          ++sheets;
        }
        for (const composition::Node& child : node.children()) {
          count(child);
        }
      };
  count(application.root().tree().root());
  EXPECT_EQ(sheets, 0U);
  press(providers, application, Button::b);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "Red"));
  EXPECT_EQ(application.interaction().focused(), trigger);
  press(providers, application, Button::a);
  settle(providers, application);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  EXPECT_FALSE(shows(application, "Red"));
  EXPECT_EQ(application.interaction().focused(), trigger);
}

TEST_F(ControlsTest, WheelPickerTurnsOnlyWhileEngaged) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::wheel())
            .frame({.width = 300.0F}),
        components::Button("After", [] {}).id("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_EQ(probe.shade->get(), Shade::green);
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  press(providers, application, Button::dpad_up);
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  EXPECT_EQ(focused_key(application), "after");
}

TEST_F(ControlsTest, WheelPickerFlicksCarryPastTheDrag) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::wheel())
            .frame({.width = 300.0F})
            .id("wheel"));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_EQ(probe.shade->get(), Shade::green);
  const provider::PixelPoint middle = centre(application, "wheel");
  const auto touch = [&](float lift, provider::TouchPhase phase) {
    provider::InputFrame frame;
    frame.touches.push_back(provider::Touch{
        .id = provider::TouchId{5},
        .position = provider::PixelPoint{.x = middle.x, .y = middle.y - lift},
        .phase = phase});
    providers.input().push(std::move(frame));
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    frames(application, 1);
  };
  touch(0.0F, provider::TouchPhase::began);
  touch(8.0F, provider::TouchPhase::moved);
  touch(16.0F, provider::TouchPhase::moved);
  touch(24.0F, provider::TouchPhase::moved);
  EXPECT_EQ(probe.shade->get(), Shade::green);
  touch(24.0F, provider::TouchPhase::ended);
  frames(application, 2);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
}

TEST_F(ControlsTest, PalettePickerStepsTheSelectionSideways) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] {
          return shades();
        }).picker_style(controls::PickerStyle::palette()));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_EQ(probe.shade->get(), Shade::green);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  press(providers, application, Button::dpad_left);
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(probe.shade->get(), Shade::red);
}

TEST_F(ControlsTest, InlinePickerRowsSelectOnActivate) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::radio_group())
            .frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
}

TEST_F(ControlsTest, HorizontalRadioGroupLayoutLaysTheOptionsInARow) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::radio_group())
            .horizontal_radio_group_layout()
            .id("horizontal"),
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::radio_group())
            .id("vertical"));
  };
  Application application = start(providers, Host{&probe, &build});
  const auto buttons = [&application](std::string_view key) {
    std::vector<composition::Rect> found;
    const auto walk = [&found](const auto& self,
                               const composition::Node& node) -> void {
      if (node.content_as<components::Button::Content>().has_value()) {
        found.push_back(node.frame());
        return;
      }
      for (const composition::Node& child : node.children()) {
        self(self, child);
      }
    };
    if (const auto node = node_named(application, key)) {
      walk(walk, node->get());
    }
    return found;
  };
  const std::vector<composition::Rect> row = buttons("horizontal");
  const std::vector<composition::Rect> column = buttons("vertical");
  ASSERT_EQ(row.size(), 3U);
  ASSERT_EQ(column.size(), 3U);
  for (std::size_t index = 1; index < row.size(); ++index) {
    EXPECT_FLOAT_EQ(row[index].y, row[0].y);
    EXPECT_FLOAT_EQ(row[index].x,
                    row[index - 1].x + row[index - 1].width +
                        controls::PickerStyle::radio_group_spacing);
    EXPECT_GT(column[index].y, column[index - 1].y);
  }
  EXPECT_LT(frame_of(application, "horizontal").height,
            frame_of(application, "vertical").height);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
}

TEST_F(ControlsTest, PickerOptionsComeFromTaggedContent) {
  int chosen = 0;
  const controls::Picker::Selection selection{
      .is_selected =
          [](const composition::TagValue& tag) {
            return *tag.get<Shade>() == Shade::blue;
          },
      .select = [&chosen](const composition::TagValue&) { ++chosen; }};
  std::vector<controls::PickerStyle::Option> options =
      controls::Picker::options(
          [] {
            return components::Group(components::Text("Red").tag(Shade::red),
                                     components::Text("untagged"),
                                     components::Text("Blue").tag(Shade::blue));
          },
          selection);
  ASSERT_EQ(options.size(), 2U);
  EXPECT_FALSE(options[0].is_selected);
  EXPECT_TRUE(options[1].is_selected);
  EXPECT_EQ(controls::PickerStyle::selected_index(options),
            std::optional<std::size_t>{1});
  options[0].select();
  EXPECT_EQ(chosen, 1);
}

TEST_F(ControlsTest, MenuRunsTheChosenActionAndCloses) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(controls::Menu("Actions", [target] {
      return components::Group(
          components::Button("Duplicate", [target] { ++target->actions; }),
          components::Button("Delete", [target] { target->actions += 10; }));
    }));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FALSE(shows(application, "Duplicate"));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Duplicate"));
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.actions, 10);
  EXPECT_FALSE(shows(application, "Duplicate"));
}

TEST_F(ControlsTest, AMenuUsesTheButtonStyleItIsGiven) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Recording recording;
  const Build build = [&recording](Probe&, composition::Context&) {
    return components::VStack(
        controls::Menu("Actions",
                       [] { return components::Button("Duplicate", [] {}); })
            .menu_style(controls::MenuStyle::button())
            .button_style(RecordingStyle{&recording}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GT(recording.calls, 0);
}

TEST_F(ControlsTest, AnAutomaticMenuKeepsItsBorderedButton) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Recording recording;
  const Build build = [&recording](Probe&, composition::Context&) {
    return components::VStack(controls::Menu("Actions", [] {
                                return components::Button("Duplicate", [] {});
                              }).button_style(RecordingStyle{&recording}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(recording.calls, 0);
}

TEST_F(ControlsTest, ButtonStyleReachesButtonsThroughTheEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Recording recording;
  const Build build = [&recording](Probe&, composition::Context&) {
    return components::VStack(
        components::Button("Delete", components::ButtonRole::destructive, [] {})
            .frame({.width = 100.0F, .height = 50.0F})
            .button_style(RecordingStyle{&recording})
            .id("styled"),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 2);
  EXPECT_GT(recording.calls, 0);
  EXPECT_TRUE(recording.focused);
  EXPECT_TRUE(recording.enabled);
  EXPECT_EQ(recording.role, components::ButtonRole::destructive);
  EXPECT_FALSE(recording.pressed);
  for (int held = 0; held < 3; ++held) {
    providers.input().push(pad({Button::a}));
    frames(application, 1);
  }
  EXPECT_TRUE(recording.pressed);
  providers.input().push(pad({}));
  frames(application, 3);
  EXPECT_FALSE(recording.pressed);
  press(providers, application, Button::dpad_down);
  frames(application, 2);
  EXPECT_FALSE(recording.focused);
}

TEST_F(ControlsTest, AStyleThatWrapsItsLabelOnlyWhenPressedKeepsTheLabel) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::Button(
            [] {},
            components::Label("Share", components::Image::system_name("star"))
                .id("label"))
            .frame({.width = 160.0F, .height = 50.0F})
            .button_style(PressWrappingStyle{})
            .id("styled"),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 2);
  ASSERT_TRUE(shows(application, "Share"));
  const auto label = node_of(application, "label");
  ASSERT_TRUE(label.has_value());
  for (int round = 0; round < 3; ++round) {
    for (int held = 0; held < 3; ++held) {
      providers.input().push(pad({Button::a}));
      frames(application, 1);
    }
    EXPECT_TRUE(shows(application, "Pressed"));
    EXPECT_TRUE(shows(application, "Share"));
    EXPECT_EQ(node_of(application, "label"), label);
    providers.input().push(pad({}));
    settle(providers, application);
    EXPECT_FALSE(shows(application, "Pressed"));
    EXPECT_TRUE(shows(application, "Share"));
    EXPECT_EQ(node_of(application, "label"), label);
  }
}

TEST_F(ControlsTest, AToggleStyleThatWrapsItsLabelWhenPressedKeepsTheLabel) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    return components::VStack(controls::Toggle(flag, components::Text("Wi-Fi"))
                                  .frame({.width = 300.0F})
                                  .toggle_style(PressWrappingToggleStyle{})
                                  .id("toggle"),
                              button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_TRUE(shows(application, "Wi-Fi"));
  for (int round = 0; round < 2; ++round) {
    for (int held = 0; held < 3; ++held) {
      providers.input().push(pad({Button::a}));
      frames(application, 1);
    }
    EXPECT_TRUE(shows(application, "On"));
    EXPECT_TRUE(shows(application, "Ring"));
    EXPECT_TRUE(shows(application, "Wi-Fi"));
    providers.input().push(pad({}));
    settle(providers, application);
    EXPECT_FALSE(shows(application, "Ring"));
    EXPECT_TRUE(shows(application, "Wi-Fi"));
  }
}

TEST_F(ControlsTest, AStyledLabelLeavesWithItsButton) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  int disappeared = 0;
  const Build build = [&disappeared](Probe& seen,
                                     composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    if (flag.get()) {
      return components::VStack(button("after"));
    }
    return components::VStack(
        components::Button([] {}, components::Text("Gone").on_disappear(
                                      [&disappeared] { ++disappeared; }))
            .frame({.width = 160.0F, .height = 50.0F})
            .button_style(PressWrappingStyle{}),
        button("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_TRUE(shows(application, "Gone"));
  probe.flag->set(true);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "Gone"));
  EXPECT_EQ(disappeared, 1);
}

TEST_F(ControlsTest, DisabledButtonsReportNotEnabledToTheirStyle) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Recording recording;
  const Build build = [&recording](Probe&, composition::Context&) {
    return components::VStack(components::Button("Off", [] {})
                                  .frame({.width = 100.0F, .height = 50.0F})
                                  .disabled(true)
                                  .button_style(RecordingStyle{&recording}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GT(recording.calls, 0);
  EXPECT_FALSE(recording.enabled);
}

TEST_F(ControlsTest, TheNearestButtonStyleWins) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
               components::VStack(button("inner"))
                   .button_style(components::ButtonStyle::automatic()))
        .button_style(components::ButtonStyle::bordered());
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(automatic_button(application.root().tree().root()),
            std::optional<bool>{true});
  provider::headless::HeadlessProviderFactory others;
  const Build outer = [](Probe&, composition::Context&) {
    return components::VStack(button("only"))
        .button_style(components::ButtonStyle::borderless());
  };
  Application styled = start(others, Host{&probe, &outer});
  EXPECT_EQ(automatic_button(styled.root().tree().root()),
            std::optional<bool>{false});
}

TEST_F(ControlsTest, ButtonStylesCompareByKindAndCustomValue) {
  Recording one;
  Recording two;
  EXPECT_EQ(components::ButtonStyle::bordered(),
            components::ButtonStyle::bordered());
  EXPECT_NE(components::ButtonStyle::bordered(),
            components::ButtonStyle::plain());
  EXPECT_EQ(components::ButtonStyle{RecordingStyle{&one}},
            components::ButtonStyle{RecordingStyle{&one}});
  EXPECT_NE(components::ButtonStyle{RecordingStyle{&one}},
            components::ButtonStyle{RecordingStyle{&two}});
  EXPECT_TRUE(components::ButtonStyle::automatic().is_automatic());
  EXPECT_FALSE(components::ButtonStyle{RecordingStyle{&one}}.is_automatic());
}

TEST_F(ControlsTest, TextFieldWritesTheKeyboardResult) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto text = context.state(text_key);
    seen.text = text;
    return components::VStack(
        controls::TextField("Name", text).frame({.width = 300.0F}).id("name"));
  };
  Application application = start(providers, Host{&probe, &build});
  providers.text_input().push(std::string{"Kyogre"});
  press(providers, application, Button::a);
  frames(application, 8);
  EXPECT_EQ(probe.text->get(), "Kyogre");
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  EXPECT_EQ(providers.text_input().requests().front().hint, "Name");
  EXPECT_EQ(providers.text_input().requests().front().kind,
            provider::TextInputKind::text);
  EXPECT_FALSE(providers.text_input().requests().front().traits.multiline);
}

TEST_F(ControlsTest, ControlsBindDirectlyToAModelWithNoMirrorState) {
  struct Settings {
    bool on{false};
    double level{0.5};
    int count{2};
    Shade shade{Shade::green};

    auto operator==(const Settings&) const -> bool = default;
  };
  composition::Observable<Settings> model;
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [&model](Probe&, composition::Context& context) {
    const composition::Bindable settings{context, model};
    const Settings& seen = context.read(model);
    return components::VStack(
        controls::Toggle("On", settings[&Settings::on]).id("toggle"),
        controls::Slider(settings[&Settings::level],
                         controls::ClosedRange<double>{.lower_bound = 0.0,
                                                       .upper_bound = 1.0},
                         0.25)
            .frame({.width = 224.0F})
            .id("slider"),
        controls::Stepper(
            "Count", settings[&Settings::count],
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 5}),
        controls::Picker("Shade", settings[&Settings::shade],
                         [] { return shades(); }),
        components::Text(
            std::format("{} {:.2f} {}", seen.on, seen.level, seen.count)));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "false 0.50 2"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"toggle"}));
  frames(application, 2);
  EXPECT_TRUE(model.get().on);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_right);
  EXPECT_DOUBLE_EQ(model.get().level, 0.75);
  model.write(&Settings::count, 4);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "true 0.75 4"));
}

TEST_F(ControlsTest, SliderReportsEditingAroundEachStepAndShowsValueLabels) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  auto edits = std::make_shared<std::vector<bool>>();
  const Build build = [edits](Probe& seen, composition::Context& context) {
    const auto level = context.state(level_key);
    seen.level = level;
    const composition::Binding<float> narrow{
        [level] { return static_cast<float>(level.get()); },
        [level](float value) { level.set(static_cast<double>(value)); }};
    return components::VStack(
        controls::Slider(narrow,
                         controls::ClosedRange<float>{.lower_bound = 0.0F,
                                                      .upper_bound = 1.0F},
                         std::optional<float>{0.25F}, components::Text("Low"),
                         components::Text("High"),
                         [edits](bool editing) { edits->push_back(editing); })
            .frame({.width = 300.0F})
            .id("slider"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Low"));
  EXPECT_TRUE(shows(application, "High"));
  press(providers, application, Button::dpad_right);
  EXPECT_NEAR(probe.level->get(), 0.75, 1e-6);
  EXPECT_EQ(*edits, (std::vector<bool>{true, false}));
}

TEST_F(ControlsTest, NumericTextFieldsParseAndUseTheNumberPad) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(
        controls::TextField("Age", count, controls::NumberFormat::number())
            .frame({.width = 300.0F})
            .id("age"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "2"));
  providers.text_input().push(std::string{"42"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"age"}));
  frames(application, 8);
  EXPECT_EQ(probe.count->get(), 42);
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  EXPECT_EQ(providers.text_input().requests().front().initial, "2");
  EXPECT_EQ(providers.text_input().requests().front().traits.keyboard,
            provider::KeyboardType::numbers_and_punctuation);
  providers.text_input().push(std::string{"4x"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"age"}));
  frames(application, 8);
  EXPECT_EQ(probe.count->get(), 42);
  const controls::NumberFormat money =
      controls::NumberFormat::number().precision(2);
  EXPECT_EQ(money.format(3.14159), "3.14");
  EXPECT_EQ(money.parse<double>(" +2.5 "), 2.5);
  EXPECT_FALSE(money.parse<int>("2.5").has_value());
  EXPECT_EQ(controls::NumberFormat::keyboard<unsigned>(),
            provider::KeyboardType::number_pad);
}

TEST_F(ControlsTest, TextFieldShowsItsPromptAndTextEditorsAskForLines) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto text = context.state(text_key);
    seen.text = text;
    return components::VStack(
        controls::TextField("Name", text,
                            controls::TextFieldOptions{.prompt = "Required"})
            .frame({.width = 300.0F})
            .id("name"),
        controls::TextEditor(text).frame({.width = 300.0F}).id("editor"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Required"));
  EXPECT_FALSE(shows(application, "Name"));
  providers.text_input().push(std::string{"one\ntwo"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"editor"}));
  frames(application, 8);
  EXPECT_EQ(probe.text->get(), "one\ntwo");
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  EXPECT_TRUE(providers.text_input().requests().front().traits.multiline);
  EXPECT_GE(frame_of(application, "editor").height,
            controls::TextEditor::minimum_height);
}

TEST_F(ControlsTest, SubmitRunsTheEnclosingActionsUpToASubmitScope) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  auto fired = std::make_shared<std::vector<std::string>>();
  const Build build = [fired](Probe& seen, composition::Context& context) {
    const auto text = context.state(text_key);
    seen.text = text;
    return components::VStack(
               components::VStack(
                   controls::TextField("Inner", text)
                       .frame({.width = 300.0F})
                       .id("inner")
                       .on_submit([fired] { fired->push_back("field"); }))
                   .submit_scope()
                   .on_submit([fired] { fired->push_back("blocked"); }),
               controls::TextField("Outer", text)
                   .frame({.width = 300.0F})
                   .id("outer")
                   .submit_label(composition::SubmitLabel::search))
        .on_submit([fired] { fired->push_back("page"); });
  };
  Application application = start(providers, Host{&probe, &build});
  providers.text_input().push(std::string{"a"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"inner"}));
  frames(application, 8);
  EXPECT_EQ(*fired, std::vector<std::string>{"field"});
  providers.text_input().push(std::string{"b"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"outer"}));
  frames(application, 8);
  EXPECT_EQ(*fired, (std::vector<std::string>{"field", "page"}));
  EXPECT_EQ(providers.text_input().requests().back().traits.submit_label,
            provider::SubmitLabel::search);
  providers.text_input().push(std::unexpected(
      provider::ProviderError{provider::ProviderErrorCode::cancelled}));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"outer"}));
  frames(application, 8);
  EXPECT_EQ(fired->size(), 2U);
}

TEST_F(ControlsTest,
       KeyboardConfigurationReachesTheRequestAndRefusalsAreNoted) {
  provider::headless::HeadlessProviderFactory providers;
  provider::TextInputTraitSet refused;
  refused.insert(provider::TextInputTrait::autocapitalization);
  providers.text_input().refuse(refused);
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto text = context.state(text_key);
    seen.text = text;
    return components::VStack(
        controls::TextField("Handle", text)
            .frame({.width = 300.0F})
            .id("handle")
            .keyboard_type(provider::KeyboardType::email_address)
            .text_content_type(provider::TextContentType::username)
            .text_input_autocapitalization(
                provider::TextInputAutocapitalization::never)
            .autocorrection_disabled());
  };
  Application application = start(providers, Host{&probe, &build});
  providers.text_input().push(std::string{"kyogre"});
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"handle"}));
  frames(application, 8);
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  const provider::TextInputTraits traits =
      providers.text_input().requests().front().traits;
  EXPECT_EQ(traits.keyboard, provider::KeyboardType::email_address);
  EXPECT_EQ(traits.content, provider::TextContentType::username);
  EXPECT_EQ(traits.capitalization,
            provider::TextInputAutocapitalization::never);
  EXPECT_EQ(traits.autocorrection_disabled, std::optional<bool>{true});
  bool noted = false;
  for (const Introspection::NodeRecord& record :
       application.introspection().nodes()) {
    for (const std::string& note : record.notes) {
      noted = noted || note == "autocapitalization";
    }
  }
  EXPECT_TRUE(noted);
}

TEST_F(ControlsTest, SecureFieldAsksForAPasswordAndShowsBullets) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto text = context.state(text_key);
    seen.text = text;
    return components::VStack(controls::SecureField("Password", text)
                                  .frame({.width = 300.0F})
                                  .id("secret"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Password"));
  providers.text_input().push(std::string{"hunter2"});
  press(providers, application, Button::a);
  frames(application, 8);
  EXPECT_EQ(probe.text->get(), "hunter2");
  EXPECT_EQ(providers.text_input().requests().front().kind,
            provider::TextInputKind::password);
  EXPECT_TRUE(shows(application, "•••••••"));
  EXPECT_FALSE(shows(application, "hunter2"));
}

TEST_F(ControlsTest, ImagesLoadFromAssetsWithAPlaceholderWhenMissing) {
  provider::headless::HeadlessProviderFactory providers;
  providers.assets().add(*provider::AssetPath::create("pictures/square.png"),
                         std::as_bytes(std::span{square}));
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::HStack(components::Image("pictures/square.png"),
                              components::Image("pictures/missing.png"));
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 2);
  const composition::Node& root = application.root().tree().root();
  EXPECT_EQ(count_rasters(root), 1);
  const auto raster = first_raster(root);
  ASSERT_TRUE(raster.has_value());
  EXPECT_FLOAT_EQ(raster->width, 2.0F);
  EXPECT_FLOAT_EQ(raster->height, 2.0F);
}

TEST_F(ControlsTest, FixedImagesSitAtTheirFrameAlignmentAndResizableOnesFit) {
  provider::headless::HeadlessProviderFactory providers;
  providers.assets().add(*provider::AssetPath::create("pictures/square.png"),
                         std::as_bytes(std::span{square}));
  providers.gpu().rasterize(providers.surface().size());
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::VStackOptions{
            .alignment = components::HorizontalAlignment::leading,
            .spacing = 0.0F},
        components::Image("pictures/square.png")
            .frame({.width = 40.0F,
                    .height = 40.0F,
                    .alignment = composition::Alignment::top_leading})
            .id("top_leading"),
        components::Image("pictures/square.png")
            .frame({.width = 40.0F, .height = 40.0F})
            .id("centre"),
        components::Image("pictures/square.png")
            .frame({.width = 40.0F,
                    .height = 40.0F,
                    .alignment = composition::Alignment::bottom_trailing})
            .id("bottom_trailing"),
        components::Image("pictures/square.png")
            .resizable()
            .scaled_to_fit()
            .frame({.width = 60.0F, .height = 20.0F})
            .id("fit"),
        components::Image("pictures/square.png")
            .resizable()
            .scaled_to_fill()
            .frame({.width = 60.0F, .height = 20.0F})
            .id("fill"));
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 4);
  const auto raster_in = [&application](std::string_view key) {
    const auto node = node_named(application, key);
    return node.has_value() ? first_raster(node->get())
                            : std::optional<composition::Rect>{};
  };
  const auto offset = [&application, &raster_in](std::string_view key) {
    const auto node = node_named(application, key);
    const composition::Rect outer =
        node.has_value() ? node->get().frame() : composition::Rect{};
    const composition::Rect inner = raster_in(key).value();
    return std::array{inner.x - outer.x, inner.y - outer.y, inner.width,
                      inner.height};
  };
  EXPECT_EQ(offset("top_leading"), (std::array{0.0F, 0.0F, 2.0F, 2.0F}));
  EXPECT_EQ(offset("centre"), (std::array{19.0F, 19.0F, 2.0F, 2.0F}));
  EXPECT_EQ(offset("bottom_trailing"), (std::array{38.0F, 38.0F, 2.0F, 2.0F}));
  EXPECT_EQ(offset("fit"), (std::array{20.0F, 0.0F, 20.0F, 20.0F}));
  EXPECT_EQ(offset("fill"), (std::array{0.0F, -20.0F, 60.0F, 60.0F}));
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const composition::Rect corner = raster_in("bottom_trailing").value();
  const auto ground =
      image->pixel(static_cast<std::uint32_t>(corner.x - 10.0F),
                   static_cast<std::uint32_t>(corner.y - 10.0F));
  const auto drawn = image->pixel(static_cast<std::uint32_t>(corner.x + 1.0F),
                                  static_cast<std::uint32_t>(corner.y + 1.0F));
  ASSERT_TRUE(ground.has_value() && drawn.has_value());
  EXPECT_NE(*ground, *drawn);
}

TEST_F(ControlsTest, LabelStylesChooseTitleOrIcon) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::Label("Both", components::Text("icon-a")),
        components::Label("Title", components::Text("icon-b"))
            .label_style(components::LabelStyle::title_only()),
        components::Label("Hidden", components::Text("icon-c"))
            .label_style(components::LabelStyle::icon_only()));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Both"));
  EXPECT_TRUE(shows(application, "icon-a"));
  EXPECT_TRUE(shows(application, "Title"));
  EXPECT_FALSE(shows(application, "icon-b"));
  EXPECT_FALSE(shows(application, "Hidden"));
  EXPECT_TRUE(shows(application, "icon-c"));
}

TEST_F(ControlsTest, DividersAreHairlinesAcrossTheirStack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::VStack(button("top"), components::Divider().id("across"),
                           button("bottom"))
            .frame({.width = 240.0F}),
        components::HStack(button("left"), components::Divider().id("down"),
                           button("right"))
            .fixed_size(false, true),
        components::HStack(button("solo"), components::Divider().id("free"))
            .frame({.height = 120.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Rect across = frame_of(application, "across");
  EXPECT_FLOAT_EQ(across.width, 240.0F);
  EXPECT_GT(across.height, 0.0F);
  EXPECT_LE(across.height, 1.0F);
  const composition::Rect down = frame_of(application, "down");
  EXPECT_FLOAT_EQ(down.height, 50.0F);
  EXPECT_LE(down.width, 1.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "free").height, 120.0F);
}

TEST_F(ControlsTest, ProgressFractionsClampAndSpinnersKeepTicking) {
  EXPECT_EQ(controls::ProgressView::fraction(5.0, 10.0), 0.5);
  EXPECT_EQ(controls::ProgressView::fraction(15.0, 10.0), 1.0);
  EXPECT_EQ(controls::ProgressView::fraction(1.0, 0.0), 0.0);
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(controls::ProgressView());
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(application.last_frame().animating);
  EXPECT_FALSE(application.root().tree().dirty());
  const Build settled = [](Probe&, composition::Context&) {
    return components::VStack(controls::ProgressView(0.5));
  };
  provider::headless::HeadlessProviderFactory others;
  Application still = start(others, Host{&probe, &settled});
  frames(still, 2);
  EXPECT_FALSE(still.root().tree().dirty());
}

TEST_F(ControlsTest, NotANumberProgressDrawsAsIndeterminate) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [nan](Probe&, composition::Context&) {
    return components::VStack(
        controls::ProgressView(nan, 1.0),
        controls::ProgressViewStyle::bar(nan, nan).id("bar"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(application.last_frame().animating);
  const std::function<bool(const composition::Node&)> indeterminate =
      [&indeterminate](const composition::Node& node) {
        if (node.content_as<
                    controls::ProgressViewStyle::Indeterminate::Content>()
                .has_value()) {
          return true;
        }
        return std::ranges::any_of(node.children(), indeterminate);
      };
  EXPECT_TRUE(indeterminate(application.root().tree().root()));
  const auto bar = node_named(application, "bar");
  ASSERT_TRUE(bar.has_value());
  const std::function<void(const composition::Node&)> finite =
      [&finite](const composition::Node& node) {
        EXPECT_TRUE(std::isfinite(node.frame().width));
        EXPECT_TRUE(std::isfinite(node.frame().height));
        for (const composition::Node& child : node.children()) {
          finite(child);
        }
      };
  finite(bar->get());
  EXPECT_GT(bar->get().frame().width, 0.0F);
}

}
