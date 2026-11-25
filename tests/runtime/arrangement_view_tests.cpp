#include <arisa/components/arrangement_view.hpp>
#include <arisa/components/arrangement_view_style.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/overlay_arrangement_edges.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/split_arrangement_sizing.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/vertical_edge.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/theme/space.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

using components::ArrangementView;
using components::ArrangementViewStyle;
using components::OverlayArrangementViewStyle;
using components::SplitArrangementViewStyle;
using composition::Axis;
using composition::Element;
using composition::infinity;
using composition::Rect;
using composition::SplitArrangementSizing;

class ArrangementViewTest : public ScrollFixture {
 protected:
  static constexpr float tolerance = 0.01F;

  struct Reading {
    std::optional<Axis> axis;
    int z_index{-1};

    friend auto operator==(const Reading&, const Reading&) -> bool = default;
  };

  using Readings = std::map<std::string, Reading>;

  class Part final : public composition::Component {
   public:
    Part(std::string name, std::shared_ptr<Readings> readings,
         composition::FrameSpec frame = {.max_width = infinity,
                                         .max_height = infinity})
        : name_(std::move(name)),
          readings_(std::move(readings)),
          frame_(frame) {}

    auto operator==(const Part& other) const -> bool {
      return name_ == other.name_ && readings_ == other.readings_ &&
             frame_ == other.frame_;
    }

    auto body(composition::Context& context) const -> Element override {
      (*readings_)[name_] =
          Reading{.axis = context.environment(
                      components::Environment::split_arrangement_axis),
                  .z_index = context.environment(
                      components::Environment::overlay_arrangement_z_index)};
      return components::ZStack().frame(frame_);
    }

   private:
    std::string name_;
    std::shared_ptr<Readings> readings_;
    composition::FrameSpec frame_;
  };

  class Stacked {
   public:
    auto make_body(ArrangementViewStyle::Configuration configuration) const
        -> Element {
      return components::VStack(
                 components::VStackOptions{.spacing = theme::space::none},
                 std::move(configuration.primary),
                 std::move(configuration.secondary))
          .id("stacked");
    }

    friend auto operator==(const Stacked&, const Stacked&) -> bool = default;
  };

  class Forwarding {
   public:
    auto make_body(ArrangementViewStyle::Configuration configuration) const
        -> Element {
      return ArrangementView(std::move(configuration)).id("forwarded");
    }

    friend auto operator==(const Forwarding&, const Forwarding&)
        -> bool = default;
  };

  class ForwardingToSplit {
   public:
    auto make_body(ArrangementViewStyle::Configuration configuration) const
        -> Element {
      return ArrangementView(std::move(configuration))
          .arrangement_view_style(
              ArrangementViewStyle::split().axes(composition::Axes::vertical));
    }

    friend auto operator==(const ForwardingToSplit&, const ForwardingToSplit&)
        -> bool = default;
  };

  class Stepped final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> step{"arrangement-step", 0};
    static constexpr std::array<float, 3> ratios{0.5F, 0.25F, 0.75F};

    explicit Stepped(std::shared_ptr<Readings> readings)
        : readings_(std::move(readings)) {}

    auto operator==(const Stepped& other) const -> bool {
      return readings_ == other.readings_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto stepped = context.state(step);
      const float chosen =
          ratios[static_cast<std::size_t>(stepped.get()) % ratios.size()];
      return components::VStack(
          components::Button("Next",
                             [stepped] { stepped.set(stepped.get() + 1); })
              .id("next"),
          ArrangementView(part(readings_, "primary"),
                          part(readings_, "secondary")
                              .split_arrangement_layout_ratio(chosen))
              .arrangement_view_style(ArrangementViewStyle::split())
              .frame({.width = 400.0F, .height = 200.0F})
              .id("box"));
    }

   private:
    std::shared_ptr<Readings> readings_;
  };

  static auto boxed(Element arrangement, float width, float height) -> Element {
    return components::ZStack(std::move(arrangement)
                                  .frame({.width = width, .height = height})
                                  .id("box"))
        .frame({.max_width = infinity,
                .max_height = infinity,
                .alignment = composition::Alignment::top_leading});
  }

  static auto part(const std::shared_ptr<Readings>& readings, std::string name)
      -> Element {
    return Element{Part(name, readings)}.id(std::move(name));
  }

  static auto sized(const std::shared_ptr<Readings>& readings, std::string name)
      -> Element {
    return Element{Part(name, readings,
                        {.min_width = 80.0F,
                         .max_width = infinity,
                         .min_height = 50.0F,
                         .max_height = infinity})}
        .id(std::move(name));
  }

  static auto relative(Application& application, std::string_view key) -> Rect {
    const Rect box = frame_of(application, "box");
    const Rect frame = frame_of(application, key);
    return Rect{.x = frame.x - box.x,
                .y = frame.y - box.y,
                .width = frame.width,
                .height = frame.height};
  }

  static void expect_rect(const Rect& actual, const Rect& expected) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.width, expected.width, tolerance);
    EXPECT_NEAR(actual.height, expected.height, tolerance);
  }

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     Element arrangement, float width, float height)
      -> Application {
    Application application =
        start(providers, boxed(std::move(arrangement), width, height));
    frames(application, 2);
    return application;
  }
};

