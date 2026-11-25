#include <layout_fixture.hpp>

#include <arisa/components/alignment.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::layout {

using components::HorizontalAlignment;
using components::VerticalAlignment;

class LayoutTest : public ::testing::Test, protected LayoutFixture {
 protected:
  class Counter final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"count", 0};

    auto operator==(const Counter&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto value = context.state(count);
      return components::VStack(
          components::Button("add", [value] { value.set(value.get() + 1); })
              .id("add"),
          box("grow", 10.0F + (10.0F * static_cast<float>(value.get())),
              10.0F));
    }
  };

  class Labelled final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> width{"width", 1};
    static constexpr composition::StateKey<int> name{"name", 0};

    auto operator==(const Labelled&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto wide = context.state(width);
      const auto renamed = context.state(name);
      return components::VStack(
          components::HStack(
              components::Button("widen", [wide] { wide.set(wide.get() + 1); })
                  .id("widen"),
              components::Button("rename",
                                 [renamed] { renamed.set(renamed.get() + 1); })
                  .id("rename")),
          components::Text(std::format("Label {}", renamed.get())).id("label"),
          components::VStack(components::Text("A static paragraph."))
              .frame({.width = 300.0F, .height = 40.0F}),
          box("bar", 10.0F * static_cast<float>(wide.get()), 10.0F));
    }
  };

  class Picker final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> chosen{"chosen", 0};
    static constexpr composition::StateKey<bool> large{"large", false};
    static constexpr composition::StateKey<bool> spread{"spread", false};
    static constexpr int rows = 40;
    static constexpr int paragraphs = 60;
    static constexpr float row_height = 24.0F;

    auto operator==(const Picker&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto selected = context.state(chosen);
      const auto big = context.state(large);
      const auto wide = context.state(spread);
      std::vector<composition::Element> listed;
      for (int index = 0; index < rows; ++index) {
        listed.push_back(
            components::HStack(components::Text(std::format("Row {}", index)),
                               components::Spacer(), components::Text("detail"))
                .frame({.height = row_height})
                .foreground_style(index == selected.get()
                                      ? theme::ColourRole::accent
                                      : theme::ColourRole::label)
                .id(composition::ElementKey{std::format("row {}", index)}));
      }
      std::vector<composition::Element> page;
      for (int index = 0; index < paragraphs; ++index) {
        page.push_back(
            components::Text(std::format("Paragraph {}", index))
                .id(composition::ElementKey{std::format("para {}", index)}));
      }
      composition::Element capsule =
          components::ZStack()
              .frame({.width = 200.0F, .height = row_height})
              .offset(0.0F, row_height * static_cast<float>(selected.get()))
              .id("capsule");
      return components::VStack(
                 components::HStack(
                     components::Button(
                         "next",
                         [selected] { selected.set(selected.get() + 1); })
                         .id("next"),
                     components::Button("grow", [big] { big.set(!big.get()); })
                         .id("grow"),
                     components::Button("spread",
                                        [wide] { wide.set(!wide.get()); })
                         .id("spread")),
                 components::HStack(
                     components::VStack(
                         components::VStackOptions{.spacing = 0.0F},
                         std::move(listed))
                         .background(composition::Alignment::top_leading,
                                     std::move(capsule))
                         .frame({.width = 240.0F}),
                     components::VStack(
                         components::VStackOptions{
                             .spacing =
                                 theme::Space{wide.get() ? 12.0F : 2.0F}},
                         std::move(page))
                         .id("page")),
                 components::VStack(components::Text("sized").id("sized"))
                     .font(big.get() ? theme::Font::large_title
                                     : theme::Font::body))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top});
    }
  };

  class Ledger final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> first{"first", 0};
    static constexpr int rows = 200;
    static constexpr int shift = 4;

    auto operator==(const Ledger&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto start = context.state(first);
      return components::HStack(
                 components::VStack(
                     components::Button(
                         "shift", [start] { start.set(start.get() + shift); })
                         .id("shift"),
                     components::ScrollView(components::LazyVStack(
                         components::LazyVStackOptions{.spacing = 0.0F},
                         components::ForEach(
                             std::views::iota(start.get(), start.get() + rows),
                             [](int index) {
                               return std::format("entry-{}", index);
                             },
                             [](int index) {
                               return components::HStack(
                                          components::Text(
                                              std::format("Entry {}", index)),
                                          components::Spacer(),
                                          components::Text("detail"))
                                   .frame({.max_width = composition::infinity});
                             }))))
                     .frame({.max_width = composition::infinity}),
                 components::VStack(components::Text("Side"))
                     .frame({.max_width = composition::infinity}))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity});
    }
  };

  static auto press(composition::Tree& target, std::string_view key) -> void {
    ASSERT_TRUE(target.activate(*target.find(composition::ElementKey{key})));
    target.reconcile();
  }

  static auto run(composition::Tree& target, Viewport viewport = handheld,
                  LayoutContext::TypesetterRef setter = std::nullopt)
      -> LayoutContext {
    auto context = LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    const auto ran = context->update(target, viewport, setter);
    EXPECT_TRUE(ran.has_value());
    EXPECT_TRUE(ran.value_or(false));
    return std::move(*context);
  }
};

