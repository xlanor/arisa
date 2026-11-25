#include <arisa/components/environment.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/composition/node_anchor.hpp>

#include <memory>

namespace arisa::components {

const composition::StateKey<TimedWake> TimelineView::wake{"timeline_view"};

void TimelineView::Context::invalidate_timeline_content() const {
  if (const std::shared_ptr<const composition::NodeAnchor> alive =
          anchor.lock()) {
    alive->mark_dirty();
  }
}

auto TimelineView::body(composition::Context& context) const
    -> composition::Element {
  const TimelineSchedule::Date current = TimedWake::now(context);
  TimedWake& pending = context.retained(wake);
  if (schedule_.continuous()) {
    if (!schedule_.paused()) {
      if (schedule_.minimum_interval() > std::chrono::nanoseconds{0}) {
        if (const auto next = schedule_.next(current)) {
          pending.schedule(context, *next, current);
        }
      } else {
        pending.cancel();
        context.request_frame();
      }
    } else {
      pending.cancel();
    }
  } else if (const auto next = schedule_.next(current)) {
    pending.schedule(context, *next, current);
  } else {
    pending.cancel();
  }
  return content_(Context{.date = schedule_.entry(current),
                          .cadence = Cadence::live,
                          .mode = TimelineScheduleMode::normal,
                          .anchor = context.node_anchor()});
}

}
