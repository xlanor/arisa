#include <arisa/async/frame_timeline.hpp>
#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/preferred_color_scheme.hpp>
#include <arisa/composition/root_services.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/markup/markup.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::markup {

using composition::Element;
using composition::infinity;
using theme::Font;

static_assert(std::same_as<decltype(Text("x")), components::Text>);
static_assert(std::same_as<decltype(VStack()), components::VStack>);
static_assert(std::invocable<decltype(VStack)&, const HorizontalAlign&>);
static_assert(!std::invocable<decltype(HStack)&, const HorizontalAlign&>);
static_assert(!std::invocable<decltype(VStack)&, const VerticalAlign&>);
static_assert(!std::invocable<decltype(VStack)&, Spacing, components::Text>);
static_assert(!std::invocable<decltype(VStack)&, components::Text, Spacing>);
static_assert(!std::invocable<decltype(Text)&, Spacing>);
static_assert(!std::invocable<decltype(Button)&, const FontOption&>);
static_assert(!std::invocable<decltype(ZStack)&, Spacing>);
static_assert(!std::invocable<decltype(Spacer)&, Columns>);

using TextBuilder = decltype(Text(font::title));

static_assert(!std::invocable<TextBuilder>);
static_assert(std::invocable<TextBuilder, const char (&)[6]>);
static_assert(!std::invocable<TextBuilder&, const char (&)[6]>);
static_assert(!std::copy_constructible<TextBuilder>);
static_assert(std::is_nothrow_move_constructible_v<TextBuilder>);
static_assert(std::is_nothrow_move_assignable_v<TextBuilder>);

template <class T>
concept PaddedDirectly = requires(T value) { std::move(value).padding(12); };

class MarkupTest : public ::testing::Test {
 protected:
  static constexpr composition::EnvironmentKey<int> level{"level", 0};

  class Shown final : public composition::Component {
   public:
    Shown(std::shared_ptr<composition::Observable<bool>> visible,
          std::shared_ptr<int> gone, bool tagged)
        : visible_(std::move(visible)),
          gone_(std::move(gone)),
          tagged_(tagged) {}

    auto operator==(const Shown&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      if (!context.read(*visible_)) {
        return components::Spacer();
      }
      auto leave = [gone = gone_] { ++*gone; };
      if (tagged_) {
        return Text(on_disappear(leave))("here");
      }
      return components::Text("here").on_disappear(leave);
    }

   private:
    std::shared_ptr<composition::Observable<bool>> visible_;
    std::shared_ptr<int> gone_;
    bool tagged_;
  };

  class Bare final : public composition::Component {
   public:
    auto operator==(const Bare&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto selected = context.state(chosen);
      return VStack(id("probe"), padding(12), focused(selected),
                    gesture(DragGesture(0.0F)))(Text("x"));
    }

   private:
    static constexpr composition::FocusState<bool> chosen{"bare-chosen"};
  };

  class Plain final : public composition::Component {
   public:
    auto operator==(const Plain&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return Text("plain");
    }
  };

  class Wrapper final : public composition::Component {
   public:
    auto operator==(const Wrapper&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return Plain{};
    }
  };

  template <class Content>
  static auto as(const composition::ElementContent& content) -> const Content& {
    return static_cast<const Content&>(content);
  }

