#include <arisa/runtime/overlays.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/length.hpp>

#include <optional>
#include <string>
#include <vector>

namespace arisa::runtime {

const composition::EnvironmentKey<std::optional<Overlays>>
    Overlays::environment{"arisa.overlays", std::nullopt};

Overlays::Overlays(Dispatcher dispatcher, composition::Tree& tree)
    : dispatcher_(std::move(dispatcher)),
      tree_(tree),
      next_(std::make_shared<std::atomic<std::uint64_t>>(1)) {}

auto Overlays::withdraw(const Dispatcher& dispatcher, composition::Tree* tree,
                        std::uint64_t id) -> bool {
  return dispatcher.post(
      [tree, id] { tree->present_overlay(id, std::nullopt); });
}

auto Overlays::expire(Dispatcher dispatcher, composition::Tree* tree,
                      std::uint64_t id, std::chrono::nanoseconds duration)
    -> async::Task<void> {
  co_await async::sleep(duration);
  static_cast<void>(withdraw(dispatcher, tree, id));
}

auto Overlays::present(composition::OverlayLayer layer,
                       composition::Element content, OverlayBehaviour behaviour)
    -> OverlayHandle {
  const std::uint64_t id = next_->fetch_add(1, std::memory_order_relaxed);
  composition::Tree* tree = &tree_.get();
  std::vector<composition::Element> children;
  if (behaviour.dismiss_on_tap_outside) {
    children.push_back(
        components::ZStack()
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .on_tap_gesture([dispatcher = dispatcher_, tree, id] {
              static_cast<void>(withdraw(dispatcher, tree, id));
            })
            .transition(motion::Transition::identity()));
  }
  children.push_back(std::move(content).transition(behaviour.transition));
  composition::Element shown = composition::Layer(
      layer, behaviour.input, behaviour.alignment, std::move(children));
  if (behaviour.dismiss_on_back) {
    shown = std::move(shown).on_exit_command(
        std::string{close_label}, [dispatcher = dispatcher_, tree, id] {
          static_cast<void>(withdraw(dispatcher, tree, id));
        });
  }
  if (behaviour.duration.has_value()) {
    shown = std::move(shown).task(
        [dispatcher = dispatcher_, tree, id, duration = *behaviour.duration] {
          return expire(dispatcher, tree, id, duration);
        });
  }
  const bool posted =
      dispatcher_.post([tree, id, element = std::move(shown)]() mutable {
        tree->present_overlay(id, std::move(element),
                              composition::Tree::OverlayEnvironment::inherited);
      });
  if (!posted) {
    return OverlayHandle{support::ScopedCallback{}};
  }
  return OverlayHandle{
      support::ScopedCallback{[dispatcher = dispatcher_, tree, id]() noexcept {
        static_cast<void>(withdraw(dispatcher, tree, id));
      }}};
}

}
