#include <gpu_stage_screen.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/container_background_placement.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

GpuStageScreen::GpuStageScreen(StageControl& control) noexcept
    : control_(control) {}

auto GpuStageScreen::modes() -> composition::Element {
  return components::Group(
      components::Text("Plasma").tag(StageControl::Mode::plasma),
      components::Text("Ripples").tag(StageControl::Mode::ripples),
      components::Text("Tunnel").tag(StageControl::Mode::tunnel));
}

auto GpuStageScreen::matrices() -> composition::Element {
  return components::Group(
      components::Text("601").tag(provider::ColourMatrix::bt601),
      components::Text("709").tag(provider::ColourMatrix::bt709),
      components::Text("2020").tag(provider::ColourMatrix::bt2020));
}

auto GpuStageScreen::ranges() -> composition::Element {
  return components::Group(
      components::Text("Limited").tag(provider::ColourRange::limited),
      components::Text("Full").tag(provider::ColourRange::full));
}

auto GpuStageScreen::decoding(composition::Context& context,
                              StageControl& control) -> composition::Element {
  const auto declared_matrix = context.state(matrix);
  const auto declared_range = context.state(range);
  const StageControl::VideoFormat format = context.read(control.shown_format());
  const bool planar = format == StageControl::VideoFormat::nv12;
  const bool decoded = format == StageControl::VideoFormat::decoder;
  const bool under = context.read(control.shown_under());
  std::vector<composition::Element> rows;
  rows.push_back(
      components::Button(under ? "In card" : "Under UI",
                         [&control, under] { control.set_under(!under); })
          .button_style(components::ButtonStyle::glass())
          .id(under_button));
  rows.push_back(
      components::Button(planar ? "RGBA" : "NV12",
                         [&control, planar] {
                           control.set_video_format(
                               planar ? StageControl::VideoFormat::rgba
                                      : StageControl::VideoFormat::nv12);
                         })
          .button_style(components::ButtonStyle::glass())
          .id(format_button));
  if (control.decoder_offered() && !decoded) {
    rows.push_back(components::Button("Decoder",
                                      [&control] {
                                        control.set_video_format(
                                            StageControl::VideoFormat::decoder);
                                      })
                       .button_style(components::ButtonStyle::glass())
                       .id(decoder_button));
  }
  if (decoded) {
    rows.push_back(composition::Element{DecoderLine{control}});
  }
  const components::VStackOptions options{
      .alignment = components::HorizontalAlignment::leading,
      .spacing = theme::space::md};
  if (!planar) {
    return components::VStack(options, std::move(rows));
  }
  rows.push_back(
      controls::Picker("Matrix", declared_matrix, [] { return matrices(); })
          .picker_style(controls::SegmentedPickerStyle{})
          .id(matrix_picker));
  rows.push_back(
      controls::Picker("Range", declared_range, [] { return ranges(); })
          .picker_style(controls::SegmentedPickerStyle{})
          .id(range_picker));
  return components::VStack(options, std::move(rows))
      .on_change(declared_matrix.get(),
                 [&control](const provider::ColourMatrix&,
                            const provider::ColourMatrix& now) {
                   control.set_matrix(now);
                 })
      .on_change(declared_range.get(),
                 [&control](const provider::ColourRange&,
                            const provider::ColourRange& now) {
                   control.set_range(now);
                 });
}

auto GpuStageScreen::hud() -> composition::Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading},
             components::Text("Full screen")
                 .font(theme::Font::headline)
                 .padding(theme::space::md)
                 .glass_effect(theme::Glass::regular(),
                               theme::Shape::capsule()),
             components::Spacer())
      .padding(theme::space::lg)
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

