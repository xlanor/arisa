#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/sdl3/sdl3_text_input.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

#include <optional>
#include <string>

namespace arisa::provider::sdl3 {

class Sdl3TextInputTest : public Sdl3Test {
 protected:
  static void type(const char* text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    ASSERT_TRUE(SDL_PushEvent(&event));
  }

  static void press(SDL_Keycode key) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    event.key.down = true;
    ASSERT_TRUE(SDL_PushEvent(&event));
  }

  standard::StdThreadingProvider threading_;
};

TEST_F(Sdl3TextInputTest, ReturnCommitsTheTypedText) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3TextInput text_input{*pump};
  auto session =
      text_input.begin(TextInputRequest{.initial = "ar", .hint = ""});
  ASSERT_TRUE(session.has_value());
  EXPECT_FALSE(session->poll().has_value());
  type("isax");
  press(SDLK_BACKSPACE);
  press(SDLK_RETURN);
  const auto outcome = session->poll();
  ASSERT_TRUE(outcome.has_value());
  ASSERT_TRUE(outcome->has_value());
  EXPECT_EQ(**outcome, "arisa");
}

TEST_F(Sdl3TextInputTest, TheDraftFollowsTheTypingBeforeReturn) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3TextInput text_input{*pump};
  auto session = text_input.begin(
      TextInputRequest{.initial = "st", .hint = "", .live = true});
  ASSERT_TRUE(session.has_value());
  EXPECT_EQ(session->draft(), std::optional<std::string>{"st"});
  type("ar");
  EXPECT_FALSE(session->poll().has_value());
  EXPECT_EQ(session->draft(), std::optional<std::string>{"star"});
  press(SDLK_BACKSPACE);
  EXPECT_FALSE(session->poll().has_value());
  EXPECT_EQ(session->draft(), std::optional<std::string>{"sta"});
}

TEST_F(Sdl3TextInputTest, EscapeCancels) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3TextInput text_input{*pump};
  auto session = text_input.begin(TextInputRequest{});
  ASSERT_TRUE(session.has_value());
  type("ignored");
  press(SDLK_ESCAPE);
  const auto outcome = session->poll();
  ASSERT_TRUE(outcome.has_value());
  ASSERT_FALSE(outcome->has_value());
  EXPECT_EQ(outcome->error().code(), ProviderErrorCode::cancelled);
}

TEST_F(Sdl3TextInputTest, OneSessionAtATimeAndLimitsAreHonoured) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3TextInput text_input{*pump};
  {
    auto session = text_input.begin(TextInputRequest{
        .initial = "", .hint = "", .max_length = *TextInputLimit::create(3)});
    ASSERT_TRUE(session.has_value());
    const auto second = text_input.begin(TextInputRequest{});
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code(), ProviderErrorCode::resource_exhausted);
    type("日本語です");
    press(SDLK_RETURN);
    const auto outcome = session->poll();
    ASSERT_TRUE(outcome.has_value() && outcome->has_value());
    EXPECT_EQ(**outcome, "日本語");
  }
  EXPECT_TRUE(text_input.begin(TextInputRequest{}).has_value());
}

TEST_F(Sdl3TextInputTest, CountsAndErasesCodePoints) {
  EXPECT_EQ(Sdl3TextInput::code_points("añ日"), 3U);
  std::string text = "a日";
  Sdl3TextInput::erase_last_code_point(text);
  EXPECT_EQ(text, "a");
}

TEST_F(Sdl3TextInputTest, MultilineReturnAddsALineAndKeypadEnterCommits) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3TextInput text_input{*pump};
  auto session = text_input.begin(
      TextInputRequest{.initial = "one",
                       .hint = "",
                       .traits = TextInputTraits{.multiline = true}});
  ASSERT_TRUE(session.has_value());
  press(SDLK_RETURN);
  EXPECT_FALSE(session->poll().has_value());
  type("two");
  press(SDLK_KP_ENTER);
  const auto outcome = session->poll();
  ASSERT_TRUE(outcome.has_value() && outcome->has_value());
  EXPECT_EQ(**outcome, "one\ntwo");
}

TEST_F(Sdl3TextInputTest, WithoutAWindowOnlyMultilineIsHonoured) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  const Sdl3TextInput text_input{*pump};
  const TextInputRequest request{
      .initial = "",
      .hint = "",
      .traits =
          TextInputTraits{.keyboard = KeyboardType::number_pad,
                          .content = TextContentType::username,
                          .capitalization = TextInputAutocapitalization::never,
                          .autocorrection_disabled = true,
                          .submit_label = SubmitLabel::search,
                          .multiline = true}};
  const TextInputTraitSet refused = text_input.unhonoured(request);
  EXPECT_TRUE(refused.contains(TextInputTrait::keyboard_type));
  EXPECT_TRUE(refused.contains(TextInputTrait::text_content_type));
  EXPECT_TRUE(refused.contains(TextInputTrait::autocapitalization));
  EXPECT_TRUE(refused.contains(TextInputTrait::autocorrection));
  EXPECT_TRUE(refused.contains(TextInputTrait::submit_label));
  EXPECT_FALSE(refused.contains(TextInputTrait::multiline));
  EXPECT_TRUE(text_input.unhonoured(TextInputRequest{}).empty());
}

}