TEST_F(ArrangementViewTest, SplitPlacesTheSecondaryBesideThePrimaryWhenWide) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = launch(
      providers,
      ArrangementView(part(readings, "primary"), part(readings, "secondary"))
          .arrangement_view_style(ArrangementViewStyle::split()),
      400.0F, 200.0F);
  expect_rect(relative(application, "primary"), {0.0F, 0.0F, 200.0F, 200.0F});
  expect_rect(relative(application, "secondary"),
              {200.0F, 0.0F, 200.0F, 200.0F});
  EXPECT_EQ(readings->at("primary"),
            (Reading{.axis = Axis::horizontal, .z_index = 0}));
  EXPECT_EQ(readings->at("secondary"),
            (Reading{.axis = Axis::horizontal, .z_index = 0}));
}

TEST_F(ArrangementViewTest, SplitStacksTheSecondaryBelowWhenTall) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = launch(
      providers,
      ArrangementView(part(readings, "primary"), part(readings, "secondary"))
          .arrangement_view_style(ArrangementViewStyle::split()),
      200.0F, 400.0F);
  expect_rect(relative(application, "primary"), {0.0F, 0.0F, 200.0F, 200.0F});
  expect_rect(relative(application, "secondary"),
              {0.0F, 200.0F, 200.0F, 200.0F});
  EXPECT_EQ(readings->at("secondary").axis, Axis::vertical);
}

TEST_F(ArrangementViewTest, SplitAxesPinTheArrangedAxis) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = launch(
      providers,
      ArrangementView(part(readings, "primary"), part(readings, "secondary"))
          .arrangement_view_style(
              SplitArrangementViewStyle().axes(composition::Axes::horizontal)),
      200.0F, 400.0F);
  expect_rect(relative(application, "secondary"),
              {100.0F, 0.0F, 100.0F, 400.0F});
  EXPECT_EQ(readings->at("secondary").axis, Axis::horizontal);
  EXPECT_EQ(
      ArrangementViewStyle::arranged_axis(composition::Axes::vertical,
                                          {.width = 400.0F, .height = 100.0F}),
      Axis::vertical);
  EXPECT_EQ(ArrangementViewStyle::arranged_axis(
                composition::Axes{}, {.width = 400.0F, .height = 100.0F}),
            Axis::horizontal);
}

TEST_F(ArrangementViewTest, SplitRightToLeftPutsThePrimaryOnTheRight) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = launch(
      providers,
      ArrangementView(
          part(readings, "primary").split_arrangement_layout_ratio(0.25F),
          part(readings, "secondary"))
          .arrangement_view_style(ArrangementViewStyle::split())
          .environment(components::Environment::layout_direction,
                       components::LayoutDirection::right_to_left),
      400.0F, 200.0F);
  expect_rect(relative(application, "primary"), {300.0F, 0.0F, 100.0F, 200.0F});
  expect_rect(relative(application, "secondary"), {0.0F, 0.0F, 300.0F, 200.0F});
}