auto GpuStageScreen::surface_controls(composition::Context& context,
                                      StageControl& control)
    -> composition::Element {
  const bool continuous = context.read(control.shown_surface_drive()) ==
                          provider::FrameDrive::continuous;
  std::vector<composition::Element> rows;
  rows.push_back(
      components::Button(continuous ? "On demand" : "Continuous",
                         [&control, continuous] {
                           control.set_surface_drive(
                               continuous ? provider::FrameDrive::on_demand
                                          : provider::FrameDrive::continuous);
                         })
          .button_style(components::ButtonStyle::glass())
          .id(surface_drive_button));
  if (!continuous) {
    rows.push_back(components::Button(
                       "Next frame", [&control] { control.request_surfaces(); })
                       .button_style(components::ButtonStyle::glass())
                       .id(surface_next_button));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      std::move(rows));
}

auto GpuStageScreen::body(composition::Context& context) const
    -> composition::Element {
  const auto running = context.state(playing);
  const auto pace = context.state(speed);
  const auto shown = context.state(mode);
  StageControl& control = control_;
  const StageControl::Page page = context.read(control.shown_page());
  const bool video = page == StageControl::Page::video;
  const bool surfaces = page == StageControl::Page::surface &&
                        control.texture_surface().has_value() &&
                        control.underlay_surface().has_value();
  const bool full = context.read(control.shown_full_screen());
  const bool under = video && context.read(control.shown_under());
  const auto toast = context.state(full_screen_toast);
  composition::Element sidebar =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          components::Text("GPU stage").font(theme::Font::title),
          components::Button(running.get() ? "Pause" : "Play",
                             [running] { running.set(!running.get()); })
              .button_style(components::ButtonStyle::glass_prominent())
              .id(play_button),
          components::Text(std::format("Speed {:.1f}x", pace.get()))
              .font(theme::Font::footnote)
              .foreground_style(theme::ColourRole::text_secondary),
          controls::Slider(
              pace,
              controls::ClosedRange<double>{
                  .lower_bound = 0.0, .upper_bound = StageControl::fastest},
              0.1)
              .id(speed_slider),
          controls::Picker("Mode", shown, [] { return modes(); })
              .picker_style(controls::SegmentedPickerStyle{})
              .id(mode_picker),
          components::Button(video ? "Stage" : "Video",
                             [&control, video] {
                               control.set_page(
                                   video ? StageControl::Page::stage
                                         : StageControl::Page::video);
                             })
              .button_style(components::ButtonStyle::glass())
              .id(page_button),
          components::Button(surfaces ? "Stage" : "Surface",
                             [&control, surfaces] {
                               control.set_page(
                                   surfaces ? StageControl::Page::stage
                                            : StageControl::Page::surface);
                             })
              .button_style(components::ButtonStyle::glass())
              .id(surface_button),
          components::Button("Full screen",
                             [&control, toast] {
                               control.set_full_screen(true);
                               toast.set(true);
                             })
              .button_style(components::ButtonStyle::glass())
              .id(full_screen_button),
          video      ? decoding(context, control)
          : surfaces ? surface_controls(context, control)
                     : composition::Element{components::Spacer().frame(
                           {.height = 0.0F})},
          components::Spacer())
          .padding(theme::space::lg)
          .frame({.width = sidebar_width,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .glass_effect(theme::Glass::regular(),
                        theme::Shape::rounded(sidebar_radius))
          .on_change(
              running.get(), true,
              [&control](const bool&, const bool& now) { control.play(now); })
          .on_change(pace.get(), true,
                     [&control](const double&, const double& now) {
                       control.set_speed(now);
                     })
          .on_change(shown.get(), true,
                     [&control](const StageControl::Mode&,
                                const StageControl::Mode& now) {
                       control.set_mode(now);
                     });
  composition::Element content =
      video && !under
          ? composition::Element{components::HStack(
                components::Spacer(), composition::Element{VideoCard{control}},
                components::Spacer())}
      : surfaces ? composition::Element{SurfacePage{control}}
                 : composition::Element{components::Spacer()};
  composition::Element stage =
      components::HStack(std::move(sidebar), std::move(content))
          .padding(theme::space::lg)
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .exclusive_frame(full, {composition::OverlayLayer::toast,
                                  composition::OverlayLayer::hud});
  if (full) {
    stage = std::move(stage).on_exit_command(
        "Exit full screen", [&control] { control.set_full_screen(false); });
  }
  if (under) {
    stage = std::move(stage).container_background(
        composition::ContainerBackgroundPlacement::window,
        [&control] { return composition::Element{Backdrop{control}}; });
  }
  return std::move(stage)
      .overlay(full, composition::OverlayLayer::hud, hud())
      .toast(toast, toast_duration,
             [] { return components::Text("Full screen. Press B to return"); });
}

auto GpuStageScreen::Backdrop::body(composition::Context& context) const
    -> composition::Element {
  const provider::TextureId frame = context.read(control_.get().frame());
  if (frame.value() == 0) {
    return components::ZStack();
  }
  return components::Image::External(frame).frame(
      {.max_width = composition::infinity,
       .max_height = composition::infinity});
}

auto GpuStageScreen::VideoCard::body(composition::Context& context) const
    -> composition::Element {
  const provider::TextureId frame = context.read(control_.get().frame());
  composition::Element picture =
      frame.value() == 0
          ? composition::Element{components::VStack().frame(
                {.width = video_width, .height = video_height})}
          : composition::Element{components::Image::External(frame).frame(
                {.width = video_width, .height = video_height})};
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           components::Alignment::bottom},
             std::move(picture),
             components::Text("Live")
                 .font(theme::Font::headline)
                 .padding(theme::space::md)
                 .glass_effect(theme::Glass::regular(), theme::Shape::capsule())
                 .padding(theme::space::md))
      .padding(theme::space::md)
      .background(theme::ColourRole::secondary_system_background,
                  theme::Shape::rounded(card_radius));
}

