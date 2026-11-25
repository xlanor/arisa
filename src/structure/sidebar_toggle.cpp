#include <arisa/structure/sidebar_toggle.hpp>

#include <cmath>
#include <utility>

namespace arisa::structure {

const composition::EnvironmentKey<std::optional<SidebarToggle>>
    SidebarToggle::key{"sidebar_toggle", std::nullopt};

SidebarToggle::SidebarToggle() : shared_(std::make_shared<Shared>()) {}

void SidebarToggle::attach(composition::Identity owner, Action action) const {
  shared_->owner = owner;
  shared_->action = std::move(action);
  shared_->available.set(true);
}

void SidebarToggle::detach(composition::Identity owner) const {
  if (shared_->owner != owner) {
    return;
  }
  shared_->owner.reset();
  shared_->action = nullptr;
  shared_->available.set(false);
}

void SidebarToggle::show_floating_panel(
    composition::NavigationSplitViewVisibility shown) const {
  shared_->floating_panel.set(shown);
}

void SidebarToggle::report_header_depth(float depth) const {
  if (std::abs(shared_->header_depth.get() - depth) > 0.25F) {
    shared_->header_depth.set(depth);
  }
}

void SidebarToggle::toggle() const {
  if (shared_->action) {
    shared_->action();
  }
}

}
