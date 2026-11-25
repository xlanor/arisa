#include <layout_fixture.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/seed.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::rendering {

using namespace std::chrono_literals;

class EffectsPainterTest : public ::testing::Test,
                           protected layout::LayoutFixture {
 protected:
  static constexpr std::uint32_t card_rgb = 0x204060;
  static constexpr std::uint32_t detail_rgb = 0x608020;
  static constexpr std::uint32_t child_rgb = 0x803010;
  static constexpr composition::Namespace hero{"hero"};

  struct Quad {
    float centre_x{0.0F};
    float centre_y{0.0F};
    float half_width{0.0F};
    float half_height{0.0F};
    float left{0.0F};
    float top{0.0F};
    float radius{0.0F};
    std::uint32_t rgb{0};
    std::uint32_t alpha{0};
  };

  class Hero final : public composition::Component {
   public:
    Hero(const composition::Observable<bool>& open, bool same_id,
         motion::MatchedGeometryProperties properties)
        : open_(open), same_id_(same_id), properties_(properties) {}

    auto operator==(const Hero&) const -> bool { return false; }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool open = context.read(open_.get());
      const composition::Namespace::ID space = context.namespace_id(hero);
      if (open) {
        return components::VStack(
            components::VStack(components::VStack()
                                   .frame({.width = 40.0F, .height = 40.0F})
                                   .background(theme::Colour::hex(child_rgb))
                                   .id("child"))
                .frame({.width = 400.0F, .height = 200.0F})
                .background(theme::Colour::hex(detail_rgb),
                            theme::RoundedRectangle(20.0F))
                .matched_geometry_effect(same_id_ ? "card" : "other", space,
                                         properties_,
                                         motion::UnitPoint::top_leading)
                .id("detail"));
      }
      return components::VStack(
          components::VStack()
              .frame({.width = 100.0F, .height = 50.0F})
              .background(theme::Colour::hex(card_rgb),
                          theme::RoundedRectangle(6.0F))
              .matched_geometry_effect("card", space, properties_,
                                       motion::UnitPoint::top_leading)
              .id("card"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> open_;
    bool same_id_;
    motion::MatchedGeometryProperties properties_;
  };

  class Tabs final : public composition::Component {
   public:
    explicit Tabs(const composition::Observable<int>& selected)
        : selected_(selected) {}

    auto operator==(const Tabs&) const -> bool { return false; }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int chosen = context.read(selected_.get());
      const composition::Namespace::ID space = context.namespace_id(hero);
      return components::VStack(
          components::VStack()
              .frame({.width = 80.0F, .height = 30.0F})
              .matched_geometry_effect(0, space)
              .id("tab0"),
          components::VStack()
              .frame({.width = 160.0F, .height = 30.0F})
              .matched_geometry_effect(1, space)
              .id("tab1"),
          components::VStack()
              .frame({.width = 10.0F, .height = 10.0F})
              .background(theme::Colour::hex(card_rgb))
              .matched_geometry_effect(chosen, space,
                                       motion::MatchedGeometryProperties::frame,
                                       motion::UnitPoint::center, false)
              .id("indicator"));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> selected_;
  };

  class Counter final : public composition::Component {
   public:
    Counter(const composition::Observable<int>& count,
            motion::ContentTransition transition)
        : count_(count), transition_(transition) {}

    auto operator==(const Counter&) const -> bool { return false; }

    auto body(composition::Context& context) const
        -> composition::Element override {
      return components::VStack(
          components::Text(std::format("{}", context.read(count_.get())))
              .content_transition(transition_)
              .id("count"));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> count_;
    motion::ContentTransition transition_;
  };

  void SetUp() override {
    auto created = Painter::create(gpu_);
    ASSERT_TRUE(created.has_value());
    painter_.emplace(std::move(*created));
    auto context = layout::LayoutContext::create();
    ASSERT_TRUE(context.has_value());
    layout_.emplace(std::move(*context));
  }

  auto paint_at(composition::Tree& tree, std::chrono::nanoseconds now,
                Painter::Highlights highlights = {})
      -> std::vector<provider::ShapeVertex> {
    const Painter::TypesetterRef setter = std::ref(typesetter());
    EXPECT_TRUE(layout_->update(tree, handheld, setter).has_value());
    auto frame = gpu_.begin_frame();
    EXPECT_TRUE(frame.has_value());
    auto statistics = painter_->paint(
        tree, *frame,
        Painter::Target{
            .pixels = handheld.pixels, .scale = handheld.scale, .time = now},
        setter, highlights);
    EXPECT_TRUE(statistics.has_value());
    EXPECT_TRUE(frame->present().has_value());
    tree.close_transactions();
    return gpu_.last_shapes().vertices;
  }

  static auto quads(const std::vector<provider::ShapeVertex>& vertices,
                    provider::ShapeKind kind) -> std::vector<Quad> {
    std::vector<Quad> found;
    for (std::size_t first = 0; first + 3 < vertices.size(); first += 4) {
      const provider::ShapeVertex& corner = vertices[first];
      if (corner.kind != kind) {
        continue;
      }
      Quad quad{.centre_x = 0.0F,
                .centre_y = 0.0F,
                .half_width = corner.half_size[0],
                .half_height = corner.half_size[1],
                .left = corner.position[0],
                .top = corner.position[1],
                .radius = corner.shape[provider::ShapeVertex::radius],
                .rgb = corner.colour & 0x00FFFFFFU,
                .alpha = corner.colour >> 24U};
      for (std::size_t index = first; index < first + 4; ++index) {
        quad.centre_x += vertices[index].position[0] * 0.25F;
        quad.centre_y += vertices[index].position[1] * 0.25F;
      }
      quad.left = quad.centre_x - quad.half_width;
      quad.top = quad.centre_y - quad.half_height;
      found.push_back(quad);
    }
    return found;
  }

  static auto rgb(std::uint32_t value) -> std::uint32_t {
    return provider::Vertex::rgba(static_cast<std::uint8_t>(value >> 16U),
                                  static_cast<std::uint8_t>(value >> 8U),
                                  static_cast<std::uint8_t>(value), 0);
  }

  static auto fills(const std::vector<provider::ShapeVertex>& vertices,
                    std::uint32_t colour) -> std::vector<Quad> {
    std::vector<Quad> found = quads(vertices, provider::ShapeKind::fill);
    std::erase_if(
        found, [colour](const Quad& quad) { return quad.rgb != rgb(colour); });
    return found;
  }

  static auto frame_of(const composition::Tree& tree, std::string_view key)
      -> composition::Rect {
    return LayoutFixture::frame(tree, key);
  }

  static auto focused_button(motion::HoverEffect effect)
      -> composition::Element {
    return components::VStack(components::Button("Go", [] {})
                                  .frame({.width = 120.0F, .height = 60.0F})
                                  .background(theme::Colour::hex(card_rgb))
                                  .hover_effect(effect)
                                  .id("go"));
  }

  provider::headless::NullGpu gpu_;
  std::optional<Painter> painter_;
  std::optional<layout::LayoutContext> layout_;
};

TEST_F(EffectsPainterTest,
       MatchedArrivalStartsAtTheDepartedFrameAndCarriesChildren) {
  composition::Observable<bool> open{false};
  composition::Tree tree{
      Hero{open, true, motion::MatchedGeometryProperties::frame},
      [] { return true; }};
  tree.reconcile();
  const auto before = fills(paint_at(tree, 0ms), card_rgb);
  ASSERT_EQ(before.size(), 1U);
  const composition::Rect card = frame_of(tree, "card");
  motion::with_animation(motion::Animation::linear(1.0),
                         [&open] { open.set(true); });
  tree.reconcile();
  EXPECT_EQ(tree.departing(), 0U);
  EXPECT_EQ(tree.transitions_held(), 0U);
  const auto vertices = paint_at(tree, 1s);
  EXPECT_TRUE(fills(vertices, card_rgb).empty());
  const auto start = fills(vertices, detail_rgb);
  ASSERT_EQ(start.size(), 1U);
  EXPECT_NEAR(start[0].centre_x, before[0].centre_x, 0.6F);
  EXPECT_NEAR(start[0].centre_y, before[0].centre_y, 0.6F);
  EXPECT_NEAR(start[0].half_width, 50.0F, 0.6F);
  EXPECT_NEAR(start[0].half_height, 25.0F, 0.6F);
  EXPECT_EQ(start[0].alpha, 255U);
  const composition::Rect detail = frame_of(tree, "detail");
  const composition::Rect inner = frame_of(tree, "child");
  const auto child = fills(vertices, child_rgb);
  ASSERT_EQ(child.size(), 1U);
  EXPECT_NEAR(child[0].half_width, 5.0F, 0.1F);
  EXPECT_NEAR(child[0].left, card.x + ((inner.x - detail.x) * 0.25F), 0.6F);
  EXPECT_NEAR(child[0].top, card.y + ((inner.y - detail.y) * 0.25F), 0.6F);
  const auto halfway = fills(paint_at(tree, 1500ms), detail_rgb);
  ASSERT_EQ(halfway.size(), 1U);
  EXPECT_NEAR(halfway[0].half_width, 125.0F, 0.6F);
  EXPECT_NEAR(halfway[0].left, (card.x + detail.x) * 0.5F, 0.6F);
  const auto settled = paint_at(tree, 2100ms);
  ASSERT_EQ(fills(settled, detail_rgb).size(), 1U);
  EXPECT_NEAR(fills(settled, detail_rgb)[0].half_width, 200.0F, 0.01F);
  EXPECT_NEAR(fills(settled, child_rgb)[0].half_width, 20.0F, 0.01F);
}

TEST_F(EffectsPainterTest, MatchedCornerRadiusAndColourStartFromTheSource) {
  composition::Observable<bool> open{false};
  composition::Tree tree{
      Hero{open, true,
           motion::MatchedGeometryProperties::frame |
               motion::MatchedGeometryProperties::corner_radius |
               motion::MatchedGeometryProperties::colour},
      [] { return true; }};
  tree.reconcile();
  const auto before = fills(paint_at(tree, 0ms), card_rgb);
  ASSERT_EQ(before.size(), 1U);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&open] { open.set(true); });
  tree.reconcile();
  const auto start = fills(paint_at(tree, 1s), card_rgb);
  ASSERT_EQ(start.size(), 1U);
  EXPECT_NEAR(start[0].radius, before[0].radius, 0.01F);
  EXPECT_NEAR(start[0].half_width, 50.0F, 0.6F);
  const auto settled = fills(paint_at(tree, 2100ms), detail_rgb);
  ASSERT_EQ(settled.size(), 1U);
  EXPECT_NEAR(settled[0].radius, 20.0F, 0.01F);
}

TEST_F(EffectsPainterTest, UnmatchedIdsFallBackToTheNormalTransition) {
  composition::Observable<bool> open{false};
  composition::Tree tree{
      Hero{open, false, motion::MatchedGeometryProperties::frame},
      [] { return true; }};
  tree.reconcile();
  paint_at(tree, 0ms);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&open] { open.set(true); });
  tree.reconcile();
  EXPECT_EQ(tree.departing(), 1U);
  EXPECT_EQ(tree.transitions_held(), 1U);
  paint_at(tree, 1s);
  const auto vertices = paint_at(tree, 1500ms);
  const auto leaving = fills(vertices, card_rgb);
  const auto arriving = fills(vertices, detail_rgb);
  ASSERT_EQ(leaving.size(), 1U);
  ASSERT_EQ(arriving.size(), 1U);
  EXPECT_NEAR(static_cast<double>(leaving[0].alpha), 128.0, 2.0);
  EXPECT_NEAR(static_cast<double>(arriving[0].alpha), 128.0, 2.0);
  EXPECT_NEAR(arriving[0].half_width, 200.0F, 0.01F);
  paint_at(tree, 2100ms);
  tree.reconcile();
  EXPECT_EQ(tree.departing(), 0U);
  EXPECT_EQ(tree.transitions_held(), 0U);
}

