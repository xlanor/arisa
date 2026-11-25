#include <arisa/components/open_url_inbox.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/subscription_view.hpp>

#include <algorithm>
#include <utility>

namespace arisa::components {

OpenURLInbox::OpenURLInbox() : state_(std::make_shared<State>()) {}

OpenURLInbox::OpenURLInbox(std::vector<std::string> claimed)
    : state_(std::make_shared<State>()) {
  state_->claimed = std::move(claimed);
}

void OpenURLInbox::deliver(provider::Url url) const {
  const std::uint64_t next = state_->deliveries.get().sequence + 1;
  state_->deliveries.set(Delivery{.sequence = next, .url = std::move(url)});
}

auto OpenURLInbox::claims(const provider::Url& url) const -> bool {
  return std::ranges::any_of(
      state_->claimed, [value = url.value()](const std::string& prefix) {
        return !prefix.empty() && value.starts_with(prefix);
      });
}

auto OpenURLInbox::deliveries() const noexcept
    -> composition::Observable<Delivery>& {
  return state_->deliveries;
}

auto OpenURLInbox::Receiver::body(composition::Context& context) const
    -> composition::Element {
  const std::optional<OpenURLInbox>& inbox =
      context.environment(Environment::open_url_inbox);
  composition::Element shown = composition::PlaceholderContentView{source_};
  if (!inbox.has_value()) {
    return shown;
  }
  return std::move(shown).on_receive(
      inbox->deliveries(), [action = action_](const Delivery& delivery) {
        if (delivery.url.has_value() && *action) {
          (*action)(*delivery.url);
        }
      });
}

}
