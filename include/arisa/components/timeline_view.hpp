#pragma once

#include <arisa/components/timed_wake.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/state_key.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::components {

class TimelineView final : public composition::Component {
 public:
  enum class Cadence : std::uint8_t { live, seconds, minutes };

  struct Context {
    TimelineSchedule::Date date;
    Cadence cadence{Cadence::live};
    TimelineScheduleMode mode{TimelineScheduleMode::normal};
    std::weak_ptr<const composition::NodeAnchor> anchor{};

    void invalidate_timeline_content() const;
  };

  using Content = std::function<composition::Element(const Context&)>;

  TimelineView(TimelineSchedule schedule, Content content)
      : schedule_(std::move(schedule)), content_(std::move(content)) {}

  auto operator==(const TimelineView&) const -> bool { return false; }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static const composition::StateKey<TimedWake> wake;

  TimelineSchedule schedule_;
  Content content_;
};

}