TEST_F(EffectsPainterTest, NonSourceFollowsTheSourceFrame) {
  composition::Observable<int> selected{0};
  composition::Tree tree{Tabs{selected}, [] { return true; }};
  tree.reconcile();
  const auto first = fills(paint_at(tree, 0ms), card_rgb);
  ASSERT_EQ(first.size(), 1U);
  const composition::Rect tab0 = frame_of(tree, "tab0");
  const composition::Rect tab1 = frame_of(tree, "tab1");
  EXPECT_NEAR(first[0].left, tab0.x, 0.01F);
  EXPECT_NEAR(first[0].top, tab0.y, 0.01F);
  EXPECT_NEAR(first[0].half_width, 40.0F, 0.01F);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&selected] { selected.set(1); });
  tree.reconcile();
  paint_at(tree, 1s);
  const auto halfway = fills(paint_at(tree, 1500ms), card_rgb);
  ASSERT_EQ(halfway.size(), 1U);
  EXPECT_NEAR(halfway[0].half_width, 60.0F, 0.01F);
  EXPECT_NEAR(halfway[0].top, (tab0.y + tab1.y) * 0.5F, 0.01F);
  const auto settled = fills(paint_at(tree, 2100ms), card_rgb);
  EXPECT_NEAR(settled[0].left, tab1.x, 0.01F);
  EXPECT_NEAR(settled[0].half_width, 80.0F, 0.01F);
}

