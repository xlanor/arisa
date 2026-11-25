#include <springs_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_curve.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto SpringsPage::Develop::body(motion::TransitionContent content,
                                motion::TransitionPhase phase) const
    -> motion::TransitionContent {
  if (phase.is_identity()) {
    return content;
  }
  return content.grayscale(1.0F).brightness(0.3F).opacity(0.0F);
}

auto SpringsPage::Flip::body(motion::TransitionContent content,
                             motion::TransitionPhase phase) const
    -> motion::TransitionContent {
  if (phase.is_identity()) {
    return content;
  }
  return content
      .rotation_3d_effect(motion::Angle::degrees(phase.value() * 90.0), 0.0F,
                          1.0F, 0.0F)
      .hue_rotation(motion::Angle::degrees(120.0))
      .opacity(0.0F);
}

auto SpringsPage::Demos::card(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(ColourRole::surface_raised,
                  theme::RoundedRectangle(theme::radius::card));
}

auto SpringsPage::Demos::puck(float shift) -> Element {
  return components::ShapeView(theme::Capsule{})
      .fill(ColourRole::accent)
      .frame({.width = 28.0F, .height = 20.0F})
      .offset(shift, 0.0F);
}

auto SpringsPage::Demos::lane(std::string_view name, Element shown) -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Text(std::string{name})
          .font(theme::Font::caption)
          .frame(
              {.width = 110.0F, .alignment = composition::Alignment::leading}),
      components::ZStack(std::move(shown))
          .frame({.width = 220.0F,
                  .height = 24.0F,
                  .alignment = composition::Alignment::leading})
          .background(ColourRole::tertiary_system_fill, theme::Capsule{}));
}

auto SpringsPage::Demos::velocities(composition::StateHandle<bool> launched)
    -> Element {
  const float shift = launched.get() ? 190.0F : 0.0F;
  const std::array<std::pair<std::string_view, double>, 3> launches{
      std::pair{std::string_view{"Resting"}, 0.0},
      std::pair{std::string_view{"Thrown"}, 4.0},
      std::pair{std::string_view{"Pulled back"}, -3.0}};
  std::vector<Element> rows;
  rows.push_back(components::Button("Launch", [launched] {
                   launched.set(!launched.get());
                 }).id(std::string{launch_button}));
  for (const auto& [name, velocity] : launches) {
    rows.push_back(lane(
        name, puck(shift).animation(motion::Animation::interpolating_spring(
                                        motion::Spring::smooth(0.8), velocity),
                                    launched.get())));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      std::move(rows));
}

auto SpringsPage::Demos::curves(composition::StateHandle<bool> curved)
    -> Element {
  const float shift = curved.get() ? 190.0F : 0.0F;
  const std::array<std::pair<std::string_view, motion::UnitCurve>, 11> presets{
      std::pair{std::string_view{"Linear"}, motion::UnitCurve::linear},
      std::pair{std::string_view{"Ease in"}, motion::UnitCurve::ease_in},
      std::pair{std::string_view{"Ease out"}, motion::UnitCurve::ease_out},
      std::pair{std::string_view{"Ease in out"},
                motion::UnitCurve::ease_in_out},
      std::pair{std::string_view{"Circular in"},
                motion::UnitCurve::circular_ease_in},
      std::pair{std::string_view{"Circular out"},
                motion::UnitCurve::circular_ease_out},
      std::pair{std::string_view{"Circular in out"},
                motion::UnitCurve::circular_ease_in_out},
      std::pair{std::string_view{"Back"}, motion::UnitCurve::back_ease_out},
      std::pair{std::string_view{"Elastic"},
                motion::UnitCurve::elastic_ease_out},
      std::pair{std::string_view{"Bounce"}, motion::UnitCurve::bounce_ease_out},
      std::pair{std::string_view{"Inverse ease in"},
                motion::UnitCurve::ease_in.inverse()}};
  std::vector<Element> rows;
  rows.push_back(components::Button("Run", [curved] {
                   curved.set(!curved.get());
                 }).id(std::string{curves_button}));
  for (const auto& [name, curve] : presets) {
    rows.push_back(lane(
        name, puck(shift).animation(motion::Animation::timing_curve(curve, 0.9),
                                    curved.get())));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      std::move(rows));
}