TEST_F(LayoutTest, StacksApplyPaddingSpacingAndAlignment) {
  const auto layout =
      tree(components::VStack(
               components::VStackOptions{
                   .alignment = HorizontalAlignment::leading, .spacing = 10.0F},
               box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F))
               .padding(8.0F)
               .id("stack"));
  static_cast<void>(run(*layout));
  EXPECT_EQ(frame(*layout, "stack"),
            (composition::Rect{
                .x = 582.0F, .y = 317.0F, .width = 116.0F, .height = 86.0F}));
  EXPECT_EQ(frame(*layout, "a"),
            (composition::Rect{
                .x = 590.0F, .y = 325.0F, .width = 100.0F, .height = 40.0F}));
  EXPECT_EQ(frame(*layout, "b"),
            (composition::Rect{
                .x = 590.0F, .y = 375.0F, .width = 60.0F, .height = 20.0F}));
}

TEST_F(LayoutTest, ZeroHeightContentKeepsItsWidthInsideARow) {
  const auto layout = tree(components::VStack(
      components::HStack(
          components::VStack(box("thin", 20.0F, 0.0F)).id("column"),
          components::VStack(box("other", 30.0F, 0.0F)).id("second"))
          .id("row")));
  static_cast<void>(run(*layout));
  EXPECT_FLOAT_EQ(frame(*layout, "column").width, 20.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "second").width, 30.0F);
  EXPECT_LT(frame(*layout, "row").width, 100.0F);
}

TEST_F(LayoutTest, TrailingAlignmentAndTokenSpacing) {
  const auto layout = tree(
      components::VStack(
          components::VStackOptions{.alignment = HorizontalAlignment::trailing,
                                    .spacing = theme::space::lg},
          box("wide", 100.0F, 10.0F), box("narrow", 40.0F, 10.0F))
          .id("stack"));
  static_cast<void>(run(*layout));
  const composition::Rect wide = frame(*layout, "wide");
  const composition::Rect narrow = frame(*layout, "narrow");
  EXPECT_FLOAT_EQ(narrow.x + narrow.width, wide.x + wide.width);
  EXPECT_FLOAT_EQ(narrow.y - (wide.y + wide.height), 16.0F);
}

TEST_F(LayoutTest, SpacersAndFillWidthDistributeAlongTheRow) {
  const auto layout =
      tree(components::HStack(box("left", 50.0F, 50.0F), components::Spacer(),
                              box("right", 30.0F, 20.0F))
               .frame({.max_width = composition::infinity})
               .id("row"));
  static_cast<void>(run(*layout));
  const composition::Rect row = frame(*layout, "row");
  EXPECT_FLOAT_EQ(row.width, 1280.0F);
  EXPECT_FLOAT_EQ(row.height, 50.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "left").x, 0.0F);
  const composition::Rect right = frame(*layout, "right");
  EXPECT_FLOAT_EQ(right.x, 1250.0F);
  EXPECT_FLOAT_EQ(right.y, row.y + 15.0F);
}