TEST_F(ArrangementViewTest, SplitLayoutRatioSizesEitherSide) {
  const auto measure = [](Element primary, Element secondary, float width,
                          float height) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        launch(providers,
               ArrangementView(std::move(primary), std::move(secondary))
                   .arrangement_view_style(ArrangementViewStyle::split()),
               width, height);
    return relative(application, "secondary");
  };
  auto readings = std::make_shared<Readings>();
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary").split_arrangement_layout_ratio(0.25F),
              400.0F, 200.0F),
      {300.0F, 0.0F, 100.0F, 200.0F});
  expect_rect(
      measure(part(readings, "primary").split_arrangement_layout_ratio(0.25F),
              part(readings, "secondary"), 400.0F, 200.0F),
      {100.0F, 0.0F, 300.0F, 200.0F});
  expect_rect(
      measure(part(readings, "primary").split_arrangement_layout_ratio(0.25F),
              part(readings, "secondary").split_arrangement_layout_ratio(0.25F),
              400.0F, 200.0F),
      {300.0F, 0.0F, 100.0F, 200.0F});
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary").split_arrangement_layout_ratio(0.25F),
              200.0F, 400.0F),
      {0.0F, 300.0F, 200.0F, 100.0F});
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary")
                  .split_arrangement_layout_ratio(
                      SplitArrangementSizing::Ratios{.min_horizontal = 0.7F}),
              400.0F, 200.0F),
      {120.0F, 0.0F, 280.0F, 200.0F});
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary")
                  .split_arrangement_layout_ratio(
                      SplitArrangementSizing::Ratios{.max_horizontal = 0.2F}),
              400.0F, 200.0F),
      {320.0F, 0.0F, 80.0F, 200.0F});
  expect_rect(
      measure(
          part(readings, "primary"),
          part(readings, "secondary")
              .split_arrangement_layout_ratio(SplitArrangementSizing::Ratios{
                  .ideal_horizontal = 0.3F, .ideal_vertical = 0.1F}),
          200.0F, 400.0F),
      {0.0F, 360.0F, 200.0F, 40.0F});
}

TEST_F(ArrangementViewTest, SplitLayoutSizeBoundsTheSides) {
  const auto measure = [](Element primary, Element secondary, float width,
                          float height) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        launch(providers,
               ArrangementView(std::move(primary), std::move(secondary))
                   .arrangement_view_style(ArrangementViewStyle::split()),
               width, height);
    return relative(application, "secondary");
  };
  auto readings = std::make_shared<Readings>();
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary")
                  .split_arrangement_layout_size({.ideal_width = 120.0F}),
              400.0F, 200.0F),
      {280.0F, 0.0F, 120.0F, 200.0F});
  expect_rect(measure(part(readings, "primary"),
                      part(readings, "secondary")
                          .split_arrangement_layout_size({.min_width = 300.0F}),
                      400.0F, 200.0F),
              {100.0F, 0.0F, 300.0F, 200.0F});
  expect_rect(measure(part(readings, "primary"),
                      part(readings, "secondary")
                          .split_arrangement_layout_size({.max_width = 50.0F}),
                      400.0F, 200.0F),
              {350.0F, 0.0F, 50.0F, 200.0F});
  expect_rect(measure(part(readings, "primary")
                          .split_arrangement_layout_size({.min_width = 300.0F}),
                      part(readings, "secondary"), 400.0F, 200.0F),
              {300.0F, 0.0F, 100.0F, 200.0F});
  expect_rect(
      measure(part(readings, "primary"),
              part(readings, "secondary")
                  .split_arrangement_layout_size({.ideal_height = 90.0F}),
              200.0F, 400.0F),
      {0.0F, 310.0F, 200.0F, 90.0F});
}

TEST_F(ArrangementViewTest, SplitFixedLayoutSizeKeepsTheIdealLength) {
  const auto measure = [](Element secondary) {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application =
        launch(providers,
               ArrangementView(part(readings, "primary"), std::move(secondary))
                   .arrangement_view_style(ArrangementViewStyle::split()),
               400.0F, 200.0F);
    return relative(application, "secondary");
  };
  auto readings = std::make_shared<Readings>();
  expect_rect(measure(sized(readings, "secondary")),
              {200.0F, 0.0F, 200.0F, 200.0F});
  expect_rect(
      measure(
          sized(readings, "secondary").split_arrangement_fixed_layout_size()),
      {320.0F, 0.0F, 80.0F, 200.0F});
  expect_rect(measure(sized(readings, "secondary")
                          .split_arrangement_fixed_layout_size(false, true)),
              {200.0F, 0.0F, 200.0F, 200.0F});
}

TEST_F(ArrangementViewTest, OverlayFloatsTheSecondaryAtTheArrangedEdge) {
  {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), sized(readings, "secondary"))
            .arrangement_view_style(ArrangementViewStyle::overlay()),
        400.0F, 200.0F);
    expect_rect(relative(application, "primary"), {0.0F, 0.0F, 400.0F, 200.0F});
    expect_rect(relative(application, "secondary"),
                {320.0F, 0.0F, 80.0F, 200.0F});
    EXPECT_EQ(readings->at("primary"),
              (Reading{.axis = std::nullopt, .z_index = 0}));
    EXPECT_EQ(readings->at("secondary"),
              (Reading{.axis = std::nullopt, .z_index = 1}));
  }
  {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), sized(readings, "secondary"))
            .arrangement_view_style(ArrangementViewStyle::overlay()),
        200.0F, 400.0F);
    expect_rect(relative(application, "secondary"),
                {0.0F, 350.0F, 200.0F, 50.0F});
  }
  {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), sized(readings, "secondary"))
            .arrangement_view_style(OverlayArrangementViewStyle().axes(
                composition::Axes::vertical)),
        400.0F, 200.0F);
    expect_rect(relative(application, "secondary"),
                {0.0F, 150.0F, 400.0F, 50.0F});
  }
}

