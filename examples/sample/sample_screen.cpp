#include <sample_screen.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace arisa::examples {

auto SampleScreen::body(composition::Context& context) const
    -> composition::Element {
  SampleModel& model = model_;
  SampleRenderer& renderer = model.renderer();
  const bool playing = context.read(model.playing());
  const bool full_screen = context.read(model.full_screen());
  const composition::FrameSpec card_size{.width = card_width,
                                         .height = card_height};
  composition::Element controls =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          components::Text("arisa sample").font(theme::Font::title),
          components::Button(playing ? "Pause" : "Play",
                             [&model, playing] { model.play(!playing); })
              .button_style(components::ButtonStyle::glass_prominent())
              .id(play_button),
          components::Button("Full screen",
                             [&model] { model.set_full_screen(true); })
              .button_style(components::ButtonStyle::glass())
              .id(full_screen_button),
          components::Spacer())
          .padding(theme::space::lg)
          .frame({.width = sidebar_width,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .glass_effect(theme::Glass::regular(),
                        theme::Shape::rounded(sidebar_radius))
          .id(sidebar);
  composition::Element cards =
      components::ScrollView(
          components::VStack(
              components::VStackOptions{.spacing = theme::space::lg},
              card(components::SurfaceView(renderer.card()).frame(card_size),
                   "Surface", card_caption),
              card(composition::Element{Picture{model}}, "Texture",
                   picture_caption),
              components::Spacer().frame({.height = list_tail})))
          .frame({.width = list_width, .max_height = composition::infinity})
          .id(list);
  composition::Element screen =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::top,
              .spacing = theme::space::lg},
          std::move(controls), std::move(cards), components::Spacer())
          .padding(theme::space::lg)
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top})
          .exclusive_frame(full_screen)
          .task(
              composition::ElementKey{std::int64_t{playing ? 1 : 0}},
              [&model, playing] { return repaint(std::ref(model), playing); });
  if (full_screen) {
    screen = std::move(screen).on_exit_command(
        "Exit full screen", [&model] { model.set_full_screen(false); });
  }
  return screen;
}

auto SampleScreen::card(composition::Element content, std::string_view label,
                        std::string_view key) -> composition::Element {
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           components::Alignment::bottom},
             std::move(content),
             components::Text(std::string{label})
                 .font(theme::Font::headline)
                 .padding(theme::space::md)
                 .glass_effect(theme::Glass::regular(), theme::Shape::capsule())
                 .id(key)
                 .padding(theme::space::md))
      .padding(theme::space::md)
      .background(theme::ColourRole::secondary_system_background,
                  theme::Shape::rounded(card_radius));
}

auto SampleScreen::repaint(std::reference_wrapper<SampleModel> model,
                           bool playing) -> async::Task<void> {
  if (!playing) {
    co_return;
  }
  while (true) {
    model.get().repaint_picture();
    co_await async::sleep(SampleModel::picture_interval);
  }
}

auto SampleScreen::Picture::body(composition::Context& context) const
    -> composition::Element {
  return components::Image::External(context.read(model_.get().picture()))
      .frame({.width = card_width, .height = card_height});
}

}
