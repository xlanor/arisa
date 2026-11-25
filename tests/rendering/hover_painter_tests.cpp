#include <layout_fixture.hpp>

#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/content_hover_effect.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/group_hover_effect.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_effect_phase_override.hpp>
#include <arisa/composition/hover_path.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::rendering {

using namespace std::chrono_literals;

class HoverPainterTest : public ::testing::Test,
                         protected layout::LayoutFixture {
 protected:
  static constexpr std::uint32_t card_rgb = 0x204060;
  static constexpr std::uint32_t other_rgb = 0x608020;
  static constexpr float card_width = 120.0F;
  static constexpr composition::Namespace rows{"hover-rows"};

  class Toggling final : public composition::Component {
   public:
    explicit Toggling(const composition::Observable<int>& trigger)
        : trigger_(trigger) {}

    auto operator==(const Toggling&) const -> bool { return false; }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int value = context.read(trigger_.get());
      return components::VStack(
          card(card_rgb,
               composition::CustomHoverEffect{motion::HoverEffect::lift}
                   .hover_effect_phase_override(
                       composition::HoverEffectPhaseOverride::toggled(value)),
               "card"));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> trigger_;
  };

  class Lifted {
   public:
    auto body(composition::CustomHoverEffect content) const
        -> composition::CustomHoverEffect {
      return content.hover_effect(motion::HoverEffect::lift)
          .hover_effect(motion::HoverEffect::highlight);
    }
  };

  struct Quad {
    float half_width{0.0F};
    float half_height{0.0F};
    float radius{0.0F};
    std::uint32_t rgb{0};
  };

  void SetUp() override { renew(); }

  void renew() {
    painter_.reset();
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
      found.push_back(
          Quad{.half_width = corner.half_size[0],
               .half_height = corner.half_size[1],
               .radius = corner.shape[provider::ShapeVertex::radius],
               .rgb = corner.colour & 0x00FFFFFFU});
    }
    return found;
  }

  static auto rgb(std::uint32_t value) -> std::uint32_t {
    return provider::Vertex::rgba(static_cast<std::uint8_t>(value >> 16U),
                                  static_cast<std::uint8_t>(value >> 8U),
                                  static_cast<std::uint8_t>(value), 0);
  }

  static auto width_of(const std::vector<provider::ShapeVertex>& vertices,
                       std::uint32_t colour) -> float {
    for (const Quad& quad : quads(vertices, provider::ShapeKind::fill)) {
      if (quad.rgb == rgb(colour)) {
        return quad.half_width * 2.0F;
      }
    }
    return 0.0F;
  }

  static auto card(std::uint32_t colour, composition::CustomHoverEffect effect,
                   std::string_view key) -> composition::Element {
    return components::ZStack()
        .frame({.width = card_width, .height = 60.0F})
        .background(theme::Colour::hex(colour))
        .hover_effect(std::move(effect))
        .id(std::string{key});
  }

  static auto hovering(std::vector<composition::NodeId> chain,
                       std::vector<composition::HoverPath::Group> groups = {})
      -> Painter::Highlights {
    Painter::Highlights highlights{};
    highlights.hovering = std::make_shared<const composition::HoverPath>(
        std::move(chain), std::move(groups));
    return highlights;
  }

  static auto id_of(const composition::Tree& tree, std::string_view key)
      -> composition::NodeId {
    const auto found = tree.find(composition::ElementKey{key});
    EXPECT_TRUE(found.has_value());
    return found.value_or(composition::NodeId{0});
  }

  provider::headless::NullGpu gpu_;
  std::optional<Painter> painter_;
  std::optional<layout::LayoutContext> layout_;
};

