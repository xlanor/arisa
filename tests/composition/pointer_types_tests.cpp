#include <arisa/components/text.hpp>
#include <arisa/composition/content_hover_effect.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/empty_hover_effect.hpp>
#include <arisa/composition/frame_resize_direction.hpp>
#include <arisa/composition/frame_resize_directions.hpp>
#include <arisa/composition/frame_resize_position.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/group_hover_effect.hpp>
#include <arisa/composition/horizontal_direction.hpp>
#include <arisa/composition/horizontal_directions.hpp>
#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_effect_phase_override.hpp>
#include <arisa/composition/hover_phase.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/vertical_direction.hpp>
#include <arisa/composition/vertical_directions.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/motion/hover_effect.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace arisa::composition {

class PointerTypesTest : public ::testing::Test {
 protected:
  static constexpr Namespace cards{"pointer-types-cards"};

  class Wobble {
   public:
    auto body(CustomHoverEffect content) const -> CustomHoverEffect {
      return content.hover_effect(motion::HoverEffect::lift)
          .hover_effect(motion::HoverEffect::highlight);
    }
  };
};

TEST_F(PointerTypesTest, DirectionSetsCombineAndEnumerate) {
  EXPECT_EQ(all_horizontal_directions.size(), 2U);
  EXPECT_EQ(all_vertical_directions.size(), 2U);
  EXPECT_EQ(all_frame_resize_positions.size(), 8U);
  EXPECT_EQ(all_frame_resize_directions.size(), 2U);
  EXPECT_EQ(HorizontalDirections::leading | HorizontalDirections::trailing,
            HorizontalDirections::all);
  EXPECT_EQ(HorizontalDirections{HorizontalDirection::trailing},
            HorizontalDirections::trailing);
  EXPECT_TRUE(HorizontalDirections::all.contains(HorizontalDirection::leading));
  EXPECT_FALSE(
      HorizontalDirections::leading.contains(HorizontalDirection::trailing));
  EXPECT_EQ(VerticalDirections{VerticalDirection::up}, VerticalDirections::up);
  EXPECT_EQ(VerticalDirections::up | VerticalDirections::down,
            VerticalDirections::all);
  EXPECT_FALSE(VerticalDirections::down.contains(VerticalDirection::up));
  EXPECT_EQ(FrameResizeDirections{FrameResizeDirection::inward} |
                FrameResizeDirections::outward,
            FrameResizeDirections::all);
  EXPECT_TRUE(FrameResizeDirections{}.empty());
}

TEST_F(PointerTypesTest, PointerStylesCarryTheirArguments) {
  EXPECT_EQ(PointerStyle{}, PointerStyle::default_);
  EXPECT_NE(PointerStyle::link, PointerStyle::default_);
  for (const PointerStyle& style :
       {PointerStyle::horizontal_text, PointerStyle::vertical_text,
        PointerStyle::rect_selection, PointerStyle::grab_idle,
        PointerStyle::grab_active, PointerStyle::link, PointerStyle::zoom_in,
        PointerStyle::zoom_out}) {
    EXPECT_NE(style.kind(), PointerStyle::Kind::automatic);
  }
  const PointerStyle column =
      PointerStyle::column_resize(HorizontalDirections::leading);
  EXPECT_EQ(column.kind(), PointerStyle::Kind::column_resize);
  EXPECT_EQ(column.horizontal(), HorizontalDirections::leading);
  EXPECT_EQ(PointerStyle::column_resize().horizontal(),
            HorizontalDirections::all);
  const PointerStyle row = PointerStyle::row_resize(VerticalDirections::down);
  EXPECT_EQ(row.vertical(), VerticalDirections::down);
  EXPECT_EQ(PointerStyle::row_resize().vertical(), VerticalDirections::all);
  const PointerStyle corner = PointerStyle::frame_resize(
      FrameResizePosition::top_trailing, FrameResizeDirections::outward);
  EXPECT_EQ(corner.kind(), PointerStyle::Kind::frame_resize);
  EXPECT_EQ(corner.position(), FrameResizePosition::top_trailing);
  EXPECT_EQ(corner.frame_directions(), FrameResizeDirections::outward);
  EXPECT_EQ(
      PointerStyle::frame_resize(FrameResizePosition::top).frame_directions(),
      FrameResizeDirections::all);
  const PointerStyle pictured = PointerStyle::image(
      [] { return Element{components::Text("x")}; }, motion::UnitPoint::center);
  EXPECT_EQ(pictured.kind(), PointerStyle::Kind::image);
  EXPECT_EQ(pictured.hot_spot(), motion::UnitPoint::center);
  ASSERT_NE(pictured.built(), nullptr);
  EXPECT_EQ(pictured, pictured);
}