TEST_F(ArrangementViewTest, OverlayArrangementEdgeChoosesTheSide) {
  const auto measure = [](Element secondary, float width, float height,
                          components::LayoutDirection direction) {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), std::move(secondary))
            .arrangement_view_style(ArrangementViewStyle::overlay())
            .environment(components::Environment::layout_direction, direction),
        width, height);
    return relative(application, "secondary");
  };
  auto readings = std::make_shared<Readings>();
  const auto ltr = components::LayoutDirection::left_to_right;
  expect_rect(
      measure(sized(readings, "secondary")
                  .overlay_arrangement_edge(composition::VerticalEdge::top),
              200.0F, 400.0F, ltr),
      {0.0F, 0.0F, 200.0F, 50.0F});
  expect_rect(measure(sized(readings, "secondary")
                          .overlay_arrangement_edge(
                              composition::HorizontalEdge::leading),
                      400.0F, 200.0F, ltr),
              {0.0F, 0.0F, 80.0F, 200.0F});
  expect_rect(
      measure(
          sized(readings, "secondary")
              .overlay_arrangement_edge(composition::HorizontalEdge::leading),
          400.0F, 200.0F, components::LayoutDirection::right_to_left),
      {320.0F, 0.0F, 80.0F, 200.0F});
  expect_rect(
      measure(sized(readings, "secondary")
                  .overlay_arrangement_edge(composition::VerticalEdge::top),
              400.0F, 200.0F, ltr),
      {320.0F, 0.0F, 80.0F, 200.0F});
}

TEST_F(ArrangementViewTest, AutomaticSplitsWhenRegularAndOverlaysWhenCompact) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().set_display_mode(provider::DisplayMode::docked);
  auto readings = std::make_shared<Readings>();
  Application application = launch(
      providers,
      ArrangementView(part(readings, "primary"), sized(readings, "secondary")),
      400.0F, 200.0F);
  expect_rect(relative(application, "secondary"),
              {200.0F, 0.0F, 200.0F, 200.0F});
  EXPECT_EQ(readings->at("secondary").axis, Axis::horizontal);
  providers.surface().set_display_mode(provider::DisplayMode::handheld);
  frames(application, 3);
  expect_rect(relative(application, "secondary"),
              {320.0F, 0.0F, 80.0F, 200.0F});
  EXPECT_EQ(readings->at("secondary"),
            (Reading{.axis = std::nullopt, .z_index = 1}));
}

TEST_F(ArrangementViewTest, CustomStylesReceiveTheConfiguration) {
  {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application =
        launch(providers,
               ArrangementView(sized(readings, "primary"),
                               sized(readings, "secondary"))
                   .arrangement_view_style(Stacked{}),
               400.0F, 200.0F);
    EXPECT_TRUE(node_of(application, "stacked").has_value());
    const Rect primary = relative(application, "primary");
    const Rect secondary = relative(application, "secondary");
    EXPECT_NEAR(secondary.y, primary.y + primary.height, tolerance);
    EXPECT_EQ(readings->at("secondary").axis, std::nullopt);
  }
  {
    provider::headless::HeadlessProviderFactory providers;
    providers.surface().set_display_mode(provider::DisplayMode::docked);
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), part(readings, "secondary"))
            .arrangement_view_style(Forwarding{}),
        400.0F, 200.0F);
    EXPECT_TRUE(node_of(application, "forwarded").has_value());
    expect_rect(relative(application, "secondary"),
                {200.0F, 0.0F, 200.0F, 200.0F});
  }
  {
    provider::headless::HeadlessProviderFactory providers;
    auto readings = std::make_shared<Readings>();
    Application application = launch(
        providers,
        ArrangementView(part(readings, "primary"), part(readings, "secondary"))
            .arrangement_view_style(ForwardingToSplit{}),
        400.0F, 200.0F);
    expect_rect(relative(application, "secondary"),
                {0.0F, 100.0F, 400.0F, 100.0F});
    EXPECT_EQ(readings->at("secondary").axis, Axis::vertical);
  }
}

