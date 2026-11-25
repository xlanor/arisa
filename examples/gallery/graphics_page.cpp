#include <graphics_page.hpp>

#include <gallery_shaders.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/canvas.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/keyframe_animator.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/keyframe_track.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/spring_keyframe.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/runtime/image_renderer.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <stb_image_write.h>

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

const composition::StateKey<std::string> GraphicsPage::exported{
    "graphics-exported", std::string{}};

const composition::StateKey<std::shared_ptr<const provider::Image>>
    GraphicsPage::rendered{"graphics-rendered", nullptr};

auto GraphicsPage::Clock::body(composition::Context& context) const -> Element {
  const double seconds =
      std::chrono::duration<double>(context.frame_time()).count();
  if (running_) {
    context.request_frame();
  }
  return content_(running_ ? seconds : 0.0);
}

auto GraphicsPage::card(std::string_view title, Element content) -> Element {
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

auto GraphicsPage::tile(std::uint32_t rgb, std::string_view name) -> Element {
  return components::ZStack(
             components::ZStackOptions{
                 .alignment = composition::Alignment::bottom_leading},
             components::ShapeView(theme::RoundedRectangle(12.0F))
                 .fill(graphics::LinearGradient{
                     .gradient =
                         graphics::Gradient{std::vector<theme::ColourValue>{
                             theme::Colour::hex(rgb),
                             theme::Colour::hex(0x1C1C1E)}},
                     .start_point = motion::UnitPoint::top,
                     .end_point = motion::UnitPoint::bottom}),
             components::Text(std::string{name})
                 .font(theme::Font::footnote)
                 .foreground_style(theme::Colour::hex(0xFFFFFF))
                 .padding(theme::space::sm))
      .frame({.width = 96.0F, .height = 128.0F});
}

auto GraphicsPage::badge() -> Element {
  return components::VStack(
             components::VStackOptions{.spacing = theme::space::xs},
             components::Text("arisa").font(theme::Font::title2),
             components::Text("Rendered offscreen").font(theme::Font::footnote))
      .padding(theme::space::lg)
      .foreground_style(theme::Colour::hex(0xFFFFFF))
      .background(theme::Colour::hex(0x5856D6), theme::RoundedRectangle(16.0F));
}

auto GraphicsPage::unavailable() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      tile(0xFF9500, "Installed"),
      tile(0x34C759, "Archived").grayscale(1.0).opacity(0.6F),
      tile(0x007AFF, "Locked")
          .saturation(0.0)
          .brightness(-0.2)
          .id(std::string{unavailable_tile}));
}

auto GraphicsPage::adjustments() -> Element {
  const auto swatch = [](std::string_view name, Element shown) {
    return Element{components::VStack(
        components::VStackOptions{.spacing = theme::space::xs},
        std::move(shown),
        components::Text(std::string{name}).font(theme::Font::caption))};
  };
  const auto source = [] {
    return Element{components::VStack()
                       .frame({.width = 56.0F, .height = 56.0F})
                       .background(theme::Colour::hex(0xCC6633),
                                   theme::RoundedRectangle(10.0F))};
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      swatch("Source", source()),
      swatch("Brightness", source().brightness(0.2)),
      swatch("Contrast", source().contrast(1.5)),
      swatch("Saturation", source().saturation(0.5)),
      swatch("Hue", source().hue_rotation(motion::Angle::degrees(90.0))),
      swatch("Invert", source().color_invert()),
      swatch("Multiply",
             source().color_multiply(theme::Colour{0.5F, 1.0F, 0.5F, 1.0F})),
      swatch("Luminance", source().luminance_to_alpha()));
}

auto GraphicsPage::blur(composition::StateHandle<bool> on) -> Element {
  const bool active = on.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      controls::Toggle("Blurred backdrop", on).id(std::string{blur_toggle}),
      components::ZStack(
          components::HStack(
              components::HStackOptions{.spacing = theme::space::md},
              tile(0xFF2D55, "Aurora"), tile(0x5AC8FA, "Harbour"),
              tile(0xFFCC00, "Summit"))
              .blur(active ? 12.0F : 0.0F),
          components::Text("Sheet")
              .font(theme::Font::headline)
              .padding(theme::space::lg)
              .glass_effect()));
}

