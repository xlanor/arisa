#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/controls/menu_style.hpp>

#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<MenuStyle> MenuStyle::key{
    "menu_style", MenuStyle::automatic()};

auto MenuStyle::automatic() noexcept -> MenuStyle {
  return MenuStyle{Kind::automatic};
}

auto MenuStyle::button() noexcept -> MenuStyle {
  return MenuStyle{Kind::button};
}

auto MenuStyle::borderless_button() noexcept -> MenuStyle {
  return MenuStyle{Kind::borderless_button};
}

auto MenuStyle::pull_down() noexcept -> MenuStyle {
  return MenuStyle{Kind::pull_down};
}

auto MenuStyle::borderless_pull_down() noexcept -> MenuStyle {
  return MenuStyle{Kind::borderless_pull_down};
}

auto operator==(const MenuStyle& left, const MenuStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != MenuStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto MenuStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  return components::Button(
             [open = std::move(configuration.open)] {
               if (open) {
                 open();
               }
             },
             std::move(configuration.label))
      .button_style(kind_ == Kind::borderless_button ||
                            kind_ == Kind::borderless_pull_down
                        ? components::ButtonStyle::borderless()
                    : kind_ == Kind::button
                        ? configuration.button_style.value_or(
                              components::ButtonStyle::bordered())
                        : components::ButtonStyle::bordered());
}

}