  static auto same_content(const composition::ElementContent& tagged,
                           const composition::ElementContent& modified)
      -> bool {
    using composition::ElementType;
    const ElementType type = tagged.type();
    if (type != modified.type()) {
      return false;
    }
    if (type == ElementType::of<components::VStack::Content>()) {
      return as<components::VStack::Content>(tagged).options() ==
             as<components::VStack::Content>(modified).options();
    }
    if (type == ElementType::of<components::HStack::Content>()) {
      return as<components::HStack::Content>(tagged).options() ==
             as<components::HStack::Content>(modified).options();
    }
    if (type == ElementType::of<components::ZStack::Content>()) {
      return as<components::ZStack::Content>(tagged).alignment() ==
             as<components::ZStack::Content>(modified).alignment();
    }
    if (type == ElementType::of<components::Grid::Content>()) {
      return as<components::Grid::Content>(tagged).options() ==
             as<components::Grid::Content>(modified).options();
    }
    if (type == ElementType::of<components::GridRow::Content>()) {
      return as<components::GridRow::Content>(tagged).options() ==
             as<components::GridRow::Content>(modified).options();
    }
    if (type == ElementType::of<components::LazyGrid::Flow>()) {
      return as<components::LazyGrid::Flow>(tagged).paints_like(
          as<components::LazyGrid::Flow>(modified));
    }
    if (type == ElementType::of<components::Text::Content>()) {
      const auto& left = as<components::Text::Content>(tagged);
      const auto& right = as<components::Text::Content>(modified);
      return left.text() == right.text() && left.font() == right.font() &&
             left.weight() == right.weight();
    }
    if (type == ElementType::of<components::Button::Content>()) {
      const auto& left = as<components::Button::Content>(tagged);
      const auto& right = as<components::Button::Content>(modified);
      return left.label() == right.label() &&
             left.activatable() == right.activatable();
    }
    if (type == ElementType::of<components::Spacer::Content>()) {
      return as<components::Spacer::Content>(tagged).minimum_length() ==
             as<components::Spacer::Content>(modified).minimum_length();
    }
    return true;
  }

  static void expect_same(const Element& tagged, const Element& modified) {
    EXPECT_EQ(tagged.type(), modified.type());
    EXPECT_EQ(tagged.key(), modified.key());
    EXPECT_EQ(tagged.modifiers(), modified.modifiers());
    EXPECT_TRUE(same_content(tagged.content(), modified.content()));
    ASSERT_EQ(tagged.children().size(), modified.children().size());
    for (const auto& [left, right] :
         std::views::zip(tagged.children(), modified.children())) {
      expect_same(left, right);
    }
  }

  static auto owner() -> composition::Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto bump(std::shared_ptr<int> count) -> async::Task<void> {
    ++*count;
    co_return;
  }

  static auto level_of(Element element) -> std::optional<int> {
    composition::Tree tree{std::move(element), owner()};
    tree.reconcile();
    const auto found = tree.find("probe");
    if (!found.has_value()) {
      return std::nullopt;
    }
    const auto value = tree.node(*found)->get().override_of(level);
    if (!value.has_value()) {
      return std::nullopt;
    }
    return value->get();
  }

  static auto direction_of(Element element)
      -> std::optional<components::LayoutDirection> {
    composition::Tree tree{std::move(element), owner()};
    tree.reconcile();
    const auto found = tree.find("probe");
    if (!found.has_value()) {
      return std::nullopt;
    }
    const auto value = tree.node(*found)->get().override_of(
        components::Environment::layout_direction);
    if (!value.has_value()) {
      return std::nullopt;
    }
    return value->get();
  }

  auto services() -> composition::RootServices {
    return composition::RootServices{
        .root = manual_,
        .frames = timeline_,
        .executors =
            async::Executors{.background = manual_, .blocking = manual_}};
  }

  void start(Element element) {
    composition::Tree tree{std::move(element), owner(), services()};
    tree.reconcile();
  }

  std::shared_ptr<std::chrono::nanoseconds> now_{
      std::make_shared<std::chrono::nanoseconds>(0)};
  async::ManualExecutor manual_;
  async::FrameTimeline timeline_{[now = now_] { return *now; }};
};

TEST_F(MarkupTest, ElementFormIsUnchanged) {
  expect_same(VStack(components::Text("a"), components::Text("b")),
              components::VStack(components::Text("a"), components::Text("b")));
  expect_same(VStack({.spacing = 4}, Text("a")),
              components::VStack({.spacing = 4}, components::Text("a")));
  expect_same(Spacer(), components::Spacer());
  expect_same(Spacer(8.0F), components::Spacer(8.0F));
  expect_same(Text("x"), components::Text("x"));
  expect_same(Button("OK", [] {}), components::Button("OK", [] {}));
}

