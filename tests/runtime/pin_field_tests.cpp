#include "focus_fixture.hpp"

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/structure/digit_keypad.hpp>
#include <arisa/structure/pin_field.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class PinFieldTest : public FocusFixture {
 protected:
  using Keypad = structure::DigitKeypad;
  using Pin = structure::PinField;
  using Button = provider::Button;

  struct Log {
    std::string code;
    std::vector<std::string> completed;
  };

  class Passcode final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::string> code_key{"pin-code",
                                                                 ""};

    Passcode(std::shared_ptr<Log> log, bool secure, bool completes) noexcept
        : log_(std::move(log)), secure_(secure), completes_(completes) {}

    auto operator==(const Passcode&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto code = context.state(code_key);
      log_->code = code.get();
      Pin::Options options;
      options.secure = secure_;
      if (completes_) {
        options.complete = [log = log_](const std::string& value) {
          log->completed.push_back(value);
        };
      }
      return Pin(4, composition::Binding<std::string>{code},
                 std::move(options));
    }

   private:
    std::shared_ptr<Log> log_;
    bool secure_;
    bool completes_;
  };

  auto type(Application& application, char digit) -> bool {
    const bool found =
        application.root().activate(composition::ElementKey{Keypad::key_of(digit)});
    frames(application, 2);
    return found;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(PinFieldTest, DigitsFillTheBoxesUpToTheLengthAndCompleteOnce) {
  auto log = std::make_shared<Log>();
  Application application = start(providers_, Passcode{log, false, true});
  EXPECT_TRUE(node_of(application, Pin::box_key(3)).has_value());
  EXPECT_FALSE(node_of(application, Pin::box_key(4)).has_value());
  ASSERT_TRUE(type(application, '1'));
  ASSERT_TRUE(type(application, '2'));
  ASSERT_TRUE(type(application, '3'));
  EXPECT_EQ(log->code, "123");
  EXPECT_TRUE(shows(application, "3"));
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{Keypad::key_of(Keypad::erase_name)}));
  frames(application, 2);
  EXPECT_EQ(log->code, "12");
  ASSERT_TRUE(type(application, '7'));
  ASSERT_TRUE(type(application, '9'));
  EXPECT_EQ(log->code, "1279");
  ASSERT_TRUE(type(application, '5'));
  EXPECT_EQ(log->code, "1279");
  ASSERT_EQ(log->completed.size(), 1U);
  EXPECT_EQ(log->completed.front(), "1279");
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{Keypad::key_of(Keypad::submit_name)}));
  frames(application, 2);
  EXPECT_EQ(log->completed.size(), 2U);
}

TEST_F(PinFieldTest, SecureBoxesMaskTheDigitsAndNoSubmitKeyWithoutCompletion) {
  auto log = std::make_shared<Log>();
  Application application = start(providers_, Passcode{log, true, false});
  ASSERT_TRUE(type(application, '8'));
  EXPECT_EQ(log->code, "8");
  EXPECT_EQ(std::ranges::count(texts(application), std::string{"8"}), 1);
  EXPECT_TRUE(shows(application, std::string{Pin::mask}));
  EXPECT_FALSE(
      node_of(application, Keypad::key_of(Keypad::submit_name)).has_value());
  EXPECT_TRUE(
      node_of(application, Keypad::key_of(Keypad::erase_name)).has_value());
}

TEST_F(PinFieldTest, TheKeypadTakesFocusAndTheDpadWalksItsGrid) {
  auto log = std::make_shared<Log>();
  Application application = start(providers_, Passcode{log, false, false});
  EXPECT_EQ(focused_key(application), Keypad::key_of('1'));
  press(providers_, application, Button::a);
  EXPECT_EQ(log->code, "1");
  press(providers_, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), Keypad::key_of('4'));
  press(providers_, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), Keypad::key_of('5'));
  press(providers_, application, Button::a);
  EXPECT_EQ(log->code, "15");
  for (int step = 0; step < 2; ++step) {
    press(providers_, application, Button::dpad_down);
  }
  EXPECT_EQ(focused_key(application), Keypad::key_of('0'));
}

TEST_F(PinFieldTest, EnteredAndErasedKeepTheCodeToDigitsAndLength) {
  EXPECT_EQ(Pin::entered("12", '3', 4), "123");
  EXPECT_EQ(Pin::entered("1234", '5', 4), "1234");
  EXPECT_EQ(Pin::entered("12", 'x', 4), "12");
  EXPECT_EQ(Pin::erased("12"), "1");
  EXPECT_EQ(Pin::erased(""), "");
}

}