TEST_F(EffectsPainterTest, NumericTextMovesOnlyTheDigitsThatChange) {
  composition::Observable<int> count{10};
  composition::Tree tree{
      Counter{count, motion::ContentTransition::numeric_text()},
      [] { return true; }};
  tree.reconcile();
  const auto still = quads(paint_at(tree, 0ms), provider::ShapeKind::glyph);
  ASSERT_EQ(still.size(), 2U);
  const auto by_left = [](const Quad& left, const Quad& right) {
    return left.left < right.left;
  };
  std::vector<Quad> resting = still;
  std::ranges::sort(resting, by_left);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&count] { count.set(11); });
  tree.reconcile();
  paint_at(tree, 1s);
  std::vector<Quad> rolling =
      quads(paint_at(tree, 1500ms), provider::ShapeKind::glyph);
  ASSERT_EQ(rolling.size(), 3U);
  std::ranges::sort(rolling, by_left);
  EXPECT_NEAR(rolling[0].left, resting[0].left, 1.5F);
  EXPECT_NEAR(rolling[0].top, resting[0].top, 0.01F);
  EXPECT_EQ(rolling[0].alpha, 255U);
  for (std::size_t index = 1; index < rolling.size(); ++index) {
    EXPECT_GT(rolling[index].left, resting[0].left + 1.0F);
    EXPECT_GT(std::abs(rolling[index].centre_y - resting[1].centre_y), 2.0F);
    EXPECT_NEAR(static_cast<double>(rolling[index].alpha), 128.0, 2.0);
  }
  const float upper = std::min(rolling[1].centre_y, rolling[2].centre_y);
  const float lower = std::max(rolling[1].centre_y, rolling[2].centre_y);
  EXPECT_LT(upper, resting[1].centre_y);
  EXPECT_GT(lower, resting[1].centre_y - 6.0F);
  const auto done = quads(paint_at(tree, 2100ms), provider::ShapeKind::glyph);
  ASSERT_EQ(done.size(), 2U);
  for (const Quad& glyph : done) {
    EXPECT_EQ(glyph.alpha, 255U);
  }
}

