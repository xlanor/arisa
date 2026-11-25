#include <arisa/composition/accessibility.hpp>

#include <array>

namespace arisa::composition {

auto AccessibilityTraits::name(AccessibilityTrait trait) noexcept
    -> std::string_view {
  switch (trait) {
    case AccessibilityTrait::is_button:
      return "button";
    case AccessibilityTrait::is_header:
      return "header";
    case AccessibilityTrait::is_selected:
      return "selected";
    case AccessibilityTrait::is_link:
      return "link";
    case AccessibilityTrait::is_search_field:
      return "search field";
    case AccessibilityTrait::is_image:
      return "image";
    case AccessibilityTrait::plays_sound:
      return "plays sound";
    case AccessibilityTrait::is_keyboard_key:
      return "keyboard key";
    case AccessibilityTrait::is_static_text:
      return "static text";
    case AccessibilityTrait::is_summary_element:
      return "summary";
    case AccessibilityTrait::updates_frequently:
      return "updates frequently";
    case AccessibilityTrait::starts_media_session:
      return "starts media session";
    case AccessibilityTrait::allows_direct_interaction:
      return "direct interaction";
    case AccessibilityTrait::causes_page_turn:
      return "page turn";
    case AccessibilityTrait::is_modal:
      return "modal";
    case AccessibilityTrait::is_toggle:
      return "toggle";
    case AccessibilityTrait::is_tab_bar:
      return "tab bar";
  }
  return "";
}

auto AccessibilityTraits::names() const -> std::vector<std::string_view> {
  constexpr std::array all{AccessibilityTrait::is_button,
                           AccessibilityTrait::is_header,
                           AccessibilityTrait::is_selected,
                           AccessibilityTrait::is_link,
                           AccessibilityTrait::is_search_field,
                           AccessibilityTrait::is_image,
                           AccessibilityTrait::plays_sound,
                           AccessibilityTrait::is_keyboard_key,
                           AccessibilityTrait::is_static_text,
                           AccessibilityTrait::is_summary_element,
                           AccessibilityTrait::updates_frequently,
                           AccessibilityTrait::starts_media_session,
                           AccessibilityTrait::allows_direct_interaction,
                           AccessibilityTrait::causes_page_turn,
                           AccessibilityTrait::is_modal,
                           AccessibilityTrait::is_toggle,
                           AccessibilityTrait::is_tab_bar};
  std::vector<std::string_view> found;
  for (const AccessibilityTrait trait : all) {
    if (contains(trait)) {
      found.push_back(name(trait));
    }
  }
  return found;
}

}