TEST_F(MarkupTest, StackAttributesBuildTheSameOptions) {
  expect_same(
      VStack(alignment::leading, spacing(10), justify::space_between, wrap)(),
      components::VStack(
          {.alignment = components::HorizontalAlignment::leading,
           .spacing = 10,
           .distribution = components::Distribution::space_between,
           .wrap = true}));
  expect_same(VStack(alignment::trailing)(),
              components::VStack(
                  {.alignment = components::HorizontalAlignment::trailing}));
  expect_same(VStack(alignment::center)(),
              components::VStack(
                  {.alignment = components::HorizontalAlignment::center}));
  expect_same(
      HStack(alignment::first_text_baseline, spacing(theme::space::md))(),
      components::HStack(
          {.alignment = components::VerticalAlignment::first_text_baseline,
           .spacing = theme::space::md}));
  expect_same(
      HStack(alignment::top, justify::end)(),
      components::HStack({.alignment = components::VerticalAlignment::top,
                          .distribution = components::Distribution::end}));
  expect_same(HStack(alignment::bottom, justify::space_evenly, wrap)(),
              components::HStack(
                  {.alignment = components::VerticalAlignment::bottom,
                   .distribution = components::Distribution::space_evenly,
                   .wrap = true}));
  expect_same(HStack(justify::center, justify::space_around)(),
              components::HStack(
                  {.distribution = components::Distribution::space_around}));
  expect_same(VStack(justify::start)(), components::VStack());
}

TEST_F(MarkupTest, ZStackAlignmentMatchesSwiftUiConstants) {
  expect_same(
      ZStack(alignment::top_trailing)(components::Text("a")),
      components::ZStack({.alignment = components::Alignment::top_trailing},
                         components::Text("a")));
  expect_same(
      ZStack(alignment::trailing, alignment::top)(),
      components::ZStack({.alignment = components::Alignment::top_trailing}));
  expect_same(
      ZStack(alignment::leading)(),
      components::ZStack({.alignment = components::Alignment::leading}));
  expect_same(ZStack(alignment::bottom)(),
              components::ZStack({.alignment = components::Alignment::bottom}));
  expect_same(
      ZStack(alignment::bottom_leading)(),
      components::ZStack({.alignment = components::Alignment::bottom_leading}));
  expect_same(ZStack(alignment::top_leading, alignment::center)(),
              components::ZStack());
}

TEST_F(MarkupTest, LazyGridColumnsAndSpacing) {
  expect_same(
      LazyVGrid(columns(3), spacing(theme::space::lg))(Text("a"), Text("b")),
      components::LazyVGrid(
          components::LazyVGridOptions{
              .columns = components::GridItem::repeated(3),
              .spacing = theme::space::lg},
          components::Text("a"), components::Text("b")));
  expect_same(
      LazyVGrid(columns(2, theme::space::xs), alignment::leading)(Text("a")),
      components::LazyVGrid(
          components::LazyVGridOptions{
              .columns = components::GridItem::repeated(
                  2,
                  components::GridItem{components::GridItem::Size::flexible(),
                                       theme::space::xs}),
              .alignment = components::HorizontalAlignment::leading},
          components::Text("a")));
  expect_same(LazyHGrid(rows(2), alignment::top)(Text("a")),
              components::LazyHGrid(
                  components::LazyHGridOptions{
                      .rows = components::GridItem::repeated(2),
                      .alignment = components::VerticalAlignment::top},
                  components::Text("a")));
}

TEST_F(MarkupTest, GridRowsAndSpacing) {
  expect_same(
      Grid(horizontal_spacing(4), vertical_spacing(6), alignment::top_leading)(
          GridRow(alignment::bottom)(Text("a")), Text("b")),
      components::Grid(
          components::GridOptions{
              .alignment = components::Alignment::top_leading,
              .horizontal_spacing = 4,
              .vertical_spacing = 6},
          components::GridRow(
              components::GridRowOptions{
                  .alignment = components::VerticalAlignment::bottom},
              components::Text("a")),
          components::Text("b")));
}

