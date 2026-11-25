#pragma once

#include <arisa/composition/symbol_variants.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/colour_value.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class SystemSymbols {
 public:
  struct Alias {
    std::string_view name;
    std::string_view glyph;
    bool filled{false};
  };

  struct Layer {
    std::string_view glyph;
    bool filled{false};
    graphics::HierarchicalShapeStyle tier{
        graphics::HierarchicalShapeStyle::primary};
    float scale{1.0F};
    std::optional<theme::ColourRole> multicolour{};
  };

  struct Recipe {
    std::string_view name;
    std::array<Layer, 2> layers;
    std::uint8_t count{1};
  };

  struct Series {
    std::string_view name;
    std::array<std::string_view, 8> glyphs;
    std::uint8_t count{0};
  };

  struct Glyph {
    std::string text;
    graphics::HierarchicalShapeStyle tier{
        graphics::HierarchicalShapeStyle::primary};
    float scale{1.0F};
    std::optional<theme::ColourRole> multicolour{};
  };

  struct Symbol {
    std::string base;
    std::vector<Glyph> layers;
    std::vector<std::string> series;
  };

  static constexpr char32_t first_codepoint = 0x100000;
  static constexpr char32_t filled_first_codepoint = 0x101000;

  static constexpr std::array<std::string_view, 204> names{
      "account_circle",
      "add",
      "add_circle",
      "alarm",
      "apps",
      "arrow_back",
      "arrow_downward",
      "arrow_forward",
      "arrow_upward",
      "article",
      "attach_file",
      "battery_0_bar",
      "battery_1_bar",
      "battery_2_bar",
      "battery_3_bar",
      "battery_4_bar",
      "battery_5_bar",
      "battery_6_bar",
      "battery_alert",
      "battery_charging_full",
      "battery_full",
      "battery_unknown",
      "block",
      "bluetooth",
      "bolt",
      "bookmark",
      "brightness_high",
      "brightness_low",
      "broken_image",
      "calendar_today",
      "call",
      "cancel",
      "cast",
      "category",
      "celebration",
      "chat",
      "check",
      "check_box",
      "check_box_outline_blank",
      "check_circle",
      "chevron_left",
      "chevron_right",
      "close",
      "cloud",
      "computer",
      "content_copy",
      "content_paste",
      "dark_mode",
      "dashboard",
      "delete",
      "description",
      "devices",
      "done",
      "done_all",
      "download",
      "drag_handle",
      "drag_indicator",
      "edit",
      "emoji_events",
      "error",
      "expand_less",
      "expand_more",
      "explore",
      "fast_forward",
      "fast_rewind",
      "favorite",
      "filter_list",
      "flag",
      "folder",
      "format_list_bulleted",
      "fullscreen",
      "fullscreen_exit",
      "gamepad",
      "grid_view",
      "group",
      "headphones",
      "help",
      "history",
      "home",
      "image",
      "info",
      "inventory_2",
      "joystick",
      "keyboard",
      "keyboard_arrow_down",
      "keyboard_arrow_left",
      "keyboard_arrow_right",
      "keyboard_arrow_up",
      "label",
      "lan",
      "language",
      "left_panel_close",
      "left_panel_open",
      "library_books",
      "light_mode",
      "link",
      "list",
      "location_on",
      "lock",
      "lock_open",
      "login",
      "logout",
      "mail",
      "map",
      "memory",
      "menu",
      "menu_book",
      "mic",
      "mood",
      "more_horiz",
      "more_vert",
      "mouse",
      "movie",
      "music_note",
      "network_wifi_1_bar",
      "network_wifi_2_bar",
      "network_wifi_3_bar",
      "new_releases",
      "notifications",
      "open_in_new",
      "palette",
      "pause",
      "person",
      "person_add",
      "photo_camera",
      "photo_library",
      "play_arrow",
      "power_settings_new",
      "print",
      "public",
      "push_pin",
      "radio_button_checked",
      "radio_button_unchecked",
      "redo",
      "refresh",
      "remove",
      "remove_circle",
      "reorder",
      "repeat",
      "replay",
      "report",
      "restart_alt",
      "rocket_launch",
      "save",
      "schedule",
      "science",
      "sd_card",
      "search",
      "security",
      "sell",
      "settings",
      "share",
      "shield",
      "shopping_cart",
      "shuffle",
      "signal_cellular_alt",
      "signal_wifi_0_bar",
      "signal_wifi_4_bar",
      "signal_wifi_off",
      "skip_next",
      "skip_previous",
      "smartphone",
      "sort",
      "speaker",
      "sports_esports",
      "stadia_controller",
      "star",
      "stop",
      "storage",
      "swap_vert",
      "sync",
      "tablet",
      "tag",
      "text_fields",
      "thumb_down",
      "thumb_up",
      "timer",
      "toggle_off",
      "toggle_on",
      "translate",
      "trophy",
      "tune",
      "tv",
      "undo",
      "unfold_more",
      "update",
      "upload",
      "usb",
      "videocam",
      "videogame_asset",
      "view_list",
      "visibility",
      "visibility_off",
      "volume_down",
      "volume_off",
      "volume_up",
      "warning",
      "watch",
      "widgets",
      "wifi",
      "wifi_find",
      "wifi_off",
      "zoom_in",
      "zoom_out",
  };

  static constexpr std::size_t alias_count = 253;
  static constexpr std::size_t recipe_count = 11;
  static constexpr std::size_t series_count = 5;

  static auto aliases() noexcept -> std::span<const Alias>;

  static auto recipes() noexcept -> std::span<const Recipe>;

  static auto series() noexcept -> std::span<const Series>;

  static auto alias(std::string_view name) noexcept -> std::optional<Alias>;

  static auto resolve(std::string_view name,
                      composition::SymbolVariants variants)
      -> std::optional<Symbol>;

  static auto known(std::string_view name) -> bool;

  static auto codepoint(std::string_view name) noexcept
      -> std::optional<char32_t>;

  static auto text(std::string_view name) -> std::optional<std::string>;

  static auto codepoint(std::string_view name,
                        composition::SymbolVariants variants)
      -> std::optional<char32_t>;

  static auto text(std::string_view name, composition::SymbolVariants variants)
      -> std::optional<std::string>;

 private:
  static auto index(std::string_view name) noexcept
      -> std::optional<std::size_t>;

  static auto suffix(composition::SymbolVariants shape) noexcept
      -> std::string_view;

  static auto glyph(std::string_view name, bool filled)
      -> std::optional<std::string>;

  static auto variant_names(std::string_view name,
                            composition::SymbolVariants variants)
      -> std::vector<std::string>;
};

}