TEST_F(PointerTypesTest, GestureInputKindsAreAnOptionSet) {
  EXPECT_TRUE(GestureInputKinds::all.contains(GestureInputKinds::pointer));
  EXPECT_TRUE(GestureInputKinds::all.contains(
      GestureInputKinds::direct_touch | GestureInputKinds::indirect_touch));
  EXPECT_FALSE(
      GestureInputKinds::direct_touch.contains(GestureInputKinds::pointer));
  EXPECT_TRUE(GestureInputKinds{}.empty());
}

TEST_F(PointerTypesTest, HoverPhaseCarriesTheLocation) {
  const HoverPhase active = HoverPhase::active(Point{.x = 3.0F, .y = 4.0F});
  EXPECT_TRUE(active.is_active());
  EXPECT_EQ(active.location(), (Point{.x = 3.0F, .y = 4.0F}));
  EXPECT_FALSE(HoverPhase::ended.is_active());
  EXPECT_NE(active, HoverPhase::ended);
}

TEST_F(PointerTypesTest, HoverGroupsMatchByNamespaceAndId) {
  const HoverEffectGroup plain{cards.id()};
  const HoverEffectGroup named{std::string{"row"}, cards.id()};
  EXPECT_FALSE(plain.same_group(named));
  EXPECT_TRUE(named.same_group(
      named.behavior(HoverEffectGroup::Behavior::follows_group)));
  EXPECT_NE(named, named.behavior(HoverEffectGroup::Behavior::ignores_group));
  EXPECT_TRUE(HoverEffectGroup::Behavior::preserves_group.activates());
  EXPECT_FALSE(HoverEffectGroup::Behavior::follows_group.activates());
  EXPECT_FALSE(HoverEffectGroup::Behavior::ignores_group.follows());
  EXPECT_TRUE(HoverEffectGroup::system_overlays().same_group(
      HoverEffectGroup::system_overlays()));
}

TEST_F(PointerTypesTest, PhaseOverridesApplyToTheNaturalPhase) {
  EXPECT_TRUE(HoverEffectPhaseOverride::active().applied(false));
  EXPECT_FALSE(HoverEffectPhaseOverride::inactive().applied(true));
  const auto toggled = HoverEffectPhaseOverride::toggled(3);
  EXPECT_TRUE(toggled.trigger().has_value());
  EXPECT_FALSE(toggled.temporary());
  EXPECT_TRUE(HoverEffectPhaseOverride::active_temporarily(1).temporary());
  EXPECT_TRUE(HoverEffectPhaseOverride::inactive_temporarily(1).temporary());
  EXPECT_TRUE(HoverEffectPhaseOverride::toggled_temporarily(1).temporary());
  EXPECT_NE(HoverEffectPhaseOverride::toggled(3),
            HoverEffectPhaseOverride::toggled(4));
}