TEST_F(LayoutTest, MinimumMaximumAndPercentSizing) {
  const auto layout =
      tree(components::VStack(
               components::VStack()
                   .frame({.min_width = 200.0F, .min_height = 12.0F})
                   .id("minimum"),
               components::VStack(box("inner", 300.0F, 10.0F))
                   .frame({.max_width = 120.0F})
                   .id("maximum"),
               components::HStack(
                   components::VStack()
                       .frame({.width = composition::Length::percent(0.25F),
                               .height = 10.0F})
                       .id("quarter"))
                   .frame({.max_width = composition::infinity}))
               .frame({.max_width = composition::infinity}));
  static_cast<void>(run(*layout));
  EXPECT_FLOAT_EQ(frame(*layout, "minimum").width, 200.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "minimum").height, 12.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "maximum").width, 120.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "quarter").width, 320.0F);
}

TEST_F(LayoutTest, ZStackOverlaysLaterChildrenOnTheFirst) {
  const auto layout =
      tree(components::ZStack(
               components::ZStackOptions{
                   .alignment = components::Alignment::top_trailing},
               box("base", 200.0F, 100.0F), box("badge", 40.0F, 20.0F))
               .id("stack"));
  static_cast<void>(run(*layout));
  const composition::Rect base = frame(*layout, "base");
  const composition::Rect badge = frame(*layout, "badge");
  EXPECT_EQ(frame(*layout, "stack"), base);
  EXPECT_FLOAT_EQ(badge.x, base.x + 160.0F);
  EXPECT_FLOAT_EQ(badge.y, base.y);
  EXPECT_FLOAT_EQ(badge.width, 40.0F);
}

TEST_F(LayoutTest, LazyGridsBuildEqualFlexibleColumnsInRows) {
  std::vector<composition::Element> items;
  for (int index = 0; index < 5; ++index) {
    items.push_back(box(std::format("item{}", index), 50.0F, 30.0F));
  }
  const auto layout =
      tree(components::LazyVGrid(
               components::LazyVGridOptions{
                   .columns = components::GridItem::repeated(
                       3,
                       components::GridItem{
                           components::GridItem::Size::flexible(), 10.0F}),
                   .spacing = 10.0F},
               std::move(items))
               .frame({.width = 320.0F})
               .id("grid"));
  static_cast<void>(run(*layout));
  const composition::Rect grid = frame(*layout, "grid");
  EXPECT_FLOAT_EQ(grid.width, 320.0F);
  EXPECT_FLOAT_EQ(grid.height, 70.0F);
  for (int index = 0; index < 5; ++index) {
    const composition::Rect item = frame(*layout, std::format("item{}", index));
    EXPECT_FLOAT_EQ(item.x,
                    grid.x + 25.0F + (110.0F * static_cast<float>(index % 3)))
        << index;
    EXPECT_FLOAT_EQ(item.y, grid.y + (40.0F * static_cast<float>(index / 3)))
        << index;
  }
}

TEST_F(LayoutTest, FramesAreInPointsAtEveryScale) {
  const auto layout = tree(box("a", 100.0F, 40.0F));
  static_cast<void>(run(*layout, docked()));
  EXPECT_EQ(frame(*layout, "a"),
            (composition::Rect{
                .x = 590.0F, .y = 340.0F, .width = 100.0F, .height = 40.0F}));
}

TEST_F(LayoutTest, TextIsMeasuredThroughTheTypesetter) {
  const std::string sentence = "The quick brown fox jumps over the lazy dog";
  const auto layout = tree(components::VStack(
      components::Text("Hello world").id("single"),
      components::Text(sentence).frame({.max_width = 120.0F}).id("capped"),
      components::VStack(components::Text(sentence)
                             .frame({.max_width = composition::infinity})
                             .id("filled"))
          .frame({.width = 150.0F})));
  text::Typesetter& setter = typesetter();
  static_cast<void>(run(*layout, handheld, setter));
  const text::TextStyle style = components::Text::text_style(
      components::Environment::theme.fallback(), theme::Font::body,
      text::Locale::undetermined());
  const auto single = setter.measure(
      "Hello world", style, text::LayoutWidth::unbounded(), handheld.scale);
  ASSERT_TRUE(single.has_value());
  EXPECT_FLOAT_EQ(frame(*layout, "single").width, single->width);
  EXPECT_FLOAT_EQ(frame(*layout, "single").height, single->height);
  const composition::Rect capped = frame(*layout, "capped");
  EXPECT_FLOAT_EQ(capped.width, 120.0F);
  const auto wrapped =
      setter.measure(sentence, style, text::LayoutWidth::points(120.0F).value(),
                     handheld.scale);
  EXPECT_FLOAT_EQ(capped.height, wrapped->height);
  EXPECT_GT(capped.height, single->height);
  const composition::Rect filled = frame(*layout, "filled");
  EXPECT_FLOAT_EQ(filled.width, 150.0F);
  const auto narrow =
      setter.measure(sentence, style, text::LayoutWidth::points(150.0F).value(),
                     handheld.scale);
  EXPECT_FLOAT_EQ(filled.height, narrow->height);
}