TEST_F(EffectsPainterTest, OpacityContentTransitionAlphasSumToOpaque) {
  composition::Observable<int> count{10};
  composition::Tree tree{Counter{count, motion::ContentTransition::opacity},
                         [] { return true; }};
  tree.reconcile();
  const auto still = quads(paint_at(tree, 0ms), provider::ShapeKind::glyph);
  ASSERT_EQ(still.size(), 2U);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&count] { count.set(25); });
  tree.reconcile();
  paint_at(tree, 1s);
  for (const auto moment : {1250ms, 1500ms, 1750ms}) {
    const auto fading =
        quads(paint_at(tree, moment), provider::ShapeKind::glyph);
    ASSERT_EQ(fading.size(), 4U);
    std::uint32_t total = 0;
    for (const Quad& glyph : fading) {
      total += glyph.alpha;
    }
    EXPECT_NEAR(static_cast<double>(total), 510.0, 3.0);
    EXPECT_NEAR(static_cast<double>(fading[0].alpha + fading[2].alpha), 255.0,
                1.0);
  }
}

TEST_F(EffectsPainterTest, LiftSpringsInAndOutOnFocusChange) {
  composition::Tree tree{focused_button(motion::HoverEffect::lift),
                         [] { return true; }};
  tree.reconcile();
  const auto go = tree.find(composition::ElementKey{"go"});
  ASSERT_TRUE(go.has_value());
  const Painter::Highlights focus{.focused = go, .pressed = std::nullopt};
  const auto resting = fills(paint_at(tree, 0ms), card_rgb);
  ASSERT_EQ(resting.size(), 1U);
  EXPECT_NEAR(resting[0].half_width, 60.0F, 0.01F);
  EXPECT_TRUE(quads(paint_at(tree, 0ms), provider::ShapeKind::stroke).empty());
  EXPECT_NEAR(fills(paint_at(tree, 0ms, focus), card_rgb)[0].half_width, 60.0F,
              0.01F);
  const float rising =
      fills(paint_at(tree, 80ms, focus), card_rgb)[0].half_width;
  EXPECT_GT(rising, 60.5F);
  EXPECT_LT(rising, 63.5F);
  const auto lifted = paint_at(tree, 2s, focus);
  EXPECT_NEAR(fills(lifted, card_rgb)[0].half_width,
              60.0F * (1.0F + motion::HoverEffect::lift_scale), 0.05F);
  const auto rings = quads(lifted, provider::ShapeKind::stroke);
  ASSERT_EQ(rings.size(), 1U);
  EXPECT_NEAR(static_cast<double>(rings[0].alpha), 255.0, 2.0);
  EXPECT_GE(quads(lifted, provider::ShapeKind::shadow).size() +
                quads(lifted, provider::ShapeKind::glyph).size(),
            2U);
  paint_at(tree, 2s);
  const float falling = fills(paint_at(tree, 2080ms), card_rgb)[0].half_width;
  EXPECT_LT(falling, 63.5F);
  EXPECT_GT(falling, 60.2F);
  const auto rested = paint_at(tree, 4s);
  EXPECT_NEAR(fills(rested, card_rgb)[0].half_width, 60.0F, 0.01F);
  EXPECT_TRUE(quads(rested, provider::ShapeKind::stroke).empty());
}

