#include <motion_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <array>
#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::Capsule;
using theme::Circle;
using theme::ColourRole;
using theme::Elevation;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto MotionPage::operator==(const MotionPage& other) const -> bool {
  const auto address = [](const MeterRef& meter) -> const FrameMeter* {
    return meter.has_value() ? &meter->get() : nullptr;
  };
  return address(meter_) == address(other.meter_);
}

auto MotionPage::body(composition::Context& context) const -> Element {
  const auto play = context.state(playing);
  const auto on = context.state(toggled);
  const bool moved = play.get();
  const FrameMeter::Reading reading =
      meter_.has_value() ? context.read(meter_->get().reading())
                         : FrameMeter::Reading{};
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(
      VStack(alignment::leading, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          HStack(alignment::center, spacing(theme::space::lg),
                 frame({.max_width = infinity}))(
              Button(action([play] {
                       with_animation(Animation::spring(),
                                      [play] { play.set(!play.get()); });
                     }),
                     id(play_button))(moved ? "Reset" : "Play"),
              toggle(on), status(reading)),
          HStack(alignment::top, spacing(theme::space::xl), wrap,
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::top}))(
              playground(moved),
              VStack(alignment::leading, spacing(theme::space::sm),
                     frame({.width = 380.0F,
                            .alignment = composition::Alignment::leading}))(
                  card(moved)),
              VStack(alignment::leading, spacing(theme::space::xl))(
                  effects(moved), list(moved)),
              retargets(moved))));
}

auto MotionPage::lane(const Preset& preset, bool moved) -> Element {
  constexpr float track = 220.0F;
  constexpr float ball = 28.0F;
  return HStack(alignment::center, spacing(theme::space::sm))(
      Text(
          font::caption,
          frame({.width = 64.0F, .alignment = composition::Alignment::leading}),
          foreground_style(ColourRole::text_secondary))(
          std::string{preset.name}),
      ZStack(alignment::leading)(
          VStack(frame({.width = track, .height = ball}),
                 background(ColourRole::surface, Capsule()))(),
          VStack(frame({.width = ball, .height = ball}),
                 background(ColourRole::accent, Circle()),
                 offset(moved ? track - ball : 0.0F),
                 animation(preset.animation, moved))()));
}

auto MotionPage::playground(bool moved) -> Element {
  const std::array presets{
      Preset{.name = "smooth", .animation = Animation::smooth()},
      Preset{.name = "snappy", .animation = Animation::snappy()},
      Preset{.name = "bouncy", .animation = Animation::bouncy()},
      Preset{.name = "spring", .animation = Animation::spring()},
      Preset{.name = "ease", .animation = Animation::ease_in_out(0.6)}};
  std::vector<Element> lanes;
  lanes.push_back(Text(font::headline)("Springs"));
  for (const Preset& preset : presets) {
    lanes.push_back(lane(preset, moved));
  }
  return VStack(
      alignment::leading, spacing(theme::space::sm),
      frame({.width = 320.0F, .alignment = composition::Alignment::leading}))(
      std::move(lanes));
}

auto MotionPage::retargets(bool moved) -> Element {
  const std::array presets{
      Preset{.name = "blend 0",
             .animation = moved ? Animation::spring(outward_response, 1.0)
                                : Animation::spring(return_response, 1.0)},
      Preset{.name = "blend 0.6",
             .animation = moved ? Animation::spring(outward_response, 1.0)
                                : Animation::spring(return_response, 1.0,
                                                    return_blend)}};
  std::vector<Element> lanes;
  lanes.push_back(Text(font::headline)("Retarget"));
  for (const Preset& preset : presets) {
    lanes.push_back(lane(preset, moved));
  }
  return VStack(
      alignment::leading, spacing(theme::space::sm),
      frame({.width = 320.0F, .alignment = composition::Alignment::leading}),
      id(std::string{retarget_lanes}))(std::move(lanes));
}