TEST_F(LayoutTest, LayoutRerunsOnlyWhenSomethingChanged) {
  const auto layout = tree(Counter{});
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  EXPECT_TRUE(context->update(*layout, handheld, std::nullopt).value());
  EXPECT_FALSE(context->update(*layout, handheld, std::nullopt).value());
  layout->reconcile();
  EXPECT_FALSE(context->update(*layout, handheld, std::nullopt).value());
  EXPECT_EQ(context->passes(), 1U);
  EXPECT_FLOAT_EQ(frame(*layout, "grow").width, 10.0F);
  ASSERT_TRUE(layout->activate(*layout->find(composition::ElementKey{"add"})));
  layout->reconcile();
  EXPECT_TRUE(context->update(*layout, handheld, std::nullopt).value());
  EXPECT_FLOAT_EQ(frame(*layout, "grow").width, 20.0F);
  EXPECT_TRUE(context->update(*layout, docked(), std::nullopt).value());
  EXPECT_FALSE(context->update(*layout, docked(), std::nullopt).value());
  context->invalidate();
  EXPECT_TRUE(context->update(*layout, docked(), std::nullopt).value());
  EXPECT_EQ(context->passes(), 4U);
}

TEST_F(LayoutTest, MarginsOffsetTheChildAndGrowTheParent) {
  const auto layout =
      tree(components::VStack(
               components::VStackOptions{
                   .alignment = HorizontalAlignment::leading, .spacing = 0.0F},
               box("a", 100.0F, 40.0F).margin(10.0F), box("b", 60.0F, 20.0F))
               .id("stack"));
  static_cast<void>(run(*layout));
  const composition::Rect stack = frame(*layout, "stack");
  const composition::Rect a = frame(*layout, "a");
  const composition::Rect b = frame(*layout, "b");
  EXPECT_FLOAT_EQ(stack.width, 120.0F);
  EXPECT_FLOAT_EQ(stack.height, 80.0F);
  EXPECT_FLOAT_EQ(a.x, stack.x + 10.0F);
  EXPECT_FLOAT_EQ(a.y, stack.y + 10.0F);
  EXPECT_FLOAT_EQ(b.x, stack.x);
  EXPECT_FLOAT_EQ(b.y, a.y + 50.0F);
}

TEST_F(LayoutTest, DistributionSpreadsChildrenAlongTheMainAxis) {
  const auto row = [](components::Distribution distribution,
                      std::string_view key) {
    return components::HStack(
               components::HStackOptions{.spacing = 0.0F,
                                         .distribution = distribution},
               box(std::format("{}0", key), 50.0F, 10.0F),
               box(std::format("{}1", key), 50.0F, 10.0F),
               box(std::format("{}2", key), 50.0F, 10.0F))
        .frame({.width = 300.0F})
        .id(composition::ElementKey{key});
  };
  const auto layout = tree(components::VStack(
      row(components::Distribution::space_between, "between"),
      row(components::Distribution::space_evenly, "evenly"),
      row(components::Distribution::space_around, "around"),
      row(components::Distribution::end, "end")));
  static_cast<void>(run(*layout));
  const auto offsets = [&](std::string_view key) {
    const float origin = frame(*layout, key).x;
    return std::array{frame(*layout, std::format("{}0", key)).x - origin,
                      frame(*layout, std::format("{}1", key)).x - origin,
                      frame(*layout, std::format("{}2", key)).x - origin};
  };
  EXPECT_EQ(offsets("between"), (std::array{0.0F, 125.0F, 250.0F}));
  EXPECT_EQ(offsets("evenly"), (std::array{37.5F, 125.0F, 212.5F}));
  EXPECT_EQ(offsets("around"), (std::array{25.0F, 125.0F, 225.0F}));
  EXPECT_EQ(offsets("end"), (std::array{150.0F, 200.0F, 250.0F}));
}

