#include <arisa/composition/observable_core.hpp>

#include <algorithm>
#include <utility>

namespace arisa::composition {

auto ObservableCore::subscribe(Listener listener) -> Subscription {
  const std::uint64_t id = next_id_++;
  listeners_.emplace(id, std::make_unique<Listener>(std::move(listener)));
  return Subscription{[core = weak_from_this(), id]() noexcept {
    if (const auto alive = core.lock()) {
      alive->unsubscribe(id);
    }
  }};
}

void ObservableCore::publish() {
  notify();
  for (const auto& entry : channels_) {
    entry.second->notify();
  }
}

void ObservableCore::publish(std::uintptr_t channel) {
  notify();
  if (const auto found = channels_.find(channel); found != channels_.end()) {
    found->second->notify();
  }
}

auto ObservableCore::channel(std::uintptr_t key) -> ObservableCore& {
  auto found = channels_.find(key);
  if (found == channels_.end()) {
    found = channels_.emplace(key, std::make_shared<ObservableCore>()).first;
  }
  return *found->second;
}

void ObservableCore::notify() {
  ++version_;
  ++publishing_;
  const std::vector<std::uint64_t> snapshot{listeners_.keys().begin(),
                                            listeners_.keys().end()};
  for (const std::uint64_t id : snapshot) {
    if (std::ranges::contains(retired_, id)) {
      continue;
    }
    if (const auto found = listeners_.find(id); found != listeners_.end()) {
      (*found->second)();
    }
  }
  --publishing_;
  if (publishing_ == 0) {
    for (const std::uint64_t id : std::exchange(retired_, {})) {
      listeners_.erase(id);
    }
  }
}

void ObservableCore::unsubscribe(std::uint64_t id) noexcept {
  if (publishing_ > 0) {
    retired_.push_back(id);
    return;
  }
  listeners_.erase(id);
}

}