TEST_F(MarkupTest, TextFontAndWeight) {
  expect_same(
      Text(font::title2, font_weight(Font::Weight::bold))("x"),
      components::Text("x").font(Font::title2).font_weight(Font::Weight::bold));
  expect_same(Text(font::of(Font::caption2))("x"),
              components::Text("x").font(Font::caption2));
  expect_same(Text(font::subheadline)("x"),
              components::Text("x").font(Font::subheadline));
  expect_same(Text(font::footnote, font::large_title)("x"),
              components::Text("x").font(Font::large_title));
  expect_same(Text(opacity(0.5F))(std::string{"x"}),
              components::Text("x").opacity(0.5F));
}

TEST_F(MarkupTest, ButtonLabelsAndActions) {
  expect_same(Button(action([] {}))("OK"), components::Button("OK", [] {}));
  expect_same(Button(id("b"))("OK"),
              components::Button("OK", components::Button::Action{}).id("b"));
  expect_same(
      Button(action([] {}))(components::Text("OK").font(Font::headline)),
      components::Button([] {}, components::Text("OK").font(Font::headline)));
}

TEST_F(MarkupTest, PaddingForms) {
  expect_same(VStack(padding())(), components::VStack().padding());
  expect_same(VStack(padding(12))(), components::VStack().padding(12));
  expect_same(VStack(padding(theme::space::xl))(),
              components::VStack().padding(theme::space::xl));
  expect_same(
      VStack(padding(theme::EdgeInsets{.top = 1, .leading = 2}))(),
      components::VStack().padding(theme::EdgeInsets{.top = 1, .leading = 2}));
  expect_same(VStack(padding(theme::Edge::horizontal, theme::space::xxl),
                     padding(theme::Edge::vertical, theme::space::xl))(),
              components::VStack().padding(theme::Insets::symmetric(
                  theme::space::xxl, theme::space::xl)));
  expect_same(VStack(padding(8), padding(theme::Edge::top, 2))(),
              components::VStack().padding(8).padding(theme::Edge::top, 2));
  expect_same(VStack(padding(theme::Edge::top, 2), padding(8))(),
              components::VStack().padding(theme::Edge::top, 2).padding(8));
}

TEST_F(MarkupTest, MarginAndFlex) {
  expect_same(VStack(margin(theme::space::sm), flex_grow(2), flex_shrink(0))(),
              components::VStack()
                  .margin(theme::space::sm)
                  .flex_grow(2)
                  .flex_shrink(0));
  expect_same(VStack(margin(theme::Insets{.bottom = 4}))(),
              components::VStack().margin(theme::Insets{.bottom = 4}));
}

TEST_F(MarkupTest, FrameCarriesEveryField) {
  const composition::FrameSpec spec{
      .width = 120,
      .height = composition::Length::percent(0.5F),
      .min_width = 10,
      .ideal_width = 100,
      .max_width = infinity,
      .min_height = 4,
      .ideal_height = 30,
      .max_height = 200,
      .alignment = components::Alignment::top_leading};
  expect_same(Text(frame(spec))("x"), components::Text("x").frame(spec));
  expect_same(VStack(frame({.max_width = infinity}))(),
              components::VStack().frame({.max_width = infinity}));
}

TEST_F(MarkupTest, GeometryAttributes) {
  expect_same(VStack(fixed_size(), layout_priority(2.0), z_index(1.0),
                     container_relative_frame(Axes::horizontal, 3U, 1U,
                                              theme::space::md))(),
              components::VStack()
                  .fixed_size()
                  .layout_priority(2.0)
                  .z_index(1.0)
                  .container_relative_frame(Axes::horizontal, 3U, 1U,
                                            theme::space::md));
  expect_same(VStack(fixed_size(false, true))(),
              components::VStack().fixed_size(false, true));
  expect_same(VStack(position(10.0F, 20.0F))(),
              components::VStack().position(10.0F, 20.0F));
  const Element named = VStack(coordinate_space("panel"))();
  EXPECT_EQ(named.interactions().coordinate_space,
            composition::ElementKey{"panel"});
}

TEST_F(MarkupTest, DataFlowTagsAndAttributes) {
  const Element linked = Link("Open", "https://example.com");
  EXPECT_EQ(
      linked.type(),
      composition::Element{components::Link("Open", "https://example.com")}
          .type());
  composition::Tree tree{VStack(control_size(ControlSize::large))(Text("x")),
                         [] { return true; }};
  tree.reconcile();
  const auto top = tree.root().children();
  const composition::Node& stack = top.front();
  ASSERT_TRUE(
      stack.override_of(components::Environment::control_size).has_value());
  EXPECT_EQ(stack.override_of(components::Environment::control_size)->get(),
            ControlSize::large);
}

