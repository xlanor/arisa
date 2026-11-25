#include <gallery_wallpaper.hpp>
#include <launch_screen.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/space.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace arisa::examples {

auto GalleryWallpaper::points() -> std::vector<composition::Point> {
  return {{0.0F, 0.0F},  {0.55F, 0.0F},  {1.0F, 0.0F},
          {0.0F, 0.45F}, {0.42F, 0.58F}, {1.0F, 0.4F},
          {0.0F, 1.0F},  {0.6F, 1.0F},   {1.0F, 1.0F}};
}

auto GalleryWallpaper::band_points(bool drifting)
    -> std::vector<composition::Point> {
  if (drifting) {
    return {{0.0F, 0.0F}, {0.36F, 0.0F}, {0.70F, 0.0F}, {1.0F, 0.0F},
            {0.0F, 1.0F}, {0.14F, 1.0F}, {0.44F, 1.0F}, {1.0F, 1.0F}};
  }
  return {{0.0F, 0.0F}, {0.30F, 0.0F}, {0.62F, 0.0F}, {1.0F, 0.0F},
          {0.0F, 1.0F}, {0.18F, 1.0F}, {0.48F, 1.0F}, {1.0F, 1.0F}};
}

auto GalleryWallpaper::launch(composition::StateHandle<bool> drift)
    -> composition::Element {
  const auto& tones = LaunchScreen::wallpaper;
  std::vector<theme::ColourValue> colours;
  for (const std::size_t index : {0U, 1U, 2U, 3U, 1U, 2U, 4U, 5U}) {
    colours.emplace_back(theme::Colour::hex(tones[index]));
  }
  return composition::Element{
      components::MeshGradient(4, 2, band_points(drift.get()),
                               std::move(colours), theme::Colour::hex(tones[5]),
                               true, graphics::Gradient::ColorSpace::perceptual)
          .grain(LaunchScreen::grain)}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .on_appear([drift] {
        motion::with_animation(
            motion::Animation::ease_in_out(LaunchScreen::drift_period)
                .repeat_forever(true)
                .preferred_frame_rate(LaunchScreen::drift_frame_rate),
            [drift] { drift.set(true); });
      });
}

auto GalleryWallpaper::busy() -> composition::Element {
  constexpr std::array<std::uint32_t, 6> stripes{0xFF3B30, 0xFFCC00, 0x34C759,
                                                 0x007AFF, 0xAF52DE, 0x111111};
  std::vector<composition::Element> rows;
  for (int line = 0; line < 4; ++line) {
    std::vector<composition::Element> bars;
    for (int column = 0; column < 64; ++column) {
      const std::uint32_t colour =
          stripes[static_cast<std::size_t>(column + line) % stripes.size()];
      bars.push_back(composition::Element{components::VStack()}
                         .frame({.width = 14.0F, .height = 22.0F})
                         .background(theme::Colour::hex(colour)));
    }
    rows.push_back(components::HStack(
        components::HStackOptions{.spacing = theme::Space{4.0F}},
        std::move(bars)));
    rows.push_back(components::Text(std::string{
        "The quick brown fox jumps over the lazy dog 0123456789 The quick "
        "brown fox jumps over the lazy dog 0123456789 The quick brown fox"}));
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::Space{2.0F}},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading})
      .clip_shape(theme::Shape::rectangle());
}

auto GalleryWallpaper::window(bool dark) -> composition::Element {
  std::vector<theme::ColourValue> values;
  for (const std::uint32_t colour : dark ? window_dark : window_light) {
    values.emplace_back(theme::Colour::hex(colour));
  }
  return composition::Element{
      components::MeshGradient(3, 3, points(), std::move(values),
                               theme::Colour::hex(dark ? 0x1E1D32 : 0xEBDCD1))
          .grain(0.015F)}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity});
}

auto GalleryWallpaper::body(composition::Context& context) const
    -> composition::Element {
  switch (look_) {
    case Look::launch:
      return launch(context.state(drifted));
    case Look::busy:
      return busy();
    case Look::window:
      break;
  }
  const theme::ColorScheme scheme =
      context.environment(components::Environment::colour_scheme)
          .value_or(
              context.environment(components::Environment::theme).scheme());
  return window(scheme == theme::ColorScheme::dark);
}

}
