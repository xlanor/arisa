#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/runtime/overlays.hpp>

#include <array>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class PresentationsPage final : public composition::Component {
 public:
  using Flag = composition::StateHandle<bool>;
  using Note = composition::StateHandle<std::string_view>;
  using Choice = composition::StateHandle<int>;
  using Item = composition::StateHandle<std::optional<int>>;

  static constexpr composition::StateKey<bool> sheet_shown{"present-sheet",
                                                           false};
  static constexpr composition::StateKey<bool> cover_shown{"present-cover",
                                                           false};
  static constexpr composition::StateKey<bool> alert_shown{"present-alert",
                                                           false};
  static constexpr composition::StateKey<bool> nested_shown{"present-nested",
                                                            false};
  static constexpr composition::StateKey<bool> dialog_shown{"present-dialog",
                                                            false};
  static constexpr composition::StateKey<bool> popover_shown{"present-popover",
                                                             false};
  static constexpr composition::StateKey<bool> toast_shown{"present-toast",
                                                           false};
  static constexpr composition::StateKey<bool> locked{"present-locked", false};
  static constexpr composition::StateKey<int> backdrop{"present-background", 0};
  static constexpr composition::StateKey<std::optional<int>> sheet_item{
      "present-item", std::nullopt};
  static constexpr composition::StateKey<bool> painted_shown{"present-painted",
                                                             false};
  static constexpr std::string_view item_button = "present-item";
  static constexpr std::string_view item_one = "present-item-1";
  static constexpr std::string_view item_two = "present-item-2";
  static constexpr std::string_view painted_button = "present-painted";
  static constexpr composition::StateKey<bool> library_shown{"present-library",
                                                             false};
  static constexpr composition::StateKey<bool> covers_shown{"present-covers",
                                                            false};
  static constexpr std::string_view covers_button = "present-covers";
  static constexpr std::string_view library_button = "present-library";
  static constexpr composition::StateKey<std::string_view> outcome{
      "present-outcome", "nothing yet"};
  static constexpr composition::StateKey<bool> options_shown{"present-options",
                                                             false};
  static constexpr composition::StateKey<composition::PresentationDetent>
      options_detent{"present-options-detent",
                     composition::PresentationDetent::medium};
  static constexpr composition::StateKey<bool> autoplay{"present-autoplay",
                                                        true};
  static constexpr composition::StateKey<bool> load_failed{"present-load",
                                                           false};
  static constexpr composition::StateKey<bool> erase_shown{"present-erase",
                                                           false};
  static constexpr composition::StateKey<bool> erase_suppressed{
      "present-erase-suppressed", false};
  static constexpr composition::StateKey<bool> inspector_shown{
      "present-inspector", false};
  static constexpr composition::StateKey<int> flashes{"present-flashes", 0};
  static constexpr composition::StateKey<bool> edit_shown{"present-edit",
                                                          false};
  static constexpr composition::StateKey<bool> edited{"present-edited", false};
  static constexpr composition::StateKey<std::optional<int>> cover_item{
      "present-cover-item", std::nullopt};
  static constexpr composition::StateKey<std::optional<int>> popover_item{
      "present-popover-item", std::nullopt};
  static constexpr std::string_view options_button = "present-options";
  static constexpr std::string_view load_button = "present-load";
  static constexpr std::string_view erase_button = "present-erase";
  static constexpr std::string_view inspector_button = "present-inspector";
  static constexpr std::string_view flash_button = "present-flash";
  static constexpr std::string_view edit_button = "present-edit";
  static constexpr std::string_view edited_toggle = "present-edited";
  static constexpr std::string_view cover_item_button = "present-cover-item";
  static constexpr std::string_view popover_item_button =
      "present-popover-item";
  static constexpr std::string_view sheet_button = "present-sheet";
  static constexpr std::string_view cover_button = "present-cover";
  static constexpr std::string_view alert_button = "present-alert";
  static constexpr std::string_view dialog_button = "present-dialog";
  static constexpr std::string_view popover_button = "present-popover";
  static constexpr std::string_view toast_button = "present-toast";
  static constexpr std::string_view done_button = "present-done";
  static constexpr std::string_view nested_button = "present-nested";
  static constexpr std::string_view lock_button = "present-lock";
  static constexpr std::string_view background_picker = "present-background";
  static constexpr float tinted_glass = 0.3F;
  static constexpr std::array<std::string_view, 4> glass_labels{
      "Theme default", "Glass regular", "Glass clear", "Glass tinted"};
  static constexpr int choices = 12;
  static constexpr float swatch_extent = 18.0F;
  static constexpr float swatch_corner = 5.0F;
  static constexpr float popover_width = 260.0F;

  class LoadError {
   public:
    auto error_description() const -> std::string;

    auto recovery_suggestion() const -> std::string;
  };

  static auto load() -> std::expected<int, LoadError>;

  static auto background_of(int choice)
      -> std::optional<composition::PresentationBackground>;

  static auto label_of(int choice) -> std::string_view;

  static auto option_key(int choice) -> std::string;

  auto operator==(const PresentationsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element trigger)
      -> composition::Element;

  using Background = std::optional<composition::PresentationBackground>;

  static auto sheet_content(Flag shown, Flag nested, Flag lock, Note last,
                            Choice choice) -> composition::Element;

  static auto cover_content(Flag shown, Choice choice) -> composition::Element;

  static auto item_content(Item item, int shown) -> composition::Element;

  static auto painted_content(Flag shown) -> composition::Element;

  static auto popover_content(Flag shown, Choice choice)
      -> composition::Element;

  static auto picker(Choice choice) -> composition::Element;

  static auto swatch(int choice) -> composition::Element;

  static auto swatch_fill(int choice) -> theme::ColourValue;

  static auto delete_actions(Note last, const Background& background)
      -> composition::Element;

  static auto dressed(composition::Element content,
                      const Background& background) -> composition::Element;

  static auto backgrounds() -> composition::Element;

  static auto options_content(
      Flag shown,
      composition::StateHandle<composition::PresentationDetent> detent,
      Flag play) -> composition::Element;

  static auto edit_content(Flag shown, Flag changed) -> composition::Element;

  static auto inspector_content(Flag shown) -> composition::Element;

  static auto erase_actions(Note last) -> composition::Element;

  static auto flash(std::optional<runtime::Overlays> overlays)
      -> async::Task<void>;

  static auto more(composition::Context& context, Note last)
      -> std::vector<composition::Element>;
};

}