auto GraphicsPage::masks() -> Element {
  const auto fade = [] {
    return Element{
        components::ShapeView(theme::Shape::rectangle())
            .fill(graphics::LinearGradient{
                .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                    theme::Colour{0.0F, 0.0F, 0.0F, 1.0F},
                    theme::Colour{0.0F, 0.0F, 0.0F, 0.0F}}},
                .start_point = motion::UnitPoint::leading,
                .end_point = motion::UnitPoint::trailing})};
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::HStack(components::HStackOptions{.spacing = theme::space::sm},
                         tile(0xAF52DE, "One"), tile(0x30B0C7, "Two"),
                         tile(0xFF9F0A, "Three"))
          .mask(fade()),
      components::Text("Masked")
          .font(theme::Font::large_title)
          .mask(components::Image::system_name("star.fill")
                    .resizable()
                    .frame({.width = 48.0F, .height = 48.0F})));
}

auto GraphicsPage::blends() -> Element {
  constexpr std::array<std::pair<graphics::BlendMode, std::string_view>, 8>
      modes{{{graphics::BlendMode::multiply, "Multiply"},
             {graphics::BlendMode::screen, "Screen"},
             {graphics::BlendMode::overlay, "Overlay"},
             {graphics::BlendMode::difference, "Difference"},
             {graphics::BlendMode::color_burn, "Burn"},
             {graphics::BlendMode::soft_light, "Soft light"},
             {graphics::BlendMode::hue, "Hue"},
             {graphics::BlendMode::luminosity, "Luminosity"}}};
  std::vector<Element> swatches;
  for (const auto& [mode, name] : modes) {
    swatches.push_back(components::VStack(
        components::VStackOptions{.spacing = theme::space::xs},
        components::ZStack(
            components::VStack()
                .frame({.width = 64.0F, .height = 64.0F})
                .background(theme::Colour::hex(0x3399E6),
                            theme::RoundedRectangle(10.0F)),
            components::VStack()
                .frame({.width = 40.0F, .height = 40.0F})
                .background(theme::Colour::hex(0xCC6633), theme::Circle())
                .blend_mode(mode)),
        components::Text(std::string{name}).font(theme::Font::caption)));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      std::move(swatches));
}

auto GraphicsPage::flip(composition::StateHandle<bool> turned) -> Element {
  const double target = turned.get() ? 180.0 : 0.0;
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg},
      tile(0x5856D6, "Card")
          .keyframe_animator(
              Turn{}, turned.get(),
              [](Element card, const Turn& turn) {
                return std::move(card).rotation_3d_effect(
                    motion::Angle::degrees(turn.degrees),
                    {.x = 0.0F, .y = 1.0F, .z = 0.0F});
              },
              [target](const Turn&) {
                return motion::Keyframes{motion::KeyframeTrack(
                    &Turn::degrees,
                    motion::SpringKeyframe(target, 0.6,
                                           motion::Spring::smooth()))};
              }),
      components::Button("Flip", [turned] { turned.set(!turned.get()); })
          .button_style(components::ButtonStyle::bordered())
          .id(std::string{flip_button}));
}

auto GraphicsPage::waveform(composition::StateHandle<bool> running) -> Element {
  return GraphicsPage::Clock{
      running.get(), [](double seconds) {
        return Element{
            components::Canvas([seconds](components::GraphicsContext& context,
                                         composition::Size size) {
              graphics::Path wave;
              constexpr int samples = 96;
              for (int index = 0; index <= samples; ++index) {
                const float x = size.width * static_cast<float>(index) /
                                static_cast<float>(samples);
                const double phase =
                    (static_cast<double>(index) * 0.18) + (seconds * 6.0);
                const float y = (size.height * 0.5F) +
                                (static_cast<float>(std::sin(phase) *
                                                    std::cos(phase * 0.21)) *
                                 size.height * 0.4F);
                if (index == 0) {
                  wave.move(composition::Point{.x = x, .y = y});
                } else {
                  wave.add_line(composition::Point{.x = x, .y = y});
                }
              }
              context.stroke(wave, graphics::ShapeStyle{ColourRole::accent},
                             3.0F);
              context.draw(components::Text(std::format("{:.1f} s", seconds)),
                           composition::Point{.x = 8.0F, .y = 8.0F},
                           motion::UnitPoint::top_leading);
            }).frame({.width = 320.0F, .height = 96.0F})};
      }};
}

