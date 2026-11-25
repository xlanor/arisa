#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace arisa::controls {

class Refreshable final : public composition::Component {
 public:
  using Action = std::function<async::Task<void>()>;

  static constexpr composition::StateKey<bool> refreshing{"refreshing", false};
  static constexpr composition::StateKey<int> generation{"refresh_generation",
                                                         0};
  static constexpr provider::Button button = provider::Button::x;
  static constexpr std::string_view label = "Refresh";
  static constexpr float indicator_height = 44.0F;
  static constexpr float pull_distance = 64.0F;

  Refreshable(composition::Element content, Action action);

  auto operator==(const Refreshable& other) const -> bool {
    return source_ == other.source_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto run(std::shared_ptr<const Action> action,
                  composition::StateHandle<bool> busy) -> async::Task<void>;

  std::shared_ptr<composition::PlaceholderContentView::Source> source_;
  std::shared_ptr<const Action> action_;
};

}

namespace arisa::composition {

template <class Action>
auto Element::refreshable(Action action) && -> Element {
  return controls::Refreshable{
      std::move(*this), controls::Refreshable::Action{std::move(action)}};
}

}
