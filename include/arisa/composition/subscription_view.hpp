#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable_core.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class P>
concept Publisher = requires(const P& publisher) {
  {
    publisher.subscribe(ObservableCore::Listener{})
  } -> std::same_as<Subscription>;
  publisher.get();
};

template <Publisher P>
class SubscriptionView final : public Component {
 public:
  using Output = std::remove_cvref_t<decltype(std::declval<const P&>().get())>;
  using Action = std::function<void(const Output&)>;

  SubscriptionView(Element content, P& publisher, Action action)
      : source_(std::make_shared<PlaceholderContentView::Source>(
            std::move(content))),
        publisher_(&publisher),
        action_(std::make_shared<const Action>(std::move(action))) {}

  auto content() const -> PlaceholderContentView {
    return PlaceholderContentView{source_};
  }

  auto publisher() const noexcept -> P& { return *publisher_; }

  auto action() const noexcept -> const Action& { return *action_; }

  auto operator==(const SubscriptionView& other) const -> bool {
    return source_ == other.source_ && publisher_ == other.publisher_ &&
           action_ == other.action_;
  }

  auto body(Context& context) const -> Element override {
    std::shared_ptr<Receiver>& held = context.retained(receiver);
    if (held == nullptr) {
      held = std::make_shared<Receiver>();
    }
    held->action = action_;
    if (held->subscribed != publisher_) {
      held->subscribed = publisher_;
      held->subscription.reset();
      held->subscription = publisher_->subscribe(
          [weak = std::weak_ptr<Receiver>{held}, publisher = publisher_] {
            const std::shared_ptr<Receiver> alive = weak.lock();
            if (alive == nullptr || alive->action == nullptr ||
                !*alive->action) {
              return;
            }
            const std::shared_ptr<const Action> action = alive->action;
            (*action)(publisher->get());
          });
    }
    return PlaceholderContentView{source_};
  }

 private:
  struct Receiver {
    std::shared_ptr<const Action> action;
    P* subscribed{nullptr};
    Subscription subscription;
  };

  static inline const StateKey<std::shared_ptr<Receiver>> receiver{
      "arisa.subscription-view", nullptr};

  std::shared_ptr<PlaceholderContentView::Source> source_;
  P* publisher_;
  std::shared_ptr<const Action> action_;
};

template <class P, class Action>
auto Element::on_receive(P& publisher, Action action) && -> Element {
  static_assert(Publisher<P>,
                "a publisher subscribes listeners and exposes its value");
  return SubscriptionView<P>{
      std::move(*this), publisher,
      typename SubscriptionView<P>::Action{std::move(action)}};
}

}
