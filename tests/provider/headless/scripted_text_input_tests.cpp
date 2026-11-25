#include <arisa/provider/headless/scripted_text_input.hpp>

#include <gtest/gtest.h>

#include <optional>

namespace arisa::provider::headless {

TEST(ScriptedTextInputTest, RepliesAfterTheScriptedPolls) {
  ScriptedTextInput text_input;
  text_input.push(std::string{"arisa"}, 2);
  auto session =
      text_input.begin(TextInputRequest{.initial = "", .hint = "name"});
  ASSERT_TRUE(session.has_value());
  EXPECT_FALSE(session->poll().has_value());
  EXPECT_FALSE(session->poll().has_value());
  const auto outcome = session->poll();
  ASSERT_TRUE(outcome.has_value() && outcome->has_value());
  EXPECT_EQ(**outcome, "arisa");
  ASSERT_EQ(text_input.requests().size(), 1U);
  EXPECT_EQ(text_input.requests().front().hint, "name");
}

TEST(ScriptedTextInputTest, ScriptsCancellation) {
  ScriptedTextInput text_input;
  text_input.push(std::unexpected(ProviderError{ProviderErrorCode::cancelled}));
  auto session = text_input.begin(TextInputRequest{});
  ASSERT_TRUE(session.has_value());
  const auto outcome = session->poll();
  ASSERT_TRUE(outcome.has_value());
  EXPECT_EQ(outcome->error().code(), ProviderErrorCode::cancelled);
}

TEST(ScriptedTextInputTest, DestroyingAnUnfinishedSessionCancelsIt) {
  ScriptedTextInput text_input;
  {
    auto session = text_input.begin(TextInputRequest{});
    ASSERT_TRUE(session.has_value());
    EXPECT_FALSE(session->poll().has_value());
    EXPECT_EQ(text_input.live_sessions(), 1U);
  }
  EXPECT_EQ(text_input.live_sessions(), 0U);
  EXPECT_EQ(text_input.cancelled_sessions(), 1U);
}

}
