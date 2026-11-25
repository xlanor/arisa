#include <effects_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::Circle;
using theme::ColourRole;
using theme::Elevation;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto EffectsPage::title(int index) -> std::string {
  constexpr std::array<std::string_view, card_count> names{"Aurora", "Harbour",
                                                           "Summit"};
  return std::string{names[static_cast<std::size_t>(index) % names.size()]};
}

auto EffectsPage::body(composition::Context& context) const -> Element {
  const auto chosen = context.state(selected);
  const auto hops = context.state(jumps);
  const auto count = context.state(tally);
  const Namespace::ID space = context.namespace_id(hero);
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(HStack(
      alignment::top, spacing(theme::space::xxl), wrap,
      frame({.max_width = infinity, .alignment = composition::Alignment::top}))(
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.width = stage_width,
                    .alignment = composition::Alignment::leading}))(
          Text(font::headline)("matched_geometry_effect"),
          stage(chosen, space)),
      VStack(alignment::leading, spacing(theme::space::xl))(
          focus_row(count), HStack(alignment::top, spacing(theme::space::xl))(
                                counter(count), jumper(hops)))));
}

auto EffectsPage::stage(composition::StateHandle<std::optional<int>> chosen,
                        Namespace::ID space) -> Element {
  constexpr float width = 200.0F;
  constexpr float height = 132.0F;
  std::vector<Element> row;
  for (int index = 0; index < card_count; ++index) {
    if (chosen.get() == index) {
      row.push_back(VStack(frame({.width = width, .height = height}),
                           id(std::format("effects-slot-{}", index)))());
    } else {
      row.push_back(card(index, chosen, space));
    }
  }
  std::vector<Element> layers;
  layers.push_back(
      HStack(alignment::top, spacing(theme::space::lg))(std::move(row)));
  if (chosen.get().has_value()) {
    layers.push_back(detail(*chosen.get(), chosen, space));
  }
  return ZStack(alignment::top_leading,
                frame({.width = stage_width,
                       .height = 280.0F,
                       .alignment = composition::Alignment::top_leading}))(
      std::move(layers));
}

auto EffectsPage::card(int index,
                       composition::StateHandle<std::optional<int>> chosen,
                       Namespace::ID space) -> Element {
  return Button(
      action([chosen, index] {
        with_animation(Animation::spring(0.45, 0.82),
                       [chosen, index] { chosen.set(index); });
      }),
      id(std::format("effects-card-{}", index)),
      frame({.width = 200.0F, .height = 132.0F}), padding(theme::space::lg),
      background(ColourRole::surface_raised,
                 RoundedRectangle(theme::radius::card,
                                  RoundedCornerStyle::continuous)),
      foreground_style(ColourRole::text_primary),
      border(ColourRole::separator, theme::Stroke::hairline()),
      shadow(Elevation::low), hover_effect(HoverEffect::lift.tilt()),
      matched_geometry_effect(index, space,
                              MatchedGeometryProperties::frame |
                                  MatchedGeometryProperties::corner_radius |
                                  MatchedGeometryProperties::colour,
                              UnitPoint::top_leading))(
      VStack(alignment::leading, spacing(theme::space::xs),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Text(font::headline)(title(index))));
}

auto EffectsPage::detail(int index,
                         composition::StateHandle<std::optional<int>> chosen,
                         Namespace::ID space) -> Element {
  return Button(
      action([chosen] {
        with_animation(Animation::spring(0.45, 0.82),
                       [chosen] { chosen.set(std::nullopt); });
      }),
      id(detail_card), frame({.width = stage_width, .height = 280.0F}),
      padding(theme::space::xl),
      background(ColourRole::accent,
                 RoundedRectangle(theme::radius::sheet,
                                  RoundedCornerStyle::continuous)),
      foreground_style(ColourRole::on_accent), shadow(Elevation::high),
      hover_effect(HoverEffect::highlight),
      focus_scope(space, FocusScopeBehavior::confined),
      matched_geometry_effect(index, space,
                              MatchedGeometryProperties::frame |
                                  MatchedGeometryProperties::corner_radius |
                                  MatchedGeometryProperties::colour,
                              UnitPoint::top_leading))(
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          Text(font::large_title)(title(index)),
          Text(font::body, frame({.max_width = infinity}))(
              "The card grows into this detail view from its presented frame, "
              "corner radius and colour. Press A to send it back.")));
}

