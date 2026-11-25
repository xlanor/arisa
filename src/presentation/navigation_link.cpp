#include <arisa/presentation/navigation_link.hpp>

#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>

#include <memory>
#include <utility>

namespace arisa::presentation {

NavigationLink::NavigationLink(composition::NavigationPath::Entry entry,
                               std::string label)
    : NavigationLink(std::move(entry),
                     components::Text(std::move(label))
                         .font(theme::Font{components::Button::label_role}),
                     std::make_shared<Target>()) {}

NavigationLink::NavigationLink(composition::NavigationPath::Entry entry,
                               composition::Element label)
    : NavigationLink(std::move(entry), std::move(label),
                     std::make_shared<Target>()) {}

NavigationLink::NavigationLink(std::string label, Destination destination)
    : NavigationLink(
          composition::NavigationPath::Entry::of(
              NavigationStack::ViewDestination{std::move(destination)}),
          std::move(label)) {}

NavigationLink::NavigationLink(Destination destination,
                               composition::Element label)
    : NavigationLink(
          composition::NavigationPath::Entry::of(
              NavigationStack::ViewDestination{std::move(destination)}),
          std::move(label)) {}

NavigationLink::NavigationLink(composition::NavigationPath::Entry entry,
                               composition::Element label, Cell cell)
    : Button(push(std::move(entry), cell), labelled(std::move(label), cell)),
      cell_(std::move(cell)) {
  if (const auto content = Button::content(*this)) {
    content->get().set_discloses(true);
  }
}

auto NavigationLink::is_detail_link(bool detail) && -> NavigationLink {
  cell_->detail = detail;
  return std::move(*this);
}

auto NavigationLink::push(composition::NavigationPath::Entry entry, Cell cell)
    -> Action {
  return [entry = std::move(entry), cell = std::move(cell)] {
    if (cell->navigator.has_value()) {
      static_cast<void>(cell->navigator->push(entry));
    }
  };
}

auto NavigationLink::labelled(composition::Element label, Cell cell)
    -> composition::Element {
  return components::Group(std::move(label), Probe(std::move(cell)));
}

NavigationLink::Probe::Probe(Cell cell)
    : Element(std::make_unique<Content>(std::move(cell))) {}

auto NavigationLink::Probe::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NavigationLink::Probe::Content::composes() const noexcept -> bool {
  return true;
}

auto NavigationLink::Probe::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const NavigationStack::Navigator& detail =
      context.environment(NavigationStack::detail_navigator);
  const NavigationStack::Navigator& found =
      cell_->detail && detail.available()
          ? detail
          : context.environment(NavigationStack::navigator);
  if (found.available()) {
    cell_->navigator = found;
  } else {
    cell_->navigator.reset();
  }
  return {};
}

}
