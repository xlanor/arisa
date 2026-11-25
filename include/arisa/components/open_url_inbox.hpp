#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/provider/browser/url.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::composition {

class Context;

}

namespace arisa::components {

class OpenURLInbox {
 public:
  class Receiver;

  struct Delivery {
    std::uint64_t sequence{0};
    std::optional<provider::Url> url{};

    friend auto operator==(const Delivery& left, const Delivery& right)
        -> bool {
      return left.sequence == right.sequence;
    }
  };

  using Action = std::function<void(const provider::Url&)>;

  OpenURLInbox();

  explicit OpenURLInbox(std::vector<std::string> claimed);

  void deliver(provider::Url url) const;

  auto claims(const provider::Url& url) const -> bool;

  auto deliveries() const noexcept -> composition::Observable<Delivery>&;

  friend auto operator==(const OpenURLInbox& left, const OpenURLInbox& right)
      -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct State {
    composition::Observable<Delivery> deliveries;
    std::vector<std::string> claimed;
  };

  std::shared_ptr<State> state_;
};

class OpenURLInbox::Receiver final : public composition::Component {
 public:
  Receiver(composition::Element content, Action action)
      : source_(std::make_shared<composition::PlaceholderContentView::Source>(
            std::move(content))),
        action_(std::make_shared<const Action>(std::move(action))) {}

  auto operator==(const Receiver& other) const -> bool {
    return source_ == other.source_ && action_ == other.action_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::shared_ptr<composition::PlaceholderContentView::Source> source_;
  std::shared_ptr<const Action> action_;
};

}

namespace arisa::composition {

template <class Action>
auto Element::on_open_url(Action action) && -> Element {
  return components::OpenURLInbox::Receiver{
      std::move(*this), components::OpenURLInbox::Action{std::move(action)}};
}

}