TEST_F(EffectsPainterTest, HighlightBrightensTheFillWithoutScaling) {
  composition::Tree tree{focused_button(motion::HoverEffect::highlight),
                         [] { return true; }};
  tree.reconcile();
  const auto go = tree.find(composition::ElementKey{"go"});
  const Painter::Highlights focus{.focused = go, .pressed = std::nullopt};
  const auto resting =
      quads(paint_at(tree, 0ms), provider::ShapeKind::fill).front();
  paint_at(tree, 0ms, focus);
  const auto lit = quads(paint_at(tree, 2s, focus), provider::ShapeKind::fill);
  ASSERT_FALSE(lit.empty());
  EXPECT_NEAR(lit.front().half_width, resting.half_width, 0.01F);
  EXPECT_NE(lit.front().rgb, resting.rgb);
  const auto lightness = [](std::uint32_t colour) {
    return (colour & 0xFFU) + ((colour >> 8U) & 0xFFU) +
           ((colour >> 16U) & 0xFFU);
  };
  EXPECT_GT(lightness(lit.front().rgb), lightness(resting.rgb));
}

TEST_F(EffectsPainterTest, TintReseedsEveryAccentRole) {
  const theme::Colour orange = theme::Colour::hex(0xFF9500);
  const theme::Theme tinted = theme::Theme::from(theme::Seed{.accent = orange});
  const auto packed = [](theme::Colour colour) {
    return rgb(
        (static_cast<std::uint32_t>(std::lround(colour.red * 255.0F)) << 16U) |
        (static_cast<std::uint32_t>(std::lround(colour.green * 255.0F)) << 8U) |
        static_cast<std::uint32_t>(std::lround(colour.blue * 255.0F)));
  };
  const auto scene = [](std::optional<graphics::ShapeStyle> tint) {
    return components::VStack(
        components::VStack(components::ZStack()
                               .frame({.width = 80.0F, .height = 30.0F})
                               .background(theme::ColourRole::accent),
                           components::Button("Go", [] {})
                               .frame({.width = 120.0F, .height = 40.0F})
                               .id("go"))
            .environment(components::Environment::tint, tint));
  };
  const std::uint32_t accent = packed(
      tinted.colour(theme::ColourRole::accent, theme::ColorScheme::light));
  const std::uint32_t ring = packed(tinted.colour(
      theme::ColourRole::keyboard_focus_indicator, theme::ColorScheme::light));
  for (const bool tinting : {false, true}) {
    layout_.emplace(std::move(*layout::LayoutContext::create()));
    composition::Tree tree{
        scene(tinting ? std::optional<theme::Colour>{orange} : std::nullopt),
        [] { return true; }};
    tree.reconcile();
    const Painter::Highlights focus{
        .focused = tree.find(composition::ElementKey{"go"}),
        .pressed = std::nullopt};
    paint_at(tree, 0ms, focus);
    const auto painted = paint_at(tree, 2s, focus);
    const auto filled = quads(painted, provider::ShapeKind::fill);
    const auto stroked = quads(painted, provider::ShapeKind::stroke);
    const bool has_accent = std::ranges::any_of(
        filled, [accent](const Quad& quad) { return quad.rgb == accent; });
    const bool has_ring = std::ranges::any_of(
        stroked, [ring](const Quad& quad) { return quad.rgb == ring; });
    EXPECT_EQ(has_accent, tinting);
    EXPECT_EQ(has_ring, tinting);
  }
}

