#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/observable.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace arisa::structure {

class SidebarToggle {
 public:
  using Action = std::function<void()>;

  static const composition::EnvironmentKey<std::optional<SidebarToggle>> key;

  SidebarToggle();

  void attach(composition::Identity owner, Action action) const;

  void detach(composition::Identity owner) const;

  auto available() const noexcept -> const composition::Observable<bool>& {
    return shared_->available;
  }

  auto floating_panel() const noexcept -> const
      composition::Observable<composition::NavigationSplitViewVisibility>& {
    return shared_->floating_panel;
  }

  void show_floating_panel(
      composition::NavigationSplitViewVisibility shown) const;

  auto header_depth() const noexcept -> const composition::Observable<float>& {
    return shared_->header_depth;
  }

  void report_header_depth(float depth) const;

  void toggle() const;

  friend auto operator==(const SidebarToggle& left, const SidebarToggle& right)
      -> bool {
    return left.shared_ == right.shared_;
  }

 private:
  struct Shared {
    composition::Observable<bool> available{false};
    composition::Observable<composition::NavigationSplitViewVisibility>
        floating_panel{composition::NavigationSplitViewVisibility::detail_only};
    composition::Observable<float> header_depth{0.0F};
    std::optional<composition::Identity> owner;
    Action action;
  };

  std::shared_ptr<Shared> shared_;
};

}