TEST_F(PointerTypesTest, CustomHoverEffectsComposeLayers) {
  const CustomHoverEffect lift = motion::HoverEffect::lift;
  ASSERT_EQ(lift.layers().size(), 1U);
  EXPECT_TRUE(CustomHoverEffect::empty().is_empty());
  EXPECT_TRUE(EmptyHoverEffect{}.is_empty());
  const HoverEffectGroup group{cards.id()};
  const CustomHoverEffect combined =
      lift.hover_effect(motion::HoverEffect::highlight, group, true);
  ASSERT_EQ(combined.layers().size(), 2U);
  EXPECT_FALSE(combined.layers()[0].group.has_value());
  EXPECT_EQ(combined.layers()[1].group, group);
  EXPECT_TRUE(lift.hover_effect_disabled().is_empty());
  EXPECT_FALSE(lift.hover_effect_disabled(false).is_empty());
  const CustomHoverEffect grouped = lift.hover_effect_group(group);
  EXPECT_EQ(grouped.layers()[0].group, group);
  EXPECT_FALSE(
      grouped.hover_effect_group(std::nullopt).layers()[0].group.has_value());
  const CustomHoverEffect named = CustomHoverEffect::empty().hover_effect_group(
      std::string{"cell"}, cards.id(),
      HoverEffectGroup::Behavior::follows_group);
  ASSERT_EQ(named.layers().size(), 1U);
  EXPECT_EQ(named.layers()[0].kind, CustomHoverEffect::Layer::Kind::group);
  EXPECT_TRUE(named.is_empty());
  const CustomHoverEffect phased =
      lift.hover_effect_phase_override(HoverEffectPhaseOverride::active());
  EXPECT_EQ(phased.layers()[0].phase, HoverEffectPhaseOverride::active());
  const CustomHoverEffect closure = lift.hover_effect(
      [](VisualEffect content, bool active, const GeometryProxy&) {
        return content.scale_effect(active ? 1.2F : 1.0F);
      });
  ASSERT_EQ(closure.layers().size(), 2U);
  EXPECT_EQ(closure.layers()[1].kind, CustomHoverEffect::Layer::Kind::content);
  const CustomHoverEffect wobble = Wobble{};
  EXPECT_EQ(wobble.layers().size(), 2U);
  const CustomHoverEffect content = ContentHoverEffect{
      group, false,
      [](VisualEffect value, bool, const GeometryProxy&) { return value; }};
  ASSERT_EQ(content.layers().size(), 1U);
  EXPECT_FALSE(content.layers()[0].enabled);
  EXPECT_EQ(content.layers()[0].group, group);
  const CustomHoverEffect static_group = GroupHoverEffect{group};
  EXPECT_EQ(static_group.layers()[0].group, group);
  const CustomHoverEffect automatic = motion::HoverEffect::automatic;
  const CustomHoverEffect resolved =
      automatic.resolved(CustomHoverEffect{motion::HoverEffect::highlight});
  EXPECT_EQ(resolved.layers()[0].system, motion::HoverEffect::highlight);
  EXPECT_EQ(automatic.resolved(std::nullopt), automatic);
}

TEST_F(PointerTypesTest, ElementModifiersRecordPointerDeclarations) {
  const HoverEffectGroup group{cards.id()};
  Element element =
      Element{components::Text("x")}
          .pointer_style(PointerStyle::link)
          .pointer_visibility(Visibility::hidden)
          .on_hover([](bool) {})
          .on_continuous_hover([](HoverPhase) {})
          .hover_effect(motion::HoverEffect::lift, group)
          .hover_effect_group()
          .content_shape(ContentShapeKinds::hover_effect,
                         theme::Shape::capsule())
          .hover_effect_disabled()
          .default_hover_effect(CustomHoverEffect{motion::HoverEffect::lift})
          .list_row_hover_effect_disabled();
  EXPECT_EQ(element.interactions().pointer_style, PointerStyle::link);
  EXPECT_EQ(element.interactions().pointer_visibility, Visibility::hidden);
  EXPECT_NE(element.interactions().hover, nullptr);
  EXPECT_NE(element.interactions().continuous_hover, nullptr);
  ASSERT_TRUE(element.modifiers().hover.has_value());
  EXPECT_EQ(element.modifiers().hover->layers()[0].group, group);
  ASSERT_TRUE(element.interactions().hover_container.has_value());
  EXPECT_EQ(element.interactions().hover_container->kind,
            Interactions::HoverContainer::Kind::implicit);
  EXPECT_EQ(element.interactions().hover_shape, theme::Shape::capsule());
  Element nulled = Element{components::Text("y")}.pointer_style(std::nullopt);
  EXPECT_EQ(nulled.interactions().pointer_style, PointerStyle::default_);
  Element named = Element{components::Text("z")}.hover_effect_group(
      std::string{"row"}, cards.id());
  EXPECT_EQ(named.interactions().hover_container->kind,
            Interactions::HoverContainer::Kind::named);
  Element cleared =
      Element{components::Text("w")}.hover_effect_group(std::nullopt);
  EXPECT_EQ(cleared.interactions().hover_container->kind,
            Interactions::HoverContainer::Kind::cleared);
  Element closure = Element{components::Text("v")}.hover_effect(
      [](VisualEffect content, bool, const GeometryProxy&) { return content; });
  EXPECT_EQ(closure.modifiers().hover->layers()[0].kind,
            CustomHoverEffect::Layer::Kind::content);
  EXPECT_FALSE(Element{components::Text("u")}
                   .hover_effect(motion::HoverEffect::lift, false)
                   .modifiers()
                   .hover.has_value());
  EXPECT_TRUE(HoverEffectEnvironment::is_hover_effect_enabled.fallback());
}

}
