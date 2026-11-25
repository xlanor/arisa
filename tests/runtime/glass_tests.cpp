#include <arisa/attr/appearance.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class GlassTest : public FocusFixture {
 protected:
  static constexpr composition::Namespace space{"glass-test"};

  class Morph final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> merged{"glass-merged", false};

    auto operator==(const Morph&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto together = context.state(merged);
      const composition::Namespace::ID id = context.namespace_id(space);
      flag() = together;
      if (together.get()) {
        return components::GlassEffectContainer(
            components::ZStack()
                .frame({.width = 200.0F, .height = 60.0F})
                .glass_effect()
                .glass_effect_id(composition::ElementKey{"blob"}, id)
                .id("merged"));
      }
      return components::GlassEffectContainer(components::HStack(
          components::HStackOptions{.spacing = theme::Space{100.0F}},
          components::ZStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .glass_effect()
              .glass_effect_id(composition::ElementKey{"blob"}, id)
              .id("one"),
          components::ZStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .glass_effect()
              .glass_effect_id(composition::ElementKey{"other"}, id)
              .id("two")));
    }

    static auto flag() -> std::optional<composition::StateHandle<bool>>& {
      static std::optional<composition::StateHandle<bool>> held;
      return held;
    }
  };

  auto glass_fills() -> std::vector<provider::ShapeVertex> {
    return glass_fills(providers_);
  }

  auto surfaces(provider::ShapeKind kind)
      -> std::vector<provider::ShapeVertex> {
    return surfaces(providers_, kind);
  }

  static auto surfaces(provider::headless::HeadlessProviderFactory& providers,
                       provider::ShapeKind kind)
      -> std::vector<provider::ShapeVertex> {
    std::vector<provider::ShapeVertex> found;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      if (vertex.kind == kind) {
        found.push_back(vertex);
      }
    }
    return found;
  }

  static auto glass_fills(
      provider::headless::HeadlessProviderFactory& providers)
      -> std::vector<provider::ShapeVertex> {
    std::vector<provider::ShapeVertex> found;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      const std::uint32_t alpha = vertex.colour >> 24U;
      if (vertex.kind == provider::ShapeKind::fill && alpha > 150U &&
          alpha < 230U) {
        found.push_back(vertex);
      }
    }
    return found;
  }

  static constexpr float canvas = 320.0F;
  static constexpr composition::Rect slab{
      .x = 60.0F, .y = 60.0F, .width = 200.0F, .height = 140.0F};
  static constexpr float slab_radius = 32.0F;
  static constexpr float grey = 0.5F;

  static auto alphas(const std::vector<provider::ShapeVertex>& vertices)
      -> std::uint32_t {
    std::uint32_t most = 0;
    for (const provider::ShapeVertex& vertex : vertices) {
      most = std::max(most, vertex.colour >> 24U);
    }
    return most;
  }

  static auto shadowed(theme::Glass glass, std::optional<theme::Elevation> cast)
      -> std::vector<provider::ShapeVertex> {
    provider::headless::HeadlessProviderFactory providers;
    Element pill = components::ZStack()
                       .frame({.width = 200.0F, .height = 40.0F})
                       .glass_effect(glass, theme::Shape::capsule());
    if (cast.has_value()) {
      pill = std::move(pill).shadow(*cast);
    }
    Application application = start(providers, std::move(pill));
    static_cast<void>(application);
    return surfaces(providers, provider::ShapeKind::shadow);
  }

  static auto slab_scene(Element glass) -> provider::Image {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{static_cast<std::uint32_t>(canvas)},
        .height = provider::SurfaceHeight{static_cast<std::uint32_t>(canvas)}};
    providers.surface().resize(extent);
    providers.surface().set_scale(*provider::SurfaceScale::create(1.0F));
    providers.gpu().rasterize(extent);
    Application application = start(
        providers, components::ZStack(
                       components::ZStackOptions{
                           .alignment = components::Alignment::top_leading},
                       std::move(glass).offset(slab.x, slab.y))
                       .frame({.width = canvas,
                               .height = canvas,
                               .alignment = components::Alignment::top_leading})
                       .background(theme::ColourValue{
                           theme::Colour{grey, grey, grey, 1.0F}}));
    frames(application, 3);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  static auto slab_of(theme::Glass glass, theme::Shape shape) -> Element {
    return components::ZStack()
        .frame({.width = slab.width, .height = slab.height})
        .glass_effect(glass, shape);
  }

  static auto colour_at(const provider::Image& image, float x, float y)
      -> std::array<float, 3> {
    const auto found = image.pixel(static_cast<std::uint32_t>(x),
                                   static_cast<std::uint32_t>(y));
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {static_cast<float>((*found)[0]) / 255.0F,
            static_cast<float>((*found)[1]) / 255.0F,
            static_cast<float>((*found)[2]) / 255.0F};
  }

  static auto luma_at(const provider::Image& image, float x, float y) -> float {
    const std::array<float, 3> c = colour_at(image, x, y);
    return (c[0] * 0.2126F) + (c[1] * 0.7152F) + (c[2] * 0.0722F);
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(GlassTest, TheModifiersRecordTheGlassStructure) {
  const theme::Glass glass =
      theme::Glass::clear().tint(theme::ColourRole::accent).interactive();
  Element element =
      components::ZStack().glass_effect(glass, theme::Shape::capsule());
  ASSERT_TRUE(element.modifiers().glass.has_value());
  EXPECT_EQ(element.modifiers().glass->glass, glass);
  EXPECT_EQ(element.modifiers().glass->shape, theme::Shape::capsule());
  EXPECT_EQ(glass.variant(), theme::Glass::Variant::clear);
  EXPECT_TRUE(glass.is_interactive());
  const Element tagged =
      attr::glass_effect{glass, theme::Shape::capsule()}.apply(
          components::ZStack());
  ASSERT_TRUE(tagged.modifiers().glass.has_value());
  EXPECT_EQ(tagged.modifiers().glass->glass, glass);
  EXPECT_EQ(theme::Glass::regular(), theme::Glass::regular());
  EXPECT_NE(theme::Glass::regular(), theme::Glass::identity());
}

TEST_F(GlassTest, AUnionDrawsOneSurfaceInsideItsContainer) {
  Application application = start(
      providers_,
      components::GlassEffectContainer(
          theme::Space{20.0F},
          components::HStack(
              components::HStackOptions{.spacing = theme::Space{10.0F}},
              components::ZStack()
                  .frame({.width = 40.0F, .height = 30.0F})
                  .glass_effect(theme::Glass::regular(),
                                theme::Shape::rectangle())
                  .id("alone"),
              components::ZStack()
                  .frame({.width = 50.0F, .height = 30.0F})
                  .glass_effect(theme::Glass::regular(),
                                theme::Shape::rectangle())
                  .glass_effect_union(
                      composition::ElementKey{"pair"},
                      composition::Namespace::ID{composition::Identity{&space},
                                                 composition::NodeId{1}}),
              components::ZStack()
                  .frame({.width = 60.0F, .height = 30.0F})
                  .glass_effect(theme::Glass::regular(),
                                theme::Shape::rectangle())
                  .glass_effect_union(
                      composition::ElementKey{"pair"},
                      composition::Namespace::ID{composition::Identity{&space},
                                                 composition::NodeId{1}}))));
  const std::vector<provider::ShapeVertex> unions =
      surfaces(provider::ShapeKind::glass_union);
  ASSERT_EQ(unions.size(), 4U);
  EXPECT_GE(unions.front().half_size[0] * 2.0F, 170.0F);
  EXPECT_TRUE(surfaces(provider::ShapeKind::glass).empty());
}

TEST_F(GlassTest, GlassIdsMorphThroughMatchedGeometry) {
  Application application = start(providers_, Morph{});
  const auto one = node_of(application, "one");
  ASSERT_TRUE(one.has_value());
  const auto& matched = application.root().tree().node(*one)->get();
  const composition::Rect origin = matched.presented()->bounds;
  ASSERT_TRUE(Morph::flag().has_value());
  motion::with_animation(motion::Animation::linear(0.5),
                         [] { Morph::flag()->set(true); });
  frames(application, 1);
  ASSERT_TRUE(
      providers_.clock().advance(std::chrono::milliseconds{16}).has_value());
  frames(application, 1);
  const auto merged = node_of(application, "merged");
  ASSERT_TRUE(merged.has_value());
  const composition::Rect early =
      application.root().tree().node(*merged)->get().presented()->bounds;
  const composition::Rect laid =
      application.root().tree().node(*merged)->get().frame();
  EXPECT_LT(early.width, laid.width - 20.0F);
  EXPECT_NEAR(early.width, origin.width, 20.0F);
  for (int index = 0; index < 40; ++index) {
    ASSERT_TRUE(
        providers_.clock().advance(std::chrono::milliseconds{16}).has_value());
    frames(application, 1);
  }
  const composition::Rect settled =
      application.root().tree().node(*merged)->get().presented()->bounds;
  EXPECT_NEAR(settled.width, laid.width, 0.5F);
}

TEST_F(GlassTest, GlassButtonsKeepTheMeasuredMetrics) {
  Application application = start(
      providers_,
      components::VStack(
          components::Button(
              components::Button::Action{[] {}},
              components::ZStack().frame({.width = 41.0F, .height = 16.0F}))
              .button_style(components::ButtonStyle::glass())
              .id("glass"),
          components::Button(
              components::Button::Action{[] {}},
              components::ZStack().frame({.width = 41.0F, .height = 16.0F}))
              .button_style(components::GlassProminentButtonStyle{})
              .id("prominent")));
  for (const std::string_view key : {"glass", "prominent"}) {
    const auto node = node_of(application, key);
    ASSERT_TRUE(node.has_value());
    const composition::Rect frame =
        application.root().tree().node(*node)->get().frame();
    EXPECT_NEAR(frame.width, 65.0F, 0.5F) << key;
    EXPECT_NEAR(frame.height, 24.0F, 0.5F) << key;
  }
  EXPECT_EQ(components::ButtonStyle::glass(),
            components::ButtonStyle{components::GlassButtonStyle{}});
  EXPECT_NE(components::ButtonStyle::glass(),
            components::ButtonStyle::glass(theme::Glass::clear()));
  EXPECT_NE(components::ButtonStyle::glass(),
            components::ButtonStyle::glass_prominent());
}

TEST_F(GlassTest, DefaultGlassFollowsTheThemeSlots) {
  theme::GlassDefaults defaults;
  defaults.toolbar = theme::Glass::regular();
  const theme::Theme glassy = theme::Theme::defaults().with(defaults);
  Application plain =
      start(providers_, structure::Scaffold(components::ZStack().frame(
                            {.width = 10.0F, .height = 10.0F})));
  const std::size_t without = surfaces(provider::ShapeKind::glass).size();
  provider::headless::HeadlessProviderFactory other;
  Application themed = start(
      other, structure::Scaffold(
                 components::ZStack().frame({.width = 10.0F, .height = 10.0F}))
                 .theme_override(glassy));
  const std::size_t with = surfaces(other, provider::ShapeKind::glass).size();
  EXPECT_EQ(with, without + 8U);
  const auto group = node_of(themed, structure::Scaffold::trailing_group_key);
  ASSERT_TRUE(group.has_value());
  const auto& slot = themed.root().tree().node(*group)->get().modifiers().glass;
  ASSERT_TRUE(slot.has_value());
  EXPECT_EQ(slot->slot, theme::GlassSlot::toolbar);
  static_cast<void>(plain);
}

TEST_F(GlassTest, AGlassShadowFollowsTheGlassShape) {
  Application application =
      start(providers_,
            components::ZStack()
                .frame({.width = 200.0F, .height = 40.0F})
                .glass_effect(theme::Glass::clear(), theme::Shape::capsule())
                .shadow(theme::Elevation::medium));
  const std::vector<provider::ShapeVertex> glass =
      surfaces(provider::ShapeKind::glass);
  const std::vector<provider::ShapeVertex> shadows =
      surfaces(provider::ShapeKind::shadow);
  ASSERT_FALSE(glass.empty());
  ASSERT_EQ(shadows.size(), 8U);
  const float radius = glass.front().shape[provider::ShapeVertex::radius];
  EXPECT_NEAR(radius, glass.front().half_size[1], 0.5F);
  for (const provider::ShapeVertex& vertex : shadows) {
    EXPECT_FLOAT_EQ(vertex.shape[provider::ShapeVertex::radius], radius);
  }
  static_cast<void>(application);
}

TEST_F(GlassTest, AnExplicitShadowReplacesTheLensShadow) {
  const std::vector<provider::ShapeVertex> lens =
      shadowed(theme::Glass::regular(), std::nullopt);
  const std::vector<provider::ShapeVertex> raised =
      shadowed(theme::Glass::regular(), theme::Elevation::medium);
  const std::vector<provider::ShapeVertex> clear =
      shadowed(theme::Glass::clear(), theme::Elevation::medium);
  EXPECT_EQ(lens.size(), 8U);
  EXPECT_EQ(raised.size(), 8U);
  EXPECT_EQ(clear.size(), 8U);
  EXPECT_TRUE(shadowed(theme::Glass::clear(), std::nullopt).empty());
  EXPECT_GT(alphas(raised), alphas(lens));
  EXPECT_GT(alphas(clear), 0U);
  EXPECT_LT(alphas(clear), alphas(raised));
}

TEST_F(GlassTest, ClearGlassNeverSamplesItsOwnShadow) {
  const provider::Image image =
      slab_scene(slab_of(theme::Glass::clear(),
                         theme::Shape::rounded(theme::Radius{slab_radius}))
                     .shadow(theme::ColourValue{theme::Colour::hex(0xFF0000)},
                             0.0F, 0.0F, slab.height * 0.5F));
  const float x = slab.x + (slab.width * 0.5F);
  for (const float y :
       {slab.y + (slab.height * 0.6F), slab.y + (slab.height * 0.75F),
        slab.y + slab.height - slab_radius}) {
    const std::array<float, 3> seen = colour_at(image, x, y);
    EXPECT_LT(seen[0] - seen[1], 0.08F) << y;
  }
  const std::array<float, 3> below =
      colour_at(image, x, slab.y + slab.height + 8.0F);
  EXPECT_GT(below[0] - below[1], 0.5F);
}

TEST_F(GlassTest, AnElevatedClearSlabKeepsItsInteriorAndRoundCorners) {
  const auto raised = [](theme::Shape shape) {
    return slab_scene(
        slab_of(theme::Glass::clear(), shape).shadow(theme::Elevation::high));
  };
  const provider::Image rounded =
      raised(theme::Shape::rounded(theme::Radius{slab_radius}));
  const provider::Image square = raised(theme::Shape::rectangle());
  const float ground = luma_at(rounded, 8.0F, 8.0F);
  const float x = slab.x + (slab.width * 0.5F);
  for (const float y :
       {slab.y + (slab.height * 0.5F), slab.y + (slab.height * 0.7F),
        slab.y + slab.height - (slab_radius * 0.75F)}) {
    EXPECT_GE(luma_at(rounded, x, y), ground - 0.02F) << y;
  }
  for (const auto [cx, cy] :
       {std::array{slab.x - 2.0F, slab.y + slab.height + 2.0F},
        std::array{slab.x + slab.width + 1.0F, slab.y + slab.height + 2.0F}}) {
    const float soft = ground - luma_at(rounded, cx, cy);
    const float hard = ground - luma_at(square, cx, cy);
    EXPECT_GT(hard, 0.01F) << cx << "," << cy;
    EXPECT_LT(soft, hard * 0.6F) << cx << "," << cy;
  }
}

}
