#include <arisa/presentation/presented_destination.hpp>

#include <utility>

namespace arisa::presentation {

const composition::StateKey<
    std::optional<PresentedDestination::Content::Pushed>>
    PresentedDestination::Content::pushed{"arisa.destination.pushed",
                                          std::nullopt};

PresentedDestination::PresentedDestination(
    composition::Element host, std::optional<composition::ElementKey> presented,
    Build build, Dismissed dismissed)
    : Element(std::make_unique<Content>(std::move(presented), std::move(build),
                                        std::move(dismissed))) {
  inherit_identity(*this, host);
  inherit(*this, host);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(host)));
}

auto PresentedDestination::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto PresentedDestination::Content::destination(const Request& request,
                                                std::uint64_t serial)
    -> NavigationStack::ViewDestination {
  return NavigationStack::ViewDestination{
      [build = request.build, held = request.held,
       dismissed = request.dismissed, serial] {
        return build().on_disappear([held, dismissed, serial] {
          const std::optional<Pushed> current = held.get();
          if (current.has_value() && current->serial == serial) {
            held.set(std::nullopt);
            if (dismissed) {
              dismissed();
            }
          }
        });
      }};
}

void PresentedDestination::Content::sync(const Request& request) {
  const std::optional<Pushed> current = request.held.get();
  const bool live =
      current.has_value() && request.navigator.contains(current->view);
  if (request.presented.has_value() && request.build) {
    if (live && current->key == *request.presented) {
      return;
    }
    const std::uint64_t serial = request.counter.get() + 1;
    request.counter.set(serial);
    const NavigationStack::ViewDestination made = destination(request, serial);
    request.held.set(Pushed{
        .serial = serial, .key = *request.presented, .view = made.identity()});
    const composition::NavigationPath::Entry entry =
        composition::NavigationPath::Entry::of(made);
    if (live) {
      request.navigator.remove(current->view);
    }
    if (!request.navigator.push(entry)) {
      request.held.set(std::nullopt);
    }
    return;
  }
  if (current.has_value()) {
    request.held.set(std::nullopt);
    if (live) {
      request.navigator.remove(current->view);
    }
  }
}

auto PresentedDestination::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  Request request{.navigator = context.environment(NavigationStack::navigator),
                  .held = context.state(pushed),
                  .counter = context.state(serials),
                  .presented = presented_,
                  .build = build_,
                  .dismissed = dismissed_};
  std::vector<composition::Element> produced;
  produced.push_back(composition::PlaceholderContentView(host_).on_change(
      presented_, true, [request = std::move(request)] { sync(request); }));
  return produced;
}

}
