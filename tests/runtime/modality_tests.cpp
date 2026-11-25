#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/input_modality.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using provider::Button;

class ModalityTest : public FocusFixture {
 protected:
  class Column final : public composition::Component {
   public:
    auto operator==(const Column&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const composition::InputModality modality =
          context.environment(components::Environment::input_modality);
      std::string name = "controller";
      if (modality == composition::InputModality::touch) {
        name = "touch";
      } else if (modality == composition::InputModality::keyboard_and_pointer) {
        name = "keyboard";
      }
      return components::VStack(button("a"), button("b"), button("c"),
                                components::Text(std::move(name)));
    }
  };

  class Long final : public composition::Component {
   public:
    auto operator==(const Long&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      std::vector<composition::Element> items;
      for (int index = 0; index < 20; ++index) {
        items.push_back(button(std::format("i{}", index), 100.0F, 20.0F));
      }
      return components::VStack(std::move(items));
    }
  };
};

TEST_F(ModalityTest, TapsShowTheRingOnTheTappedItem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Column{});
  tap(providers, application, centre(application, "b"));
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_TRUE(application.interaction().focus_visible());
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c");
}

TEST_F(ModalityTest, TouchHidesTheRingAndTheFirstDirectionRestoresIt) {
  provider::headless::HeadlessProviderFactory providers;
  interaction::InteractionOptions options;
  options.hides_focus_on_touch = true;
  Application application = start(providers, Column{}, options);
  EXPECT_TRUE(application.interaction().focus_visible());
  EXPECT_TRUE(shows(application, "controller"));
  tap(providers, application, centre(application, "b"));
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_FALSE(application.interaction().focus_visible());
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::touch);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "touch"));
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_TRUE(application.interaction().focus_visible());
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::controller);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c");
}

TEST_F(ModalityTest, PointerClicksMoveFocusAndSwitchModality) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Column{});
  const provider::PixelPoint target = centre(application, "c");
  provider::InputFrame hover;
  hover.pointer = provider::PointerState{.position = {.x = 1.0F, .y = 1.0F}};
  provider::InputFrame moved;
  moved.pointer = provider::PointerState{.position = target};
  provider::InputFrame down;
  down.pointer = provider::PointerState{.position = target, .left = true};
  provider::InputFrame up;
  up.pointer = provider::PointerState{.position = target, .left = false};
  providers.input().push(hover);
  providers.input().push(moved);
  providers.input().push(down);
  providers.input().push(up);
  frames(application, 6);
  EXPECT_EQ(focused_key(application), "c");
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::keyboard_and_pointer);
  EXPECT_TRUE(application.interaction().focus_visible());
  EXPECT_TRUE(shows(application, "keyboard"));
}

TEST_F(ModalityTest, KeyboardArrowsNavigate) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Column{});
  provider::InputFrame pressed;
  pressed.keys.push_back(
      provider::KeyEvent{.key = provider::Key::down, .down = true});
  provider::InputFrame released;
  released.keys.push_back(
      provider::KeyEvent{.key = provider::Key::down, .down = false});
  providers.input().push(pressed);
  providers.input().push(released);
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_EQ(application.interaction().modality(),
            composition::InputModality::keyboard_and_pointer);
}

TEST_F(ModalityTest, HeldDirectionRepeatsWithAcceleration) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Long{});
  EXPECT_EQ(focused_key(application), "i0");
  hold(providers, application, pad({Button::dpad_down}),
       std::chrono::milliseconds{380});
  EXPECT_EQ(focused_key(application), "i1");
  hold(providers, application, pad({Button::dpad_down}),
       std::chrono::milliseconds{1000});
  EXPECT_EQ(focused_key(application), "i12");
}

TEST_F(ModalityTest, StickNavigationHonoursDeadzoneAndHysteresis) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Long{});
  hold(providers, application, pad({}, provider::Stick{.x = 0.05F, .y = -0.1F}),
       std::chrono::milliseconds{200});
  EXPECT_EQ(focused_key(application), "i0");
  hold(providers, application, pad({}, provider::Stick{.x = 0.0F, .y = -0.9F}),
       std::chrono::milliseconds{100});
  EXPECT_EQ(focused_key(application), "i1");
  providers.input().push(pad({}, provider::Stick{.x = 0.0F, .y = -0.9F}));
  for (int index = 0; index < 5; ++index) {
    providers.input().push(pad({}, provider::Stick{.x = 0.66F, .y = -0.62F}));
  }
  providers.input().push(pad({}));
  frames(application, 8);
  const std::vector<provider::SoundRole> sounds = providers.audio().played();
  EXPECT_EQ(std::ranges::count(sounds, provider::SoundRole::focus_error), 0);
  EXPECT_EQ(focused_key(application), "i2");
}

}