TEST_F(MarkupTest, AppearanceAttributes) {
  expect_same(
      VStack(foreground_style(theme::ColourRole::accent),
             background(theme::ColourRole::surface),
             border(theme::ColourRole::separator),
             shadow(theme::Elevation::high), opacity(0.5F), opacity(0.5F))(),
      components::VStack()
          .foreground_style(theme::ColourRole::accent)
          .background(theme::ColourRole::surface)
          .border(theme::ColourRole::separator)
          .shadow(theme::Elevation::high)
          .opacity(0.5F)
          .opacity(0.5F));
  expect_same(
      VStack(background(theme::ColourRole::accent, theme::Capsule()),
             clip_shape(theme::RoundedRectangle(
                 8, theme::RoundedCornerStyle::continuous)),
             border(theme::ColourRole::separator, theme::Stroke::hairline()),
             shadow(theme::Colour::hex(0x000000, 0.3F), 4, 1, 2))(),
      components::VStack()
          .background(theme::ColourRole::accent, theme::Capsule())
          .clip_shape(
              theme::RoundedRectangle(8, theme::RoundedCornerStyle::continuous))
          .border(theme::ColourRole::separator, theme::Stroke::hairline())
          .shadow(theme::Colour::hex(0x000000, 0.3F), 4, 1, 2));
  expect_same(VStack(clip_shape(theme::Circle()),
                     shadow(theme::ColourRole::scrim, 6))(),
              components::VStack()
                  .clip_shape(theme::Circle())
                  .shadow(theme::ColourRole::scrim, 6));
  expect_same(VStack(clip_shape(theme::Rectangle()))(),
              components::VStack().clip_shape(theme::Rectangle()));
}

TEST_F(MarkupTest, IdentityKeys) {
  expect_same(Text(id("name"))("x"), components::Text("x").id("name"));
  expect_same(Text(id(42))("x"), components::Text("x").id(42));
}

TEST_F(MarkupTest, EnvironmentOverridesMatch) {
  const auto tagged = level_of(Text(environment(level, 7), id("probe"))("x"));
  const auto modified =
      level_of(components::Text("x").environment(level, 7).id("probe"));
  EXPECT_EQ(tagged, std::optional<int>{7});
  EXPECT_EQ(tagged, modified);
  const auto flipped = direction_of(
      VStack(environment(components::Environment::layout_direction,
                         components::LayoutDirection::right_to_left),
             id("probe"))());
  EXPECT_EQ(flipped, std::optional{components::LayoutDirection::right_to_left});
}

TEST_F(MarkupTest, PreferredColorSchemeIsAPreferenceForThePresentation) {
  composition::Tree tree{
      VStack(preferred_color_scheme(theme::ColorScheme::dark), id("probe"))(),
      owner()};
  tree.reconcile();
  const auto found = tree.find("probe");
  ASSERT_TRUE(found.has_value());
  EXPECT_FALSE(tree.node(*found)
                   ->get()
                   .override_of(components::Environment::colour_scheme)
                   .has_value());
  EXPECT_EQ(tree.root().preference(composition::PreferredColorScheme::key),
            std::optional<std::optional<theme::ColorScheme>>{
                theme::ColorScheme::dark});
}

TEST_F(MarkupTest, AppearAndDisappearHooksRun) {
  for (const bool tagged : {true, false}) {
    auto appeared = std::make_shared<int>(0);
    composition::Tree mounted{
        tagged ? Element{Text(on_appear([appeared] { ++*appeared; }))("x")}
               : Element{components::Text("x").on_appear(
                     [appeared] { ++*appeared; })},
        owner()};
    mounted.reconcile();
    EXPECT_EQ(*appeared, 1);

    auto visible = std::make_shared<composition::Observable<bool>>(true);
    auto gone = std::make_shared<int>(0);
    composition::Tree tree{Shown{visible, gone, tagged}, owner()};
    tree.reconcile();
    EXPECT_EQ(*gone, 0);
    visible->set(false);
    tree.reconcile();
    EXPECT_EQ(*gone, 1);
  }
}

