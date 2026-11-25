#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/markup/markup.hpp>

#include <gtest/gtest.h>

#include <concepts>
#include <optional>
#include <string>
#include <type_traits>

namespace arisa::markup {

using composition::Element;

static_assert(std::same_as<decltype(Divider()), components::Divider>);
static_assert(std::same_as<decltype(ProgressView()), controls::ProgressView>);
static_assert(std::same_as<decltype(Image("a.png")), components::Image>);
static_assert(std::invocable<decltype(Button)&, ButtonStyleAttribute>);
static_assert(std::invocable<decltype(Toggle)&, ToggleStyleAttribute>);
static_assert(std::invocable<decltype(Image)&, const Resizable&>);
static_assert(!std::invocable<decltype(Text)&, const Resizable&>);

class ControlsMarkupTest : public ::testing::Test {
 protected:
  struct Bordered {
    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> Element {
      return std::move(configuration.label);
    }

    friend auto operator==(const Bordered&, const Bordered&) -> bool = default;
  };

  class Screen final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> flag{"flag", false};
    static constexpr composition::StateKey<int> choice{"choice", 1};
    static const composition::StateKey<std::string> words;

    auto operator==(const Screen&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto on = context.state(flag);
      const auto picked = context.state(choice);
      const auto typed = context.state(words);
      return VStack(button_style(Bordered{}), id("stack"))(
          Toggle(toggle_style(ToggleStyle::checkbox()), id("toggle"))("Wi-Fi",
                                                                      on),
          Picker(picker_style(PickerStyle::segmented()), id("picker"))(
              "Choice", picked,
              [] { return Group(Text(tag(1))("One"), Text(tag(2))("Two")); }),
          ProgressView(progress_view_style(ProgressViewStyle::linear()),
                       id("progress"))(0.5),
          TextField(text_field_style(TextFieldStyle::plain()), id("field"))(
              "Name", typed),
          Label(label_style(LabelStyle::icon_only()), id("label"))(
              "Title", Text("icon")),
          Image(resizable, id("image"))("a.png"));
    }
  };

  static auto owner() -> composition::Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto node(composition::Tree& tree, std::string_view key)
      -> const composition::Node& {
    return tree.node(*tree.find(composition::ElementKey{key}))->get();
  }
};

const composition::StateKey<std::string> ControlsMarkupTest::Screen::words{
    "words", std::string{}};

TEST_F(ControlsMarkupTest, StyleAttributesWriteTheEnvironment) {
  composition::Tree tree{Screen{}, owner()};
  tree.reconcile();
  EXPECT_TRUE(node(tree, "stack")
                  .override_of(components::ButtonStyle::key)
                  .has_value());
  EXPECT_EQ(node(tree, "toggle").override_of(controls::ToggleStyle::key)->get(),
            controls::ToggleStyle::checkbox());
  EXPECT_EQ(node(tree, "picker").override_of(controls::PickerStyle::key)->get(),
            controls::PickerStyle::segmented());
  EXPECT_EQ(node(tree, "progress")
                .override_of(controls::ProgressViewStyle::key)
                ->get(),
            controls::ProgressViewStyle::linear());
  EXPECT_EQ(
      node(tree, "field").override_of(controls::TextFieldStyle::key)->get(),
      controls::TextFieldStyle::plain());
  EXPECT_EQ(node(tree, "label").override_of(components::LabelStyle::key)->get(),
            components::LabelStyle::icon_only());
  EXPECT_TRUE(node(tree, "image")
                  .content_as<components::Image::Content>()
                  ->get()
                  .resizable());
}

TEST_F(ControlsMarkupTest, TextAttributesWriteTheEnvironmentAndAspect) {
  class Texts final : public composition::Component {
   public:
    auto operator==(const Texts&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return VStack(attr::scaled_to_fit, id("fitted"))(Text(
          attr::line_limit(2, true),
          attr::truncation_mode(composition::TruncationMode::middle),
          attr::minimum_scale_factor(0.5F), attr::line_spacing(4.0F),
          attr::allows_tightening(true),
          attr::text_case(composition::TextCase::uppercase),
          attr::kerning(1.0F), attr::tracking(2.0F),
          attr::baseline_offset(3.0F), attr::monospaced_digit(true),
          attr::multiline_text_alignment(composition::TextAlignment::center),
          attr::aspect_ratio(2.0F, composition::ContentMode::fill),
          id("text"))("Text"));
    }
  };
  composition::Tree tree{Texts{}, owner()};
  tree.reconcile();
  const composition::Node& text = node(tree, "text");
  using composition::TextEnvironment;
  EXPECT_EQ(text.override_of(TextEnvironment::line_limit)->get(),
            (composition::LineLimit{.lines = 2, .reserves_space = true}));
  EXPECT_EQ(text.override_of(TextEnvironment::truncation_mode)->get(),
            composition::TruncationMode::middle);
  EXPECT_FLOAT_EQ(
      text.override_of(TextEnvironment::minimum_scale_factor)->get(), 0.5F);
  EXPECT_FLOAT_EQ(text.override_of(TextEnvironment::line_spacing)->get(), 4.0F);
  EXPECT_TRUE(text.override_of(TextEnvironment::allows_tightening)->get());
  EXPECT_EQ(text.override_of(TextEnvironment::text_case)->get(),
            std::optional{composition::TextCase::uppercase});
  EXPECT_FLOAT_EQ(text.override_of(TextEnvironment::kerning)->get(), 1.0F);
  EXPECT_FLOAT_EQ(text.override_of(TextEnvironment::tracking)->get(), 2.0F);
  EXPECT_FLOAT_EQ(text.override_of(TextEnvironment::baseline_offset)->get(),
                  3.0F);
  EXPECT_TRUE(text.override_of(TextEnvironment::monospaced_digit)->get());
  EXPECT_EQ(text.override_of(TextEnvironment::multiline_text_alignment)->get(),
            composition::TextAlignment::center);
  EXPECT_EQ(text.modifiers().aspect,
            (composition::AspectRatio{.ratio = 2.0F,
                                      .mode = composition::ContentMode::fill}));
  EXPECT_EQ(node(tree, "fitted").modifiers().aspect,
            (composition::AspectRatio{.ratio = std::nullopt,
                                      .mode = composition::ContentMode::fit}));
}

}