TEST_F(ArrangementViewTest, StylesCompareByKindAndAxes) {
  const ArrangementViewStyle automatic = ArrangementViewStyle::automatic();
  const ArrangementViewStyle split = ArrangementViewStyle::split();
  const ArrangementViewStyle overlay = ArrangementViewStyle::overlay();
  EXPECT_EQ(automatic.kind(), ArrangementViewStyle::Kind::automatic);
  EXPECT_EQ(split.kind(), ArrangementViewStyle::Kind::split);
  EXPECT_EQ(overlay.kind(), ArrangementViewStyle::Kind::overlay);
  EXPECT_EQ(split.axes(), composition::Axes::all);
  EXPECT_EQ(components::AutomaticArrangementViewStyle{},
            ArrangementViewStyle::automatic());
  const ArrangementViewStyle pinned =
      SplitArrangementViewStyle().axes(composition::Axes::vertical);
  EXPECT_EQ(pinned.axes(), composition::Axes::vertical);
  EXPECT_FALSE(pinned == split);
  EXPECT_EQ(
      OverlayArrangementViewStyle().axes(composition::Axes::horizontal).axes(),
      composition::Axes::horizontal);
  const ArrangementViewStyle custom{Stacked{}};
  EXPECT_EQ(custom.kind(), ArrangementViewStyle::Kind::custom);
  EXPECT_TRUE(custom == ArrangementViewStyle{Stacked{}});
  EXPECT_FALSE(custom == ArrangementViewStyle{Forwarding{}});
  EXPECT_EQ(ArrangementViewStyle::key.fallback(), automatic);
}

TEST_F(ArrangementViewTest, ModifiersRecordTheirLayoutValues) {
  const Element ratio = components::ZStack()
                            .split_arrangement_layout_ratio(0.3F)
                            .split_arrangement_layout_size(
                                {.min_width = 10.0F, .max_height = 90.0F})
                            .split_arrangement_fixed_layout_size(true, false);
  const SplitArrangementSizing& sizing =
      ratio.modifiers().layout_values.get(SplitArrangementSizing::key);
  EXPECT_EQ(sizing.ratio(Axis::horizontal).ideal, 0.3F);
  EXPECT_EQ(sizing.ratio(Axis::vertical).ideal, 0.3F);
  EXPECT_EQ(sizing.size(Axis::horizontal).minimum, 10.0F);
  EXPECT_EQ(sizing.size(Axis::vertical).maximum, 90.0F);
  EXPECT_TRUE(sizing.fixed(Axis::horizontal));
  EXPECT_FALSE(sizing.fixed(Axis::vertical));
  const Element cleared =
      components::ZStack()
          .split_arrangement_layout_ratio(
              SplitArrangementSizing::Ratios{.max_vertical = 0.6F})
          .split_arrangement_layout_ratio(std::nullopt);
  EXPECT_FALSE(cleared.modifiers()
                   .layout_values.get(SplitArrangementSizing::key)
                   .ratio(Axis::vertical)
                   .declared());
  const Element edged =
      components::ZStack()
          .overlay_arrangement_edge(composition::VerticalEdge::top)
          .overlay_arrangement_edge(composition::HorizontalEdge::leading);
  const composition::OverlayArrangementEdges& edges =
      edged.modifiers().layout_values.get(
          composition::OverlayArrangementEdges::key);
  EXPECT_EQ(edges.vertical(), composition::VerticalEdge::top);
  EXPECT_EQ(edges.horizontal(), composition::HorizontalEdge::leading);
}

TEST_F(ArrangementViewTest, EnvironmentDefaultsOutsideAnArrangement) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = start(providers, part(readings, "outside"));
  frames(application, 1);
  EXPECT_EQ(readings->at("outside"),
            (Reading{.axis = std::nullopt, .z_index = 0}));
}

TEST_F(ArrangementViewTest, ChangingARatioRearrangesEveryTime) {
  provider::headless::HeadlessProviderFactory providers;
  auto readings = std::make_shared<Readings>();
  Application application = start(providers, Element{Stepped{readings}});
  frames(application, 2);
  for (int round = 0; round < 7; ++round) {
    const float expected =
        400.0F * Stepped::ratios[static_cast<std::size_t>(round) %
                                 Stepped::ratios.size()];
    EXPECT_NEAR(relative(application, "secondary").width, expected, tolerance)
        << round;
    ASSERT_TRUE(application.root().activate(composition::ElementKey{"next"}));
    frames(application, 3);
  }
}

}
