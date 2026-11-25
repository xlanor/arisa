#include <arisa/provider/counted_scope.hpp>

#include <mutex>
#include <utility>

namespace arisa::provider {

class CountedScope::State {
 public:
  State(Enter enter, Leave leave) noexcept
      : enter_(std::move(enter)), leave_(std::move(leave)) {}

  auto enter() -> std::expected<void, ProviderError> {
    const std::scoped_lock lock{mutex_};
    if (holders_ == 0) {
      if (auto entered = enter_(); !entered.has_value()) {
        return entered;
      }
    }
    ++holders_;
    return {};
  }

  void leave() noexcept {
    const std::scoped_lock lock{mutex_};
    --holders_;
    if (holders_ == 0) {
      leave_();
    }
  }

  auto holders() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return holders_;
  }

 private:
  mutable std::mutex mutex_;
  Enter enter_;
  Leave leave_;
  std::size_t holders_{0};
};

CountedScope::CountedScope(Enter enter, Leave leave)
    : state_(std::make_shared<State>(std::move(enter), std::move(leave))) {}

auto CountedScope::acquire()
    -> std::expected<support::ScopedCallback, ProviderError> {
  return state_->enter().transform([this] {
    return support::ScopedCallback{
        [state = state_]() noexcept { state->leave(); }};
  });
}

auto CountedScope::holders() const -> std::size_t { return state_->holders(); }

}