auto MotionPage::card(bool open) -> Element {
  std::vector<Element> lines;
  lines.push_back(Text(font::headline)("with_animation"));
  if (open) {
    lines.push_back(Text(font::body, frame({.max_width = infinity}))(
        "Inserted rows fade in with the default opacity transition."));
  }
  return VStack(
      alignment::leading, spacing(theme::space::sm), padding(theme::space::lg),
      frame({.width = open ? 380.0F : 260.0F,
             .height = open ? 200.0F : 110.0F,
             .alignment = composition::Alignment::leading}),
      background(
          open ? ColourRole::surface_raised : ColourRole::surface,
          RoundedRectangle(open ? theme::radius::sheet : theme::radius::card,
                           RoundedCornerStyle::continuous)),
      border(ColourRole::separator, theme::Stroke::hairline()),
      shadow(open ? Elevation::high : Elevation::low))(std::move(lines));
}

auto MotionPage::list(bool grown) -> Element {
  return VStack(
      alignment::leading, spacing(theme::space::xs),
      frame({.width = 240.0F, .alignment = composition::Alignment::leading}))(
      Text(font::headline)("Transitions"),
      ForEach(
          std::views::iota(0, grown ? 5 : 2), [](int item) { return item; },
          [](int item) {
            return Text(
                font::callout, padding(theme::space::sm),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::control,
                                            RoundedCornerStyle::continuous)),
                shadow(Elevation::low),
                transition(Transition::slide().combined(
                    Transition::opacity())))(std::format("row {}", item + 1));
          }));
}

auto MotionPage::tile(std::string_view label, Element shape) -> Element {
  return VStack(alignment::center, spacing(theme::space::sm))(
      std::move(shape),
      Text(font::caption,
           foreground_style(ColourRole::text_secondary))(std::string{label}));
}

auto MotionPage::effects(bool on) -> Element {
  constexpr float side = 64.0F;
  const RoundedRectangle rounded{theme::radius::control,
                                 RoundedCornerStyle::continuous};
  return VStack(
      alignment::leading, spacing(theme::space::sm),
      frame({.width = 360.0F, .alignment = composition::Alignment::leading}))(
      Text(font::headline)("Effects"),
      HStack(alignment::top, spacing(theme::space::xl))(
          tile("scale_effect", VStack(frame({.width = side, .height = side}),
                                      background(ColourRole::accent, rounded),
                                      scale_effect(on ? 1.3F : 1.0F))()),
          tile("rotation_effect",
               VStack(frame({.width = side, .height = side}),
                      background(ColourRole::success, rounded),
                      rotation_effect(Angle::degrees(on ? 45.0 : 0.0)))()),
          tile("offset", VStack(frame({.width = side, .height = side}),
                                background(ColourRole::warning, rounded),
                                offset(0.0F, on ? -16.0F : 0.0F))())));
}

auto MotionPage::toggle(composition::StateHandle<bool> on) -> Element {
  constexpr float width = 52.0F;
  constexpr float height = 32.0F;
  constexpr float knob = 26.0F;
  constexpr float inset = 3.0F;
  const bool lit = on.get();
  return Button(action([on] { on.set(!on.get()); }),
                padding(theme::space::none),
                background(theme::Colour{0.0F, 0.0F, 0.0F, 0.0F}),
                id(toggle_button))(ZStack(alignment::leading)(
      VStack(
          frame({.width = width, .height = height}),
          background(lit ? ColourRole::success : ColourRole::gray, Capsule()),
          animation(Animation::snappy(), lit))(),
      VStack(frame({.width = knob, .height = knob}),
             background(ColourRole::on_accent, Circle()),
             shadow(Elevation::low), offset(lit ? width - knob - inset : inset),
             animation(Animation::snappy(), lit))()));
}

auto MotionPage::status(const FrameMeter::Reading& reading) -> Element {
  return Text(font::caption, foreground_style(ColourRole::text_secondary))(
      std::format("{} frames presented, {}", reading.presented,
                  reading.idle ? "idle" : "rendering"));
}

}