TEST_F(LayoutTest, GrowFactorsShareTheFreeSpace) {
  const auto layout =
      tree(components::HStack(components::HStackOptions{.spacing = 0.0F},
                              components::VStack()
                                  .frame({.height = 10.0F})
                                  .flex_grow(1.0F)
                                  .id("one"),
                              components::VStack()
                                  .frame({.height = 10.0F})
                                  .flex_grow(3.0F)
                                  .id("three"))
               .frame({.width = 400.0F}));
  static_cast<void>(run(*layout));
  EXPECT_FLOAT_EQ(frame(*layout, "one").width, 100.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "three").width, 300.0F);
  EXPECT_FLOAT_EQ(frame(*layout, "three").x, frame(*layout, "one").x + 100.0F);
}

TEST_F(LayoutTest, WrappingStacksBreakOntoNewLines) {
  std::vector<composition::Element> items;
  for (int index = 0; index < 5; ++index) {
    items.push_back(box(std::format("chip{}", index), 50.0F, 20.0F));
  }
  const auto layout =
      tree(components::HStack(
               components::HStackOptions{.alignment = VerticalAlignment::top,
                                         .spacing = 10.0F,
                                         .wrap = true},
               std::move(items))
               .frame({.width = 230.0F})
               .id("chips"));
  static_cast<void>(run(*layout));
  const composition::Rect chips = frame(*layout, "chips");
  EXPECT_FLOAT_EQ(chips.height, 50.0F);
  for (int index = 0; index < 4; ++index) {
    const composition::Rect chip = frame(*layout, std::format("chip{}", index));
    EXPECT_FLOAT_EQ(chip.x, chips.x + (60.0F * static_cast<float>(index)));
    EXPECT_FLOAT_EQ(chip.y, chips.y);
  }
  const composition::Rect last = frame(*layout, "chip4");
  EXPECT_FLOAT_EQ(last.x, chips.x);
  EXPECT_FLOAT_EQ(last.y, chips.y + 30.0F);
}

TEST_F(LayoutTest, FirstBaselineAlignsMixedTextSizes) {
  const auto layout = tree(components::HStack(
      components::HStackOptions{.alignment =
                                    VerticalAlignment::first_text_baseline},
      components::Text("Large").font(theme::Font::large_title).id("big"),
      components::Text("small").font(theme::Font::caption).id("small")));
  text::Typesetter& setter = typesetter();
  static_cast<void>(run(*layout, handheld, setter));
  const theme::Theme& fallback = components::Environment::theme.fallback();
  const auto baseline = [&](std::string_view content, theme::TextRole role) {
    const auto laid = setter.layout(
        content,
        components::Text::text_style(fallback, theme::Font{role},
                                     text::Locale::undetermined()),
        text::LayoutWidth::unbounded(), handheld.scale);
    EXPECT_TRUE(laid.has_value());
    return laid.has_value() ? (*laid)->first_baseline() : 0.0F;
  };
  const float big = baseline("Large", theme::TextRole::large_title);
  const float small = baseline("small", theme::TextRole::caption);
  ASSERT_GT(big, small);
  EXPECT_NEAR(frame(*layout, "big").y + big, frame(*layout, "small").y + small,
              0.01F);
  EXPECT_GT(frame(*layout, "small").y, frame(*layout, "big").y);
}

TEST_F(LayoutTest, RightToLeftDirectionMirrorsRowsAndInsets) {
  const auto layout =
      tree(components::HStack(components::HStackOptions{.spacing = 10.0F},
                              box("first", 50.0F, 10.0F),
                              box("second", 30.0F, 10.0F))
               .frame({.width = 300.0F,
                       .alignment = composition::Alignment::leading})
               .padding(theme::Insets{.leading = 20.0F})
               .environment(components::Environment::layout_direction,
                            components::LayoutDirection::right_to_left)
               .id("row"));
  static_cast<void>(run(*layout));
  const composition::Rect row = frame(*layout, "row");
  const composition::Rect first = frame(*layout, "first");
  const composition::Rect second = frame(*layout, "second");
  EXPECT_FLOAT_EQ(first.x + first.width, row.x + row.width - 20.0F);
  EXPECT_FLOAT_EQ(second.x + second.width, first.x - 10.0F);
}