auto SpringsPage::Demos::transitions(composition::StateHandle<bool> inserted)
    -> Element {
  std::vector<Element> tiles;
  tiles.push_back(components::Button("Toggle", [inserted] {
                    motion::with_animation(
                        motion::Animation::smooth(0.6),
                        [&inserted] { inserted.set(!inserted.get()); });
                  }).id(std::string{transitions_button}));
  if (inserted.get()) {
    tiles.push_back(
        components::Text("Develop")
            .padding(theme::space::md)
            .background(ColourRole::orange, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(motion::Transition::custom(Develop{})));
    tiles.push_back(
        components::Text("Flip")
            .padding(theme::space::md)
            .background(ColourRole::indigo, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(motion::Transition::custom(Flip{})));
    tiles.push_back(
        components::Text("Modifier")
            .padding(theme::space::md)
            .background(ColourRole::teal, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(
                motion::Transition::modifier(motion::TransitionContent{}
                                                 .blur(12.0F)
                                                 .saturation(0.0F)
                                                 .opacity(0.0F),
                                             motion::TransitionContent{})));
    tiles.push_back(components::Image::system_name("star")
                        .font(theme::Font::large_title)
                        .foreground_style(ColourRole::yellow)
                        .transition(motion::Transition::symbol_effect()));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      std::move(tiles));
}

auto SpringsPage::Demos::attached(composition::StateHandle<bool> shown)
    -> Element {
  std::vector<Element> tiles;
  tiles.push_back(components::Button("Toggle", [shown] {
                    shown.set(!shown.get());
                  }).id(std::string{attached_button}));
  if (shown.get()) {
    tiles.push_back(
        components::Text("Own animation")
            .padding(theme::space::md)
            .background(ColourRole::pink, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(motion::Transition::scale(0.4F)
                            .combined(motion::Transition::opacity())
                            .animation(motion::Animation::bouncy(0.6))));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md}, std::move(tiles));
}

auto SpringsPage::Demos::binding(composition::StateHandle<bool> bound)
    -> Element {
  const composition::Binding<bool> animated =
      composition::Binding<bool>{bound}.animation(motion::Animation::snappy());
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      controls::Toggle("Animated binding", animated)
          .id(std::string{binding_toggle}),
      lane("Follows", puck(bound.get() ? 190.0F : 0.0F)));
}

auto SpringsPage::Demos::gradient(composition::StateHandle<bool> warm)
    -> Element {
  const bool hot = warm.get();
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Button("Recolour",
                         [warm] {
                           motion::with_animation(
                               motion::Animation::smooth(0.8),
                               [&warm] { warm.set(!warm.get()); });
                         })
          .id(std::string{gradient_button}),
      components::ShapeView(theme::RoundedRectangle(16.0F))
          .fill(graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  hot ? ColourRole::orange : ColourRole::blue,
                  hot ? ColourRole::pink : ColourRole::mint}},
              .start_point = motion::UnitPoint::leading,
              .end_point = motion::UnitPoint::trailing})
          .frame({.width = 140.0F, .height = 56.0F}),
      components::Text("Gradient")
          .font(theme::Font::title)
          .foreground_style(graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  hot ? ColourRole::red : ColourRole::indigo,
                  hot ? ColourRole::yellow : ColourRole::teal}},
              .start_point = motion::UnitPoint::leading,
              .end_point = motion::UnitPoint::trailing}));
}

auto SpringsPage::Demos::body(composition::Context& context) const -> Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      card("Initial velocity", velocities(context.state(launched_key))),
      card("Unit curves", curves(context.state(curved_key))),
      card("Custom transitions", transitions(context.state(inserted_key))),
      card("Per-transition animation", attached(context.state(attached_key))),
      card("Binding animation", binding(context.state(bound_key))),
      card("Gradient fills", gradient(context.state(warm_key))));
}

auto SpringsPage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(Element{Demos{}}.padding(theme::space::lg))
            .navigation_title("Springs and transitions")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto SpringsPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Springs and transitions",
                            [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{SpringsPage{shown}}; });
}

}
