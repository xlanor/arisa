#include <animators_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/navigation_transition.hpp>
#include <arisa/composition/phase_animator.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/composition/symbol_effect_animator.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto AnimatorsPage::Spin::body(motion::TransitionContent content,
                               motion::TransitionPhase phase) const
    -> motion::TransitionContent {
  return content.opacity(phase.is_identity() ? 1.0F : 0.0F)
      .rotation_effect(motion::Angle::degrees(phase.value() * 45.0))
      .scale_effect(phase.is_identity() ? 1.0F : 0.6F);
}

auto AnimatorsPage::card(std::string_view title, Element content) -> Element {
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

auto AnimatorsPage::Clock::body(composition::Context& context) const
    -> Element {
  const components::SystemStatus& status =
      context.environment(components::Environment::system_status);
  const auto offset =
      status.local_time().has_value() && status.wall_time()
          ? status.local_time()->time_since_epoch() -
                std::chrono::floor<std::chrono::seconds>(*status.wall_time())
                    .time_since_epoch()
          : std::chrono::seconds{0};
  const auto local = std::chrono::floor<std::chrono::seconds>(date_) + offset;
  const auto day = std::chrono::floor<std::chrono::days>(local);
  const std::chrono::hh_mm_ss<std::chrono::seconds> time{local - day};
  return components::Text(std::format("{:02}:{:02}:{:02}", time.hours().count(),
                                      time.minutes().count(),
                                      time.seconds().count()))
      .font(theme::Font::title)
      .monospaced_digit();
}

auto AnimatorsPage::Zoom::body(composition::Context& context) const -> Element {
  const composition::Namespace::ID ns = context.namespace_id(space);
  return presentation::NavigationStack([ns] {
           std::vector<Element> cards;
           for (int index = 0; index < 3; ++index) {
             const theme::ColourValue tint =
                 index == 0
                     ? ColourRole::indigo
                     : (index == 1 ? ColourRole::teal : ColourRole::orange);
             cards.push_back(Element{
                 presentation::NavigationLink(
                     index,
                     Element{components::Text(std::format("Game {}", index + 1))
                                 .foreground_style(ColourRole::on_accent)
                                 .frame({.width = 96.0F, .height = 72.0F})
                                 .background(tint, theme::RoundedRectangle(
                                                       theme::radius::card))})}
                                 .button_style(components::ButtonStyle::plain())
                                 .matched_transition_source(index, ns)
                                 .id(index == 0
                                         ? std::string{zoom_card}
                                         : std::format("animators-zoom-{}",
                                                       index)));
           }
           return components::HStack(
                      components::HStackOptions{.spacing = theme::space::md},
                      std::move(cards))
               .frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::top_leading})
               .navigation_destination(
                   composition::for_type<int>, [ns](const int& index) {
                     return components::Text(std::format("Game {}", index + 1))
                         .font(theme::Font::large_title)
                         .frame({.max_width = infinity, .max_height = infinity})
                         .background(ColourRole::secondary_system_background)
                         .navigation_title(std::format("Game {}", index + 1))
                         .navigation_transition(
                             composition::NavigationTransition::zoom(index,
                                                                     ns));
                   });
         })
      .frame({.height = 220.0F, .max_width = infinity})
      .clip_shape(theme::RoundedRectangle(theme::radius::control));
}

auto AnimatorsPage::Morph::body(composition::Context& context) const
    -> Element {
  const composition::Namespace::ID ns = context.namespace_id(space);
  const auto open = context.state(open_key);
  const auto toggle = [open] {
    motion::with_animation(motion::Animation::bouncy(0.4),
                           [&open] { open.set(!open.get()); });
  };
  Element shape =
      open.get() ? components::HStack(
                       components::HStackOptions{.spacing = theme::space::lg},
                       components::Button("Back", toggle)
                           .id(std::string{morph_button}),
                       components::Image::system_name("star"),
                       components::Image::system_name("schedule"),
                       components::Image::system_name("sync"))
                       .padding(theme::space::md)
                       .frame({.width = 280.0F, .height = 48.0F})
                       .glass_effect(theme::Glass::regular(),
                                     theme::RoundedRectangle(16.0F))
                       .glass_effect_id("toolbar", ns)
                 : components::Button("Tools", toggle)
                       .frame({.width = 72.0F, .height = 48.0F})
                       .glass_effect(theme::Glass::regular(), theme::Capsule{})
                       .glass_effect_id("toolbar", ns)
                       .id(std::string{morph_button});
  return Element{components::GlassEffectContainer(std::move(shape))}.frame(
      {.height = 64.0F, .alignment = composition::Alignment::leading});
}