TEST_F(EffectsPainterTest, TiltIsClampedToTheUnitDiscAndSmoothed) {
  const auto settle = [this](provider::Stick stick) {
    layout_.emplace(std::move(*layout::LayoutContext::create()));
    composition::Tree tree{focused_button(motion::HoverEffect::lift.tilt(
                               motion::Angle::degrees(4.0), 6.0F)),
                           [] { return true; }};
    tree.reconcile();
    const auto go = tree.find(composition::ElementKey{"go"});
    const float rest = fills(paint_at(tree, 0ms), card_rgb)[0].centre_x;
    const Painter::Highlights tilted{
        .focused = go, .pressed = std::nullopt, .tilt = stick};
    paint_at(tree, 0ms, tilted);
    const float early =
        fills(paint_at(tree, 16ms, tilted), card_rgb)[0].centre_x - rest;
    const float late =
        fills(paint_at(tree, 3s, tilted), card_rgb)[0].centre_x - rest;
    return std::pair{early, late};
  };
  const auto [early, late] = settle(provider::Stick{.x = 1.0F, .y = 0.0F});
  EXPECT_NEAR(late, 6.0F, 0.05F);
  EXPECT_GT(early, 0.0F);
  EXPECT_LT(early, 3.0F);
  const auto [over_early, over_late] =
      settle(provider::Stick{.x = 2.0F, .y = 0.0F});
  EXPECT_NEAR(over_late, late, 0.01F);
  EXPECT_NEAR(over_early, early, 0.01F);
}

}
