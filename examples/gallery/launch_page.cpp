#include <launch_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/theme/space.hpp>

#include <optional>

namespace arisa::examples {

auto LaunchPage::body(composition::Context&) const -> composition::Element {
  const auto play = [replay = replay_](LaunchScreen::Ending ending,
                                       LaunchScreen::Ink ink) {
    return [replay, ending, ink] {
      if (replay != nullptr && *replay) {
        (*replay)(LaunchScreen::Style{.ending = ending, .ink = ink});
      }
    };
  };
  std::optional<composition::Element> credit;
  if (lettering_font_) {
    credit =
        components::Text("Bumbbled by Viswell Studio (local, not distributed)")
            .font(theme::Font::footnote)
            .foreground_style(theme::ColourRole::text_secondary);
  }
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 components::Text("Launch writing").font(theme::Font::headline),
                 components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::center,
                         .spacing = theme::space::md,
                         .distribution = components::Distribution::start,
                         .wrap = true},
                     composition::Element{
                         components::Button("Replay with fade",
                                            play(LaunchScreen::Ending::fade,
                                                 LaunchScreen::Ink::white))}
                         .id(fade_button),
                     composition::Element{
                         components::Button("Replay with tail-chase erase",
                                            play(LaunchScreen::Ending::erase,
                                                 LaunchScreen::Ink::white))}
                         .id(erase_button),
                     composition::Element{
                         components::Button("Replay in Joy-Con gradient",
                                            play(LaunchScreen::Ending::fade,
                                                 LaunchScreen::Ink::gradient))}
                         .id(gradient_button),
                     composition::Element{
                         components::Button("Replay as Liquid Glass",
                                            play(LaunchScreen::Ending::fade,
                                                 LaunchScreen::Ink::glass))}
                         .id(glass_button)),
                 std::move(credit))
                 .frame({.max_width = composition::infinity,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity});
}

}
