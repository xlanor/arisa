#include <arisa/components/environment.hpp>
#include <arisa/components/timed_wake.hpp>
#include <arisa/composition/node_anchor.hpp>

#include <algorithm>
#include <stop_token>

namespace arisa::components {

auto TimedWake::now(composition::Context& context) -> Date {
  if (const auto wall =
          context.environment(Environment::system_status).wall_time()) {
    return *wall;
  }
  return Date{std::chrono::duration_cast<Date::duration>(context.frame_time())};
}

void TimedWake::schedule(composition::Context& context, Date at, Date current) {
  if (job_ != nullptr && due_ == at) {
    return;
  }
  const auto delay = std::max(
      std::chrono::duration_cast<std::chrono::nanoseconds>(at - current),
      std::chrono::nanoseconds{0});
  std::weak_ptr<const composition::NodeAnchor> anchor = context.node_anchor();
  TimedWake* state = this;
  due_ = at;
  job_ = std::make_shared<async::Job>(context.services().frames.get().after(
      delay, [state, anchor = std::move(anchor)](std::stop_token) {
        const std::shared_ptr<const composition::NodeAnchor> alive =
            anchor.lock();
        if (alive == nullptr) {
          return;
        }
        std::shared_ptr<async::Job> finished = std::move(state->job_);
        state->due_.reset();
        alive->mark_dirty();
      }));
}

void TimedWake::cancel() noexcept {
  job_.reset();
  due_.reset();
}

}