auto AnimatorsPage::Demos::body(composition::Context& context) const
    -> Element {
  const auto live = context.state(live_key);
  const auto favourite = context.state(favourite_key);
  const auto materialized = context.state(materialized_key);
  const auto inserted = context.state(inserted_key);
  const auto styled = context.state(styled_key);
  const bool running = live.get();
  Element prompt =
      components::Text("Press A")
          .font(theme::Font::title3)
          .padding(theme::space::md)
          .background(ColourRole::secondary_system_fill, theme::Capsule{});
  prompt = running
               ? std::move(prompt).phase_animator(
                     std::vector<bool>{false, true},
                     [](Element content, const bool& bright) {
                       return std::move(content).opacity(bright ? 1.0F : 0.4F);
                     },
                     [](const bool&) -> std::optional<motion::Animation> {
                       return motion::Animation::ease_in_out(0.8);
                     })
               : std::move(prompt);
  Element clock{components::TimelineView(
      running
          ? components::TimelineSchedule::periodic(
                components::TimelineSchedule::Date{}, std::chrono::seconds{1})
          : components::TimelineSchedule::explicit_dates({}),
      [](const components::TimelineView::Context& timeline) {
        return Element{Clock{timeline.date}};
      })};
  const auto tapped = [favourite] { favourite.set(favourite.get() + 1); };
  Element heart =
      components::Image::system_name("favorite")
          .font(theme::Font::large_title)
          .foreground_style(ColourRole::pink)
          .symbol_effect(composition::SymbolEffect::bounce(), favourite.get());
  Element bell =
      components::Image::system_name("notifications")
          .font(theme::Font::large_title)
          .symbol_effect(composition::SymbolEffect::pulse(), running);
  std::vector<Element> transitions;
  transitions.push_back(components::Button("Toggle", [inserted] {
                          motion::with_animation(
                              motion::Animation::smooth(0.5),
                              [&inserted] { inserted.set(!inserted.get()); });
                        }).id(std::string{transition_button}));
  if (inserted.get()) {
    transitions.push_back(
        components::Text("Spin")
            .padding(theme::space::md)
            .background(ColourRole::indigo, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(motion::Transition::custom(Spin{})));
    transitions.push_back(
        components::Text("Blur")
            .padding(theme::space::md)
            .background(ColourRole::teal, theme::RoundedRectangle(12.0F))
            .foreground_style(ColourRole::on_accent)
            .transition(motion::Transition::blur_replace()));
  }
  std::vector<Element> materials;
  materials.push_back(components::Button("Materialize", [materialized] {
                        motion::with_animation(
                            motion::Animation::smooth(0.6), [&materialized] {
                              materialized.set(!materialized.get());
                            });
                      }).id(std::string{materialize_button}));
  if (materialized.get()) {
    materials.push_back(
        components::Text("Glass")
            .padding(theme::space::md)
            .glass_effect(theme::Glass::regular(), theme::Capsule{})
            .glass_effect_transition(
                composition::GlassEffectTransition::materialize()));
  }
  const bool shaped = styled.get();
  Element styles = components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Button("Restyle",
                         [styled] {
                           motion::with_animation(
                               motion::Animation::smooth(0.8),
                               [&styled] { styled.set(!styled.get()); });
                         })
          .id(std::string{styles_button}),
      components::ZStack()
          .frame({.width = 120.0F, .height = 64.0F})
          .background(
              graphics::LinearGradient{
                  .gradient =
                      graphics::Gradient{std::vector<theme::ColourValue>{
                          shaped ? ColourRole::mint : ColourRole::pink,
                          shaped ? ColourRole::blue : ColourRole::orange}},
                  .start_point = motion::UnitPoint::leading,
                  .end_point = motion::UnitPoint::trailing},
              shaped ? theme::Shape{theme::UnevenRoundedRectangle(28.0F, 4.0F,
                                                                  28.0F, 4.0F)}
                     : theme::Shape{theme::RoundedRectangle(8.0F)}),
      components::Image::system_name("star")
          .font(theme::Font::large_title)
          .blur(shaped ? 0.0F : 3.0F)
          .saturation(shaped ? 1.0 : 0.0));
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      card("Live", controls::Toggle("Run", composition::Binding<bool>{live})
                       .id(std::string{live_toggle})),
      card("Prompt", std::move(prompt)), card("Clock", std::move(clock)),
      card("Zoom", Element{Zoom{}}), card("Glass morph", Element{Morph{}}),
      card("Materialize",
           components::HStack(
               components::HStackOptions{.spacing = theme::space::md},
               std::move(materials))),
      card("Favourite",
           components::HStack(
               components::HStackOptions{.spacing = theme::space::lg},
               components::Button("Favourite", tapped)
                   .id(std::string{favourite_button}),
               std::move(heart), std::move(bell))),
      card("Transitions",
           components::HStack(
               components::HStackOptions{.spacing = theme::space::md},
               std::move(transitions))),
      card("Styles", std::move(styles)));
}

auto AnimatorsPage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(Element{Demos{}}.padding(theme::space::lg))
            .navigation_title("Animators")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto AnimatorsPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Animators", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{AnimatorsPage{shown}}; });
}

}