auto EffectsPage::jumper(composition::StateHandle<int> hops) -> Element {
  return VStack(alignment::center, spacing(theme::space::sm),
                frame({.width = column_width}))(
      Text(font::headline)("keyframe_animator"),
      VStack(alignment::center, justify::end,
             frame({.width = 160.0F, .height = 150.0F}))(VStack(
          frame({.width = 56.0F, .height = 56.0F}),
          background(ColourRole::warning, Circle()),
          keyframe_animator(
              Hop{}, hops.get(),
              [](Element ball, const Hop& hop) {
                return std::move(ball)
                    .scale_effect(static_cast<float>(hop.scale),
                                  UnitPoint::bottom)
                    .offset(0.0F, static_cast<float>(-hop.lift))
                    .rotation_effect(hop.spin);
              },
              [](const Hop&) {
                return Keyframes{
                    KeyframeTrack(&Hop::scale, LinearKeyframe(0.8, 0.1),
                                  SpringKeyframe(1.15, 0.25, Spring::bouncy()),
                                  SpringKeyframe(1.0, Spring::bouncy())),
                    KeyframeTrack(&Hop::lift, LinearKeyframe(0.0, 0.1),
                                  SpringKeyframe(70.0, 0.3, Spring::snappy()),
                                  CubicKeyframe(10.0, 0.2),
                                  SpringKeyframe(0.0, Spring::bouncy())),
                    KeyframeTrack(&Hop::spin,
                                  LinearKeyframe(Angle::degrees(0.0), 0.15),
                                  LinearKeyframe(Angle::degrees(360.0), 0.5,
                                                 UnitCurve::back_ease_out),
                                  MoveKeyframe(Angle::degrees(0.0)))};
              }))()),
      Button(action([hops] { hops.set(hops.get() + 1); }),
             id(jump_button))("Jump"));
}

auto EffectsPage::counter(composition::StateHandle<int> count) -> Element {
  return VStack(alignment::center, spacing(theme::space::sm),
                frame({.width = column_width}))(
      Text(font::headline)("numeric_text"),
      Text(font::large_title,
           content_transition(ContentTransition::numeric_text()))(
          std::format("{}", count.get())),
      HStack(alignment::center, spacing(theme::space::sm))(
          Button(action([count] {
            with_animation(Animation::snappy(),
                           [count] { count.set(count.get() - 1); });
          }))("-"),
          Button(action([count] {
                   with_animation(Animation::snappy(),
                                  [count] { count.set(count.get() + 1); });
                 }),
                 id(increment_button))("+")));
}

auto EffectsPage::tile(std::string_view label, HoverEffect effect,
                       composition::StateHandle<int> count) -> Element {
  return Button(action([count] {
                  with_animation(Animation::snappy(),
                                 [count] { count.set(count.get() + 1); });
                }),
                frame({.width = 112.0F, .height = 80.0F}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                foreground_style(ColourRole::text_primary),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(Elevation::low),
                hover_effect(effect))(Text(font::callout)(std::string{label}));
}

auto EffectsPage::focus_row(composition::StateHandle<int> count) -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm))(
      Text(font::headline)("hover_effect"),
      HStack(alignment::center, spacing(theme::space::lg))(
          tile("lift", HoverEffect::lift, count),
          tile("highlight", HoverEffect::highlight, count),
          tile("tilt", HoverEffect::lift.tilt(), count)));
}

}