auto GpuStageScreen::SurfacePage::card(composition::Element content,
                                       std::string_view label, bool glass)
    -> composition::Element {
  composition::Element caption = components::Text(std::string{label})
                                     .font(theme::Font::headline)
                                     .padding(theme::space::md);
  if (glass) {
    return components::ZStack(
               components::ZStackOptions{.alignment =
                                             components::Alignment::bottom},
               std::move(content),
               std::move(caption)
                   .glass_effect(theme::Glass::regular(),
                                 theme::Shape::capsule())
                   .padding(theme::space::md))
        .padding(theme::space::md)
        .background(theme::ColourRole::secondary_system_background,
                    theme::Shape::rounded(card_radius));
  }
  return components::VStack(
             components::VStackOptions{.spacing = theme::space::sm},
             std::move(content), std::move(caption))
      .padding(theme::space::md)
      .background(theme::ColourRole::secondary_system_background,
                  theme::Shape::rounded(card_radius));
}

auto GpuStageScreen::SurfacePage::body(composition::Context&) const
    -> composition::Element {
  const StageControl& control = control_;
  const auto texture = control.texture_surface();
  const auto underlay = control.underlay_surface();
  if (!texture.has_value() || !underlay.has_value()) {
    return components::Spacer();
  }
  const composition::FrameSpec size{.width = surface_width,
                                    .height = surface_height};
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{.spacing = theme::space::lg},
                 components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::top,
                         .spacing = theme::space::lg},
                     card(components::SurfaceView(texture->get()).frame(size),
                          "Texture", true),
                     card(components::SurfaceView(underlay->get()).frame(size),
                          "Underlay", false)),
                 components::Spacer().frame({.height = surface_page_height})))
      .id(surface_scroll);
}

auto GpuStageScreen::DecoderLine::body(composition::Context& context) const
    -> composition::Element {
  const StageControl::DecoderStatus& status =
      context.read(control_.get().decoder_status());
  const std::string text =
      status.failure.empty()
          ? std::format("Route {}, decoded {}, dropped {}, copy {:.2f} ms",
                        status.route, status.decoded, status.dropped,
                        static_cast<double>(status.copy.count()) / 1000.0)
          : status.failure;
  return components::Text(text)
      .font(theme::Font::footnote)
      .foreground_style(theme::ColourRole::text_secondary)
      .id(decoder_status_text);
}

}
