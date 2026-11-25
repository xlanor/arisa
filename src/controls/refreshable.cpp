#include <arisa/components/environment.hpp>
#include <arisa/components/refresh_action.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/refreshable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace arisa::controls {

Refreshable::Refreshable(composition::Element content, Action action)
    : source_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content))),
      action_(std::make_shared<const Action>(std::move(action))) {}

auto Refreshable::run(std::shared_ptr<const Action> action,
                      composition::StateHandle<bool> busy)
    -> async::Task<void> {
  if (action != nullptr && *action) {
    co_await (*action)();
  }
  motion::with_animation(motion::Animation::smooth(0.3),
                         [busy] { busy.set(false); });
}

auto Refreshable::body(composition::Context& context) const
    -> composition::Element {
  const auto busy = context.state(refreshing);
  const auto count = context.state(generation);
  const auto trigger = [busy, count] {
    if (busy.get()) {
      return;
    }
    motion::with_animation(motion::Animation::smooth(0.3), [busy, count] {
      busy.set(true);
      count.set(count.get() + 1);
    });
  };
  std::optional<composition::Element> indicator;
  if (busy.get()) {
    indicator = composition::Element{ProgressView()}
                    .frame({.height = indicator_height,
                            .max_width = composition::infinity})
                    .transition(motion::Transition::opacity())
                    .id("arisa.refresh.indicator");
  }
  composition::Element column =
      components::VStack(
          components::VStackOptions{.spacing = theme::space::none},
          std::move(indicator),
          composition::PlaceholderContentView(source_)
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity})
              .flex_shrink(1.0F)
              .id("arisa.refresh.content"))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity});
  if (count.get() > 0) {
    column = std::move(column).task(
        composition::ElementKey{static_cast<std::int64_t>(count.get())},
        [action = action_, busy] { return run(action, busy); });
  }
  return std::move(column)
      .environment(components::Environment::refresh,
                   std::optional<components::RefreshAction>{
                       components::RefreshAction{trigger}})
      .refresh_trigger(trigger)
      .on_command(button, std::string{label}, trigger)
      .default_sensory_feedback(
          composition::SensoryFeedback::impact(
              composition::SensoryFeedback::Weight::medium),
          count.get());
}

}
