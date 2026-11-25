#pragma once

#include <arisa/support/scoped_callback.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <memory>
#include <vector>

namespace arisa::composition {

struct ObservableVersionTag;

using ObservableVersion =
    support::StrongValue<std::uint64_t, ObservableVersionTag>;

using Subscription = support::ScopedCallback;

class ObservableCore : public std::enable_shared_from_this<ObservableCore> {
 public:
  using Listener = std::move_only_function<void()>;

  auto subscribe(Listener listener) -> Subscription;

  void publish();

  void publish(std::uintptr_t channel);

  auto channel(std::uintptr_t key) -> ObservableCore&;

  auto version() const noexcept -> ObservableVersion {
    return ObservableVersion{version_};
  }

  auto subscribers() const noexcept -> std::size_t {
    return listeners_.size() - retired_.size();
  }

 private:
  void unsubscribe(std::uint64_t id) noexcept;

  void notify();

  std::uint64_t version_{0};
  std::uint64_t next_id_{0};
  std::size_t publishing_{0};
  std::flat_map<std::uint64_t, std::unique_ptr<Listener>> listeners_;
  std::vector<std::uint64_t> retired_;
  std::flat_map<std::uintptr_t, std::shared_ptr<ObservableCore>> channels_;
};

}