TEST_F(HoverPainterTest, PointerHoverLiftsAViewThatCannotTakeFocus) {
  composition::Tree tree{
      components::VStack(card(card_rgb, motion::HoverEffect::lift, "card")),
      [] { return true; }};
  tree.reconcile();
  EXPECT_NEAR(width_of(paint_at(tree, 0ms), card_rgb), card_width, 0.05F);
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  EXPECT_NEAR(width_of(paint_at(tree, 2s, over), card_rgb),
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
  paint_at(tree, 2s, hovering({}));
  EXPECT_NEAR(width_of(paint_at(tree, 4s, hovering({})), card_rgb), card_width,
              0.05F);
}

TEST_F(HoverPainterTest, AnImplicitGroupLiftsEveryMember) {
  composition::Tree tree{
      components::VStack(
          components::VStack(
              card(card_rgb, motion::HoverEffect::lift, "card"),
              card(other_rgb, motion::HoverEffect::lift, "other"))
              .hover_effect_group()
              .id("group")),
      [] { return true; }};
  tree.reconcile();
  const composition::NodeId group = id_of(tree, "group");
  const Painter::Highlights over =
      hovering({id_of(tree, "card"), group},
               {composition::HoverPath::Group{.container = group,
                                              .named = std::nullopt}});
  paint_at(tree, 0ms, over);
  const auto lifted = paint_at(tree, 2s, over);
  EXPECT_NEAR(width_of(lifted, other_rgb),
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
}

TEST_F(HoverPainterTest, ANamedGroupActivatesFollowersAnywhere) {
  const composition::HoverEffectGroup leader{rows.id()};
  const composition::HoverEffectGroup follower =
      leader.behavior(composition::HoverEffectGroup::Behavior::follows_group);
  composition::Tree tree{
      components::VStack(
          card(card_rgb,
               composition::CustomHoverEffect{motion::HoverEffect::lift}
                   .hover_effect_group(leader),
               "card"),
          card(other_rgb,
               composition::CustomHoverEffect{motion::HoverEffect::lift}
                   .hover_effect_group(follower),
               "other")),
      [] { return true; }};
  tree.reconcile();
  const Painter::Highlights alone = hovering({id_of(tree, "other")});
  paint_at(tree, 0ms, alone);
  EXPECT_NEAR(width_of(paint_at(tree, 2s, alone), other_rgb), card_width,
              0.05F);
  const Painter::Highlights led = hovering(
      {id_of(tree, "card")}, {composition::HoverPath::Group{
                                 .container = std::nullopt, .named = leader}});
  paint_at(tree, 2s, led);
  EXPECT_NEAR(width_of(paint_at(tree, 4s, led), other_rgb),
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
}

TEST_F(HoverPainterTest, TheDefaultEffectReplacesAutomatic) {
  composition::Tree tree{
      components::VStack(card(card_rgb, motion::HoverEffect::automatic, "card"))
          .default_hover_effect(
              composition::CustomHoverEffect{motion::HoverEffect::highlight}),
      [] { return true; }};
  tree.reconcile();
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  const auto lit = paint_at(tree, 2s, over);
  EXPECT_NEAR(width_of(lit, card_rgb), 0.0F, 0.01F);
  const auto fills = quads(lit, provider::ShapeKind::fill);
  ASSERT_FALSE(fills.empty());
  EXPECT_NEAR(fills.front().half_width * 2.0F, card_width, 0.05F);
}

TEST_F(HoverPainterTest, DisabledHoverEffectsStayStill) {
  composition::Tree tree{
      components::VStack(card(card_rgb, motion::HoverEffect::lift, "card"))
          .hover_effect_disabled(),
      [] { return true; }};
  tree.reconcile();
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  EXPECT_NEAR(width_of(paint_at(tree, 2s, over), card_rgb), card_width, 0.05F);
  composition::Tree empty{
      components::VStack(
          card(card_rgb, composition::CustomHoverEffect::empty(), "card")),
      [] { return true; }};
  empty.reconcile();
  renew();
  const Painter::Highlights inside = hovering({id_of(empty, "card")});
  paint_at(empty, 0ms, inside);
  EXPECT_NEAR(width_of(paint_at(empty, 2s, inside), card_rgb), card_width,
              0.05F);
}

TEST_F(HoverPainterTest, AContentEffectFollowsItsClosure) {
  composition::Tree tree{
      components::VStack(
          card(card_rgb,
               composition::CustomHoverEffect::empty().hover_effect(
                   [](composition::VisualEffect content, bool active,
                      const composition::GeometryProxy&) {
                     return content.scale_effect(active ? 1.5F : 1.0F);
                   }),
               "card")),
      [] { return true; }};
  tree.reconcile();
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  EXPECT_NEAR(width_of(paint_at(tree, 2s, over), card_rgb), card_width * 1.5F,
              0.1F);
  composition::Tree statics{
      components::VStack(
          card(card_rgb,
               composition::ContentHoverEffect{
                   [](composition::VisualEffect content, bool active,
                      const composition::GeometryProxy&) {
                     return content.scale_effect(active ? 0.5F : 1.0F);
                   }},
               "card")),
      [] { return true; }};
  statics.reconcile();
  renew();
  const Painter::Highlights inside = hovering({id_of(statics, "card")});
  paint_at(statics, 0ms, inside);
  EXPECT_NEAR(width_of(paint_at(statics, 2s, inside), card_rgb),
              card_width * 0.5F, 0.1F);
}

TEST_F(HoverPainterTest, ACustomBodyComposesItsLayers) {
  composition::Tree tree{components::VStack(card(card_rgb, Lifted{}, "card")),
                         [] { return true; }};
  tree.reconcile();
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  const auto lifted = paint_at(tree, 2s, over);
  const auto fills = quads(lifted, provider::ShapeKind::fill);
  ASSERT_FALSE(fills.empty());
  EXPECT_NEAR(fills.front().half_width * 2.0F,
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
  EXPECT_NE(fills.front().rgb, rgb(card_rgb));
}

TEST_F(HoverPainterTest, PhaseOverridesForceAndToggleThePhase) {
  composition::Tree forced{
      components::VStack(
          card(card_rgb,
               composition::CustomHoverEffect{motion::HoverEffect::lift}
                   .hover_effect_phase_override(
                       composition::HoverEffectPhaseOverride::active()),
               "card")),
      [] { return true; }};
  forced.reconcile();
  paint_at(forced, 0ms);
  EXPECT_NEAR(width_of(paint_at(forced, 2s), card_rgb),
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
  composition::Observable<int> trigger{0};
  composition::Tree toggled{Toggling{trigger}, [] { return true; }};
  toggled.reconcile();
  renew();
  paint_at(toggled, 0ms);
  EXPECT_NEAR(width_of(paint_at(toggled, 2s), card_rgb), card_width, 0.05F);
  trigger.set(1);
  toggled.reconcile();
  paint_at(toggled, 2s);
  EXPECT_NEAR(width_of(paint_at(toggled, 4s), card_rgb),
              card_width * (1.0F + motion::HoverEffect::lift_scale), 0.1F);
}

TEST_F(HoverPainterTest, TheHoverShapeOutlinesTheRing) {
  composition::Tree tree{
      components::VStack(
          components::ZStack()
              .frame({.width = card_width, .height = 60.0F})
              .background(theme::Colour::hex(card_rgb))
              .hover_effect(motion::HoverEffect::highlight)
              .content_shape(composition::ContentShapeKinds::hover_effect,
                             theme::Shape::capsule())
              .id("card")),
      [] { return true; }};
  tree.reconcile();
  const Painter::Highlights over = hovering({id_of(tree, "card")});
  paint_at(tree, 0ms, over);
  const auto rings =
      quads(paint_at(tree, 2s, over), provider::ShapeKind::stroke);
  ASSERT_FALSE(rings.empty());
  EXPECT_GE(rings.front().radius, rings.front().half_height - 1.0F);
}

}
