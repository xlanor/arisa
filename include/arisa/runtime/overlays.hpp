#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>
#include <arisa/runtime/overlay_handle.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::runtime {

class Overlays {
 public:
  static const composition::EnvironmentKey<std::optional<Overlays>> environment;

  static constexpr std::string_view close_label = "Close";

  Overlays(Dispatcher dispatcher, composition::Tree& tree);

  friend auto operator==(const Overlays& left, const Overlays& right) noexcept
      -> bool {
    return left.dispatcher_ == right.dispatcher_ &&
           &left.tree_.get() == &right.tree_.get();
  }

  template <class View>
    requires std::constructible_from<composition::Element, View>
  auto show(composition::OverlayLayer layer, View view,
            OverlayBehaviour behaviour = {}) -> OverlayHandle {
    return present(layer, composition::Element{std::move(view)},
                   std::move(behaviour));
  }

 private:
  auto present(composition::OverlayLayer layer, composition::Element content,
               OverlayBehaviour behaviour) -> OverlayHandle;

  static auto expire(Dispatcher dispatcher, composition::Tree* tree,
                     std::uint64_t id, std::chrono::nanoseconds duration)
      -> async::Task<void>;

  static auto withdraw(const Dispatcher& dispatcher, composition::Tree* tree,
                       std::uint64_t id) -> bool;

  Dispatcher dispatcher_;
  std::reference_wrapper<composition::Tree> tree_;
  std::shared_ptr<std::atomic<std::uint64_t>> next_;
};

}