TEST_F(MarkupTest, BareAttributesApplyInsideComponentBodies) {
  static_assert(!PaddedDirectly<Bare>);
  composition::Tree tree{Bare{}, owner()};
  tree.reconcile();
  const auto found = tree.find("probe");
  ASSERT_TRUE(found.has_value());
  const composition::Node& node = tree.node(*found)->get();
  EXPECT_EQ(node.modifiers().padding,
            components::VStack().padding(12).modifiers().padding);
  EXPECT_EQ(node.interactions().focused.size(), 1U);
  EXPECT_EQ(node.interactions().gestures.size(), 1U);
}

TEST_F(MarkupTest, ModifiersApplyToAComponentElement) {
  const Element card = Element{Plain{}}.id("card").padding(16);
  EXPECT_EQ(
      card.type(),
      composition::ElementType::of<composition::ComponentContent<Plain>>());
  EXPECT_EQ(card.modifiers().padding,
            components::VStack().padding(16).modifiers().padding);

  composition::Tree tree{
      VStack(spacing(4))(Element{Plain{}}.id("card").padding(16), Plain{},
                         Wrapper{}),
      owner()};
  tree.reconcile();
  const auto found = tree.find("card");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(tree.node(*found)->get().modifiers().padding,
            components::VStack().padding(16).modifiers().padding);
}

TEST_F(MarkupTest, TasksStartLikeTheModifier) {
  auto count = std::make_shared<int>(0);
  start(Text(task([count] { return bump(count); }))("x"));
  EXPECT_EQ(*count, 1);
  start(components::Text("x").task([count] { return bump(count); }));
  EXPECT_EQ(*count, 2);
  start(Text(task(7, [count] { return bump(count); }))("x"));
  EXPECT_EQ(*count, 3);
  start(components::Text("x").task(7, [count] { return bump(count); }));
  EXPECT_EQ(*count, 4);
}

TEST_F(MarkupTest, BuilderMovesKeepTheirAttributes) {
  auto count = std::make_shared<int>(0);
  auto first = Button(action([count] { ++*count; }), id("pressed"));
  auto second = std::move(first);
  auto third = Button(action([] {}), id("other"));
  third = std::move(second);
  composition::Tree tree{std::move(third)("OK"), owner()};
  tree.reconcile();
  EXPECT_FALSE(tree.find("other").has_value());
  const auto found = tree.find("pressed");
  ASSERT_TRUE(found.has_value());
  EXPECT_TRUE(tree.activate(*found));
  EXPECT_EQ(*count, 1);
}

TEST_F(MarkupTest, MixedFormsBuildTheSameTree) {
  const Element tagged =
      VStack(alignment::leading, spacing(theme::space::md), padding())(
          components::Text("title").font(Font::title),
          HStack(spacing(4))(Text(font::caption)("a"), components::Spacer(),
                             Button(action([] {}))("b")),
          components::HStack({.spacing = 4}, Text("c"),
                             ZStack(alignment::bottom_leading)(
                                 components::Text("d").foreground_style(
                                     theme::ColourRole::accent))),
          LazyVGrid(columns(2))(Text(frame({.max_width = infinity}))("e")));
  const Element modified =
      components::VStack(
          {.alignment = components::HorizontalAlignment::leading,
           .spacing = theme::space::md},
          components::Text("title").font(Font::title),
          components::HStack(
              {.spacing = 4}, components::Text("a").font(Font::caption),
              components::Spacer(), components::Button("b", [] {})),
          components::HStack(
              {.spacing = 4}, components::Text("c"),
              components::ZStack(
                  {.alignment = components::Alignment::bottom_leading},
                  components::Text("d").foreground_style(
                      theme::ColourRole::accent))),
          components::LazyVGrid(
              components::LazyVGridOptions{
                  .columns = components::GridItem::repeated(2)},
              components::Text("e").frame({.max_width = infinity})))
          .padding();
  expect_same(tagged, modified);
}

}