auto GraphicsPage::ripple(composition::StateHandle<bool> running) -> Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      controls::Toggle("Animate", running).id(std::string{play_button}),
      components::HStack(
          components::HStackOptions{.spacing = theme::space::lg},
          waveform(running),
          GraphicsPage::Clock{
              running.get(),
              [](double seconds) {
                return Element{
                    tile(0x34C759, "Ripple")
                        .distortion_effect(
                            graphics::Shader{
                                GalleryShaders::ripple(),
                                {static_cast<float>(seconds), 4.0F}},
                            composition::Size{.width = 4.0F, .height = 4.0F})};
              }},
          tile(0xFF9500, "Stripes")
              .color_effect(
                  graphics::Shader{GalleryShaders::stripes(), {12.0F}})));
}

auto GraphicsPage::saved(const provider::Image& image) -> std::string {
  const int written = stbi_write_png(
      std::string{export_path}.c_str(), static_cast<int>(image.width()),
      static_cast<int>(image.height()), 4, image.pixels().data(),
      static_cast<int>(image.width() * 4));
  return written != 0 ? std::format("Saved {} ({}x{})", export_path,
                                    image.width(), image.height())
                      : std::string{"Could not save"};
}

auto GraphicsPage::renderer(
    composition::StateHandle<std::string> status,
    composition::StateHandle<std::shared_ptr<const provider::Image>> image)
    -> Element {
  std::vector<Element> shown;
  shown.push_back(
      components::Button("Export",
                         [status, image] {
                           runtime::ImageRenderer render{badge()};
                           render.set_scale(2.0F);
                           auto made = render.image();
                           if (!made.has_value()) {
                             status.set("Could not render");
                             return;
                           }
                           status.set(saved(*made));
                           image.set(std::make_shared<const provider::Image>(
                               std::move(*made)));
                         })
          .button_style(components::ButtonStyle::bordered())
          .id(std::string{export_button}));
  if (image.get() != nullptr) {
    shown.push_back(components::Image(image.get(), 2.0F));
  }
  if (!status.get().empty()) {
    shown.push_back(components::Text(status.get())
                        .font(theme::Font::footnote)
                        .foreground_style(ColourRole::text_secondary));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg}, std::move(shown));
}

auto GraphicsPage::edges() -> Element {
  std::vector<Element> rows;
  for (int index = 0; index < 12; ++index) {
    rows.push_back(components::Text(std::format("Row {}", index + 1))
                       .padding(theme::space::sm)
                       .frame({.max_width = infinity,
                               .alignment = composition::Alignment::leading}));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg},
      components::ScrollView(components::VStack(std::move(rows)))
          .scroll_edge_effect_style(composition::ScrollEdgeEffectStyle::soft,
                                    theme::Edge::vertical)
          .frame({.width = 200.0F, .height = 160.0F})
          .background(ColourRole::background, theme::RoundedRectangle(12.0F)),
      tile(0xFF2D55, "Hero")
          .background_extension_effect()
          .padding(theme::space::xl));
}

auto GraphicsPage::body(composition::Context& context) const -> Element {
  const auto turned = context.state(flipped);
  const auto running = context.state(playing);
  const auto on = context.state(blurred);
  const auto status = context.state(exported);
  const auto image = context.state(rendered);
  const auto shown_flag = shown_;
  return presentation::NavigationStack([turned, running, on, status, image,
                                        shown_flag] {
    return Element{
        components::ScrollView(
            components::VStack(
                components::VStackOptions{
                    .alignment = composition::HorizontalAlignment::leading,
                    .spacing = theme::space::lg},
                card("Unavailable games", unavailable()),
                card("Colour adjustments", adjustments()),
                card("Blur", blur(on)), card("Masks", masks()),
                card("Blend modes", blends()), card("3D flip", flip(turned)),
                card("Canvas and shaders", ripple(running)),
                card("Render to image", renderer(status, image)),
                card("Scroll edge and extension", edges()))
                .padding(theme::space::lg))
            .navigation_title("Visual effects")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto GraphicsPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Visual effects", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{GraphicsPage{shown}}; });
}

}
