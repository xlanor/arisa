#include <arisa/composition/context.hpp>
#include <arisa/composition/submission.hpp>

#include <algorithm>
#include <iterator>

namespace arisa::composition {

const EnvironmentKey<Submission::Handlers> Submission::handlers{
    "submit-handlers", Submission::Handlers{}};

const EnvironmentKey<Submission::Handlers> Submission::search_handlers{
    "search-submit-handlers", Submission::Handlers{}};

const EnvironmentKey<SubmitLabel> Submission::label{"submit-label",
                                                    SubmitLabel::return_key};

void Submission::Handlers::add(std::move_only_function<void()> action) {
  auto shared =
      std::make_shared<std::move_only_function<void()>>(std::move(action));
  if (scoped_) {
    outside_.push_back(std::move(shared));
    return;
  }
  inside_.push_back(std::move(shared));
}

auto Submission::collect(Context& context) -> Submission {
  return collect(context, SubmitTriggers::text);
}

auto Submission::collect(Context& context, SubmitTriggers triggers)
    -> Submission {
  std::vector<Action> actions;
  if (triggers.contains(SubmitTriggers::text)) {
    gather(context, handlers, actions);
  }
  if (triggers.contains(SubmitTriggers::search)) {
    gather(context, search_handlers, actions);
  }
  return Submission{std::move(actions)};
}

void Submission::gather(Context& context, const EnvironmentKey<Handlers>& key,
                        std::vector<Action>& actions) {
  for (const Handlers& level : context.environment_chain(key)) {
    std::ranges::copy(level.inside(), std::back_inserter(actions));
    if (level.scoped()) {
      break;
    }
    std::ranges::copy(level.outside(), std::back_inserter(actions));
  }
}

void Submission::operator()() const {
  for (const Action& action : actions_) {
    (*action)();
  }
}

}
