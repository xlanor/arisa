#include <arisa/structure/sidebar_selection.hpp>

namespace arisa::structure {

const composition::EnvironmentKey<std::optional<SidebarSelection>>
    SidebarSelection::key{"sidebar_selection", std::nullopt};

void SidebarSelection::present(composition::TagKey chosen) const {
  commit_(std::move(chosen));
}

void SidebarSelection::read(Current current) const {
  reader_->read(std::move(current));
}

auto SidebarSelection::Reader::current() const
    -> std::optional<composition::TagKey> {
  if (!current_) {
    return std::nullopt;
  }
  return current_();
}

}