TEST_F(LayoutTest, OnlyDirtySubtreesAreMeasuredAgain) {
  const auto layout = tree(Labelled{});
  text::Typesetter& setter = typesetter();
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  const std::uint64_t cold = context->measurements();
  EXPECT_GE(cold, 3U);
  ASSERT_TRUE(
      layout->activate(*layout->find(composition::ElementKey{"widen"})));
  layout->reconcile();
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  EXPECT_EQ(context->measurements(), cold);
  EXPECT_FLOAT_EQ(frame(*layout, "bar").width, 20.0F);
  ASSERT_TRUE(
      layout->activate(*layout->find(composition::ElementKey{"rename"})));
  layout->reconcile();
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  EXPECT_GT(context->measurements(), cold);
  EXPECT_LT(context->measurements() - cold, cold);
  EXPECT_EQ(context->passes(), 3U);
}

TEST_F(LayoutTest, SelectionMoveLaysOutOnlyTheRowsInvolved) {
  const auto layout = tree(Picker{});
  text::Typesetter& setter = typesetter();
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  const std::uint64_t cold = context->visits();
  EXPECT_GT(cold,
            static_cast<std::uint64_t>(Picker::rows + Picker::paragraphs));
  const std::uint64_t passes = context->passes();
  const std::uint64_t placed = context->placements();
  const composition::Rect row = frame(*layout, "row 1");
  press(*layout, "next");
  EXPECT_FALSE(context->update(*layout, handheld, setter).value());
  EXPECT_LE(context->visits() - cold, 8U);
  EXPECT_EQ(context->passes(), passes);
  EXPECT_EQ(context->placements(), placed);
  EXPECT_EQ(frame(*layout, "row 1"), row);
  const auto capsule = layout->node(*layout->find("capsule"));
  ASSERT_TRUE(capsule.has_value());
  EXPECT_FLOAT_EQ(capsule->get().modifiers().effects.offset_y,
                  Picker::row_height);
}

TEST_F(LayoutTest, SkippedSubtreesStillFollowInheritedAndStackChanges) {
  const auto layout = tree(Picker{});
  text::Typesetter& setter = typesetter();
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  const float small = frame(*layout, "sized").height;
  const float last = frame(*layout, "para 59").y;
  const std::uint64_t placed = context->placements();
  press(*layout, "grow");
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  EXPECT_GT(frame(*layout, "sized").height, small);
  EXPECT_FLOAT_EQ(frame(*layout, "para 59").y, last);
  EXPECT_LT(context->placements() - placed, 12U);
  press(*layout, "spread");
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  EXPECT_NEAR(frame(*layout, "para 59").y - last,
              10.0F * static_cast<float>(Picker::paragraphs - 1), 0.01F);
  press(*layout, "grow");
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  EXPECT_FLOAT_EQ(frame(*layout, "sized").height, small);
}

TEST_F(LayoutTest, ALazyStackStepDoesNotMeasureUnchangedRows) {
  const auto layout = tree(Ledger{});
  text::Typesetter& setter = typesetter();
  auto context = LayoutContext::create();
  ASSERT_TRUE(context.has_value());
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  const std::uint64_t cold = context->computations();
  const composition::Rect top = frame(*layout, "entry-0");
  const composition::Rect kept = frame(*layout, "entry-8");
  EXPECT_GT(top.height, 0.0F);
  EXPECT_GT(cold, 0U);
  press(*layout, "shift");
  ASSERT_TRUE(context->update(*layout, handheld, setter).value());
  const std::uint64_t step = context->computations() - cold;
  EXPECT_LT(step, cold / 2);
  const composition::Rect moved = frame(*layout, "entry-8");
  EXPECT_FLOAT_EQ(frame(*layout, "entry-4").y, top.y);
  EXPECT_FLOAT_EQ(moved.y, top.y + (4.0F * top.height));
  EXPECT_FLOAT_EQ(moved.width, kept.width);
  EXPECT_FLOAT_EQ(moved.x, kept.x);
  EXPECT_FLOAT_EQ(frame(*layout, "entry-19").y, top.y + (15.0F * top.height));
}

}
