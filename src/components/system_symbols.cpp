#include <arisa/components/system_symbols.hpp>

#include <arisa/text/utf8.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace arisa::components {

auto SystemSymbols::aliases() noexcept -> std::span<const Alias> {
  static constexpr std::array<Alias, alias_count> table{{
      Alias{.name = "airplayvideo", .glyph = "cast", .filled = false},
      Alias{.name = "alarm", .glyph = "alarm", .filled = false},
      Alias{.name = "antenna.radiowaves.left.and.right",
            .glyph = "cast",
            .filled = false},
      Alias{.name = "applewatch", .glyph = "watch", .filled = false},
      Alias{.name = "arcade.stick", .glyph = "joystick", .filled = false},
      Alias{.name = "archivebox", .glyph = "inventory_2", .filled = false},
      Alias{.name = "arrow.2.squarepath", .glyph = "repeat", .filled = false},
      Alias{.name = "arrow.backward", .glyph = "arrow_back", .filled = false},
      Alias{.name = "arrow.clockwise", .glyph = "refresh", .filled = false},
      Alias{
          .name = "arrow.counterclockwise", .glyph = "replay", .filled = false},
      Alias{.name = "arrow.down", .glyph = "arrow_downward", .filled = false},
      Alias{.name = "arrow.down.circle", .glyph = "download", .filled = false},
      Alias{.name = "arrow.down.right.and.arrow.up.left",
            .glyph = "fullscreen_exit",
            .filled = false},
      Alias{.name = "arrow.down.to.line", .glyph = "download", .filled = false},
      Alias{.name = "arrow.forward", .glyph = "arrow_forward", .filled = false},
      Alias{.name = "arrow.left", .glyph = "arrow_back", .filled = false},
      Alias{.name = "arrow.right", .glyph = "arrow_forward", .filled = false},
      Alias{.name = "arrow.triangle.2.circlepath",
            .glyph = "sync",
            .filled = false},
      Alias{.name = "arrow.up", .glyph = "arrow_upward", .filled = false},
      Alias{
          .name = "arrow.up.arrow.down", .glyph = "swap_vert", .filled = false},
      Alias{.name = "arrow.up.circle", .glyph = "upload", .filled = false},
      Alias{.name = "arrow.up.forward.app",
            .glyph = "open_in_new",
            .filled = false},
      Alias{.name = "arrow.up.left.and.arrow.down.right",
            .glyph = "fullscreen",
            .filled = false},
      Alias{.name = "arrow.up.right.square",
            .glyph = "open_in_new",
            .filled = false},
      Alias{.name = "arrow.up.to.line", .glyph = "upload", .filled = false},
      Alias{.name = "arrow.uturn.backward", .glyph = "undo", .filled = false},
      Alias{.name = "arrow.uturn.forward", .glyph = "redo", .filled = false},
      Alias{.name = "atom", .glyph = "science", .filled = false},
      Alias{.name = "backward", .glyph = "fast_rewind", .filled = false},
      Alias{.name = "backward.end", .glyph = "skip_previous", .filled = false},
      Alias{.name = "backward.end.fill",
            .glyph = "skip_previous",
            .filled = true},
      Alias{.name = "backward.fill", .glyph = "fast_rewind", .filled = true},
      Alias{.name = "battery.0", .glyph = "battery_0_bar", .filled = false},
      Alias{.name = "battery.100", .glyph = "battery_full", .filled = false},
      Alias{.name = "battery.100.bolt",
            .glyph = "battery_charging_full",
            .filled = false},
      Alias{.name = "battery.25", .glyph = "battery_2_bar", .filled = false},
      Alias{.name = "battery.50", .glyph = "battery_4_bar", .filled = false},
      Alias{.name = "battery.75", .glyph = "battery_5_bar", .filled = false},
      Alias{.name = "bell", .glyph = "notifications", .filled = false},
      Alias{.name = "bell.fill", .glyph = "notifications", .filled = true},
      Alias{.name = "bolt", .glyph = "bolt", .filled = false},
      Alias{.name = "bolt.fill", .glyph = "bolt", .filled = true},
      Alias{.name = "book", .glyph = "menu_book", .filled = false},
      Alias{.name = "bookmark", .glyph = "bookmark", .filled = false},
      Alias{.name = "bookmark.fill", .glyph = "bookmark", .filled = true},
      Alias{
          .name = "books.vertical", .glyph = "library_books", .filled = false},
      Alias{.name = "bubble.left", .glyph = "chat", .filled = false},
      Alias{.name = "cable.connector", .glyph = "usb", .filled = false},
      Alias{.name = "calendar", .glyph = "calendar_today", .filled = false},
      Alias{.name = "camera", .glyph = "photo_camera", .filled = false},
      Alias{.name = "camera.fill", .glyph = "photo_camera", .filled = true},
      Alias{.name = "cart", .glyph = "shopping_cart", .filled = false},
      Alias{.name = "cart.fill", .glyph = "shopping_cart", .filled = true},
      Alias{.name = "cellularbars",
            .glyph = "signal_cellular_alt",
            .filled = false},
      Alias{.name = "character.bubble", .glyph = "translate", .filled = false},
      Alias{.name = "checkmark", .glyph = "check", .filled = false},
      Alias{
          .name = "checkmark.circle", .glyph = "check_circle", .filled = false},
      Alias{.name = "checkmark.circle.fill",
            .glyph = "check_circle",
            .filled = true},
      Alias{.name = "checkmark.shield", .glyph = "security", .filled = false},
      Alias{
          .name = "checkmark.shield.fill", .glyph = "security", .filled = true},
      Alias{.name = "checkmark.square", .glyph = "check_box", .filled = false},
      Alias{.name = "checkmark.square.fill",
            .glyph = "check_box",
            .filled = true},
      Alias{
          .name = "chevron.backward", .glyph = "chevron_left", .filled = false},
      Alias{.name = "chevron.down", .glyph = "expand_more", .filled = false},
      Alias{
          .name = "chevron.forward", .glyph = "chevron_right", .filled = false},
      Alias{.name = "chevron.left", .glyph = "chevron_left", .filled = false},
      Alias{.name = "chevron.right", .glyph = "chevron_right", .filled = false},
      Alias{.name = "chevron.up", .glyph = "expand_less", .filled = false},
      Alias{.name = "chevron.up.chevron.down",
            .glyph = "unfold_more",
            .filled = false},
      Alias{
          .name = "circle", .glyph = "radio_button_unchecked", .filled = false},
      Alias{.name = "clock", .glyph = "schedule", .filled = false},
      Alias{.name = "clock.fill", .glyph = "schedule", .filled = true},
      Alias{.name = "cloud", .glyph = "cloud", .filled = false},
      Alias{.name = "cloud.fill", .glyph = "cloud", .filled = true},
      Alias{.name = "cpu", .glyph = "memory", .filled = false},
      Alias{.name = "desktopcomputer", .glyph = "computer", .filled = false},
      Alias{.name = "doc", .glyph = "description", .filled = false},
      Alias{.name = "doc.fill", .glyph = "description", .filled = true},
      Alias{.name = "doc.on.clipboard",
            .glyph = "content_paste",
            .filled = false},
      Alias{.name = "doc.on.doc", .glyph = "content_copy", .filled = false},
      Alias{.name = "doc.richtext", .glyph = "article", .filled = false},
      Alias{.name = "doc.text", .glyph = "article", .filled = false},
      Alias{.name = "dpad", .glyph = "gamepad", .filled = false},
      Alias{.name = "ellipsis", .glyph = "more_horiz", .filled = false},
      Alias{.name = "ellipsis.circle", .glyph = "more_horiz", .filled = false},
      Alias{.name = "ellipsis.vertical", .glyph = "more_vert", .filled = false},
      Alias{.name = "envelope", .glyph = "mail", .filled = false},
      Alias{.name = "envelope.fill", .glyph = "mail", .filled = true},
      Alias{
          .name = "exclamationmark.circle", .glyph = "error", .filled = false},
      Alias{.name = "exclamationmark.circle.fill",
            .glyph = "error",
            .filled = true},
      Alias{.name = "exclamationmark.triangle",
            .glyph = "warning",
            .filled = false},
      Alias{.name = "exclamationmark.triangle.fill",
            .glyph = "warning",
            .filled = true},
      Alias{.name = "externaldrive", .glyph = "storage", .filled = false},
      Alias{.name = "eye", .glyph = "visibility", .filled = false},
      Alias{.name = "eye.fill", .glyph = "visibility", .filled = true},
      Alias{.name = "eye.slash", .glyph = "visibility_off", .filled = false},
      Alias{.name = "face.smiling", .glyph = "mood", .filled = false},
      Alias{.name = "film", .glyph = "movie", .filled = false},
      Alias{.name = "flag", .glyph = "flag", .filled = false},
      Alias{.name = "flag.fill", .glyph = "flag", .filled = true},
      Alias{.name = "folder", .glyph = "folder", .filled = false},
      Alias{.name = "folder.fill", .glyph = "folder", .filled = true},
      Alias{.name = "forward", .glyph = "fast_forward", .filled = false},
      Alias{.name = "forward.end", .glyph = "skip_next", .filled = false},
      Alias{.name = "forward.end.fill", .glyph = "skip_next", .filled = true},
      Alias{.name = "forward.fill", .glyph = "fast_forward", .filled = true},
      Alias{
          .name = "gamecontroller", .glyph = "sports_esports", .filled = false},
      Alias{.name = "gamecontroller.fill",
            .glyph = "sports_esports",
            .filled = true},
      Alias{.name = "gear", .glyph = "settings", .filled = false},
      Alias{.name = "gearshape", .glyph = "settings", .filled = false},
      Alias{.name = "gearshape.fill", .glyph = "settings", .filled = true},
      Alias{.name = "globe", .glyph = "language", .filled = false},
      Alias{.name = "hand.thumbsdown", .glyph = "thumb_down", .filled = false},
      Alias{.name = "hand.thumbsdown.fill",
            .glyph = "thumb_down",
            .filled = true},
      Alias{.name = "hand.thumbsup", .glyph = "thumb_up", .filled = false},
      Alias{.name = "hand.thumbsup.fill", .glyph = "thumb_up", .filled = true},
      Alias{.name = "headphones", .glyph = "headphones", .filled = false},
      Alias{.name = "heart", .glyph = "favorite", .filled = false},
      Alias{.name = "heart.fill", .glyph = "favorite", .filled = true},
      Alias{.name = "hifispeaker", .glyph = "speaker", .filled = false},
      Alias{.name = "hourglass", .glyph = "schedule", .filled = false},
      Alias{.name = "house", .glyph = "home", .filled = false},
      Alias{.name = "house.fill", .glyph = "home", .filled = true},
      Alias{.name = "icloud", .glyph = "cloud", .filled = false},
      Alias{.name = "info.circle", .glyph = "info", .filled = false},
      Alias{.name = "info.circle.fill", .glyph = "info", .filled = true},
      Alias{.name = "ipad", .glyph = "tablet", .filled = false},
      Alias{.name = "iphone", .glyph = "smartphone", .filled = false},
      Alias{.name = "keyboard", .glyph = "keyboard", .filled = false},
      Alias{.name = "laptopcomputer", .glyph = "computer", .filled = false},
      Alias{.name = "largecircle.fill.circle",
            .glyph = "radio_button_checked",
            .filled = false},
      Alias{.name = "line.3.horizontal", .glyph = "menu", .filled = false},
      Alias{.name = "line.3.horizontal.decrease",
            .glyph = "filter_list",
            .filled = false},
      Alias{.name = "line.3.horizontal.decrease.circle",
            .glyph = "filter_list",
            .filled = false},
      Alias{
          .name = "line.horizontal.3", .glyph = "drag_handle", .filled = false},
      Alias{.name = "link", .glyph = "link", .filled = false},
      Alias{.name = "list.bullet",
            .glyph = "format_list_bulleted",
            .filled = false},
      Alias{.name = "list.dash", .glyph = "list", .filled = false},
      Alias{.name = "location.fill", .glyph = "location_on", .filled = true},
      Alias{.name = "lock", .glyph = "lock", .filled = false},
      Alias{.name = "lock.fill", .glyph = "lock", .filled = true},
      Alias{.name = "lock.open", .glyph = "lock_open", .filled = false},
      Alias{.name = "magnifyingglass", .glyph = "search", .filled = false},
      Alias{.name = "map", .glyph = "map", .filled = false},
      Alias{.name = "mappin", .glyph = "location_on", .filled = false},
      Alias{.name = "mappin.and.ellipse",
            .glyph = "location_on",
            .filled = false},
      Alias{.name = "memorychip", .glyph = "memory", .filled = false},
      Alias{.name = "message", .glyph = "chat", .filled = false},
      Alias{.name = "mic", .glyph = "mic", .filled = false},
      Alias{.name = "mic.fill", .glyph = "mic", .filled = true},
      Alias{.name = "minus", .glyph = "remove", .filled = false},
      Alias{.name = "minus.circle", .glyph = "remove_circle", .filled = false},
      Alias{.name = "minus.circle.fill",
            .glyph = "remove_circle",
            .filled = true},
      Alias{.name = "minus.magnifyingglass",
            .glyph = "zoom_out",
            .filled = false},
      Alias{.name = "moon", .glyph = "dark_mode", .filled = false},
      Alias{.name = "moon.fill", .glyph = "dark_mode", .filled = true},
      Alias{.name = "music.note", .glyph = "music_note", .filled = false},
      Alias{.name = "network", .glyph = "lan", .filled = false},
      Alias{.name = "newspaper", .glyph = "article", .filled = false},
      Alias{.name = "nosign", .glyph = "block", .filled = false},
      Alias{.name = "number", .glyph = "tag", .filled = false},
      Alias{.name = "paintpalette", .glyph = "palette", .filled = false},
      Alias{.name = "paintpalette.fill", .glyph = "palette", .filled = true},
      Alias{.name = "paperclip", .glyph = "attach_file", .filled = false},
      Alias{.name = "party.popper", .glyph = "celebration", .filled = false},
      Alias{.name = "pause", .glyph = "pause", .filled = false},
      Alias{.name = "pause.fill", .glyph = "pause", .filled = true},
      Alias{.name = "pencil", .glyph = "edit", .filled = false},
      Alias{.name = "person", .glyph = "person", .filled = false},
      Alias{.name = "person.2", .glyph = "group", .filled = false},
      Alias{.name = "person.2.fill", .glyph = "group", .filled = true},
      Alias{
          .name = "person.badge.plus", .glyph = "person_add", .filled = false},
      Alias{
          .name = "person.circle", .glyph = "account_circle", .filled = false},
      Alias{.name = "person.circle.fill",
            .glyph = "account_circle",
            .filled = true},
      Alias{.name = "person.crop.circle",
            .glyph = "account_circle",
            .filled = false},
      Alias{.name = "person.crop.circle.badge.checkmark",
            .glyph = "login",
            .filled = false},
      Alias{.name = "person.crop.circle.fill",
            .glyph = "account_circle",
            .filled = true},
      Alias{.name = "person.fill", .glyph = "person", .filled = true},
      Alias{.name = "phone", .glyph = "call", .filled = false},
      Alias{.name = "phone.fill", .glyph = "call", .filled = true},
      Alias{.name = "photo", .glyph = "image", .filled = false},
      Alias{.name = "photo.fill", .glyph = "image", .filled = true},
      Alias{.name = "photo.on.rectangle",
            .glyph = "photo_library",
            .filled = false},
      Alias{.name = "pin", .glyph = "push_pin", .filled = false},
      Alias{.name = "pin.fill", .glyph = "push_pin", .filled = true},
      Alias{.name = "play", .glyph = "play_arrow", .filled = false},
      Alias{.name = "play.fill", .glyph = "play_arrow", .filled = true},
      Alias{.name = "plus", .glyph = "add", .filled = false},
      Alias{.name = "plus.circle", .glyph = "add_circle", .filled = false},
      Alias{.name = "plus.circle.fill", .glyph = "add_circle", .filled = true},
      Alias{
          .name = "plus.magnifyingglass", .glyph = "zoom_in", .filled = false},
      Alias{.name = "power", .glyph = "power_settings_new", .filled = false},
      Alias{.name = "printer", .glyph = "print", .filled = false},
      Alias{.name = "puzzlepiece", .glyph = "widgets", .filled = false},
      Alias{.name = "questionmark.circle", .glyph = "help", .filled = false},
      Alias{
          .name = "questionmark.circle.fill", .glyph = "help", .filled = true},
      Alias{.name = "record.circle",
            .glyph = "radio_button_checked",
            .filled = false},
      Alias{
          .name = "rectangle.grid.1x2", .glyph = "view_list", .filled = false},
      Alias{.name = "rectangle.portrait.and.arrow.right",
            .glyph = "logout",
            .filled = false},
      Alias{.name = "repeat", .glyph = "repeat", .filled = false},
      Alias{.name = "rocket", .glyph = "rocket_launch", .filled = false},
      Alias{.name = "rosette", .glyph = "emoji_events", .filled = false},
      Alias{.name = "safari", .glyph = "explore", .filled = false},
      Alias{.name = "sdcard", .glyph = "sd_card", .filled = false},
      Alias{.name = "shield", .glyph = "shield", .filled = false},
      Alias{.name = "shield.fill", .glyph = "shield", .filled = true},
      Alias{.name = "shippingbox", .glyph = "inventory_2", .filled = false},
      Alias{.name = "shuffle", .glyph = "shuffle", .filled = false},
      Alias{.name = "sidebar.leading",
            .glyph = "left_panel_open",
            .filled = false},
      Alias{
          .name = "sidebar.left", .glyph = "left_panel_open", .filled = false},
      Alias{.name = "slider.horizontal.3", .glyph = "tune", .filled = false},
      Alias{.name = "sparkles", .glyph = "new_releases", .filled = false},
      Alias{.name = "speaker.slash", .glyph = "volume_off", .filled = false},
      Alias{.name = "speaker.wave.1", .glyph = "volume_down", .filled = false},
      Alias{.name = "speaker.wave.2", .glyph = "volume_up", .filled = false},
      Alias{.name = "speaker.wave.3", .glyph = "volume_up", .filled = false},
      Alias{
          .name = "speaker.wave.3.fill", .glyph = "volume_up", .filled = true},
      Alias{.name = "square",
            .glyph = "check_box_outline_blank",
            .filled = false},
      Alias{.name = "square.and.arrow.down",
            .glyph = "download",
            .filled = false},
      Alias{.name = "square.and.arrow.up", .glyph = "share", .filled = false},
      Alias{.name = "square.and.pencil", .glyph = "edit", .filled = false},
      Alias{.name = "square.grid.2x2", .glyph = "grid_view", .filled = false},
      Alias{
          .name = "square.grid.2x2.fill", .glyph = "grid_view", .filled = true},
      Alias{.name = "square.grid.3x3", .glyph = "apps", .filled = false},
      Alias{.name = "square.stack", .glyph = "category", .filled = false},
      Alias{.name = "star", .glyph = "star", .filled = false},
      Alias{.name = "star.fill", .glyph = "star", .filled = true},
      Alias{.name = "stop", .glyph = "stop", .filled = false},
      Alias{.name = "stop.fill", .glyph = "stop", .filled = true},
      Alias{.name = "stopwatch", .glyph = "timer", .filled = false},
      Alias{.name = "sun.max", .glyph = "light_mode", .filled = false},
      Alias{.name = "sun.max.fill", .glyph = "light_mode", .filled = true},
      Alias{.name = "sun.min", .glyph = "brightness_low", .filled = false},
      Alias{.name = "tag", .glyph = "sell", .filled = false},
      Alias{.name = "tag.fill", .glyph = "sell", .filled = true},
      Alias{.name = "textformat", .glyph = "text_fields", .filled = false},
      Alias{.name = "timer", .glyph = "timer", .filled = false},
      Alias{.name = "trash", .glyph = "delete", .filled = false},
      Alias{.name = "trash.fill", .glyph = "delete", .filled = true},
      Alias{.name = "tray", .glyph = "inventory_2", .filled = false},
      Alias{.name = "trophy", .glyph = "trophy", .filled = false},
      Alias{.name = "trophy.fill", .glyph = "trophy", .filled = true},
      Alias{.name = "tv", .glyph = "tv", .filled = false},
      Alias{.name = "video", .glyph = "videocam", .filled = false},
      Alias{.name = "video.fill", .glyph = "videocam", .filled = true},
      Alias{.name = "wifi", .glyph = "wifi", .filled = false},
      Alias{.name = "wifi.exclamationmark",
            .glyph = "wifi_find",
            .filled = false},
      Alias{.name = "wifi.slash", .glyph = "wifi_off", .filled = false},
      Alias{.name = "xmark", .glyph = "close", .filled = false},
      Alias{.name = "xmark.circle", .glyph = "cancel", .filled = false},
      Alias{.name = "xmark.circle.fill", .glyph = "cancel", .filled = true},
      Alias{.name = "xmark.octagon", .glyph = "report", .filled = false},
      Alias{.name = "xmark.octagon.fill", .glyph = "report", .filled = true},
  }};
  return table;
}

auto SystemSymbols::recipes() noexcept -> std::span<const Recipe> {
  using Tier = graphics::HierarchicalShapeStyle;
  using theme::ColourRole;
  static constexpr std::array<Recipe, recipe_count> table{{
      Recipe{.name = "checkmark.circle.fill",
             .layers = {Layer{.glyph = "check_circle",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::green},
                        Layer{.glyph = "check",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 0.7F,
                              .multicolour = ColourRole::on_accent}},
             .count = 2},
      Recipe{.name = "exclamationmark.circle.fill",
             .layers = {Layer{.glyph = "error",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt},
                        Layer{.glyph = "error",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt}},
             .count = 2},
      Recipe{.name = "exclamationmark.triangle.fill",
             .layers = {Layer{.glyph = "warning",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::yellow},
                        Layer{.glyph = "warning",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt}},
             .count = 2},
      Recipe{.name = "heart.fill",
             .layers = {Layer{.glyph = "favorite",
                              .filled = true,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::red},
                        Layer{}},
             .count = 1},
      Recipe{.name = "info.circle.fill",
             .layers = {Layer{.glyph = "info",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt},
                        Layer{.glyph = "info",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt}},
             .count = 2},
      Recipe{.name = "minus.circle.fill",
             .layers = {Layer{.glyph = "remove_circle",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::red},
                        Layer{.glyph = "remove",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 0.72F,
                              .multicolour = ColourRole::on_accent}},
             .count = 2},
      Recipe{.name = "person.crop.circle.fill",
             .layers = {Layer{.glyph = "account_circle",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt},
                        Layer{.glyph = "account_circle",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt}},
             .count = 2},
      Recipe{.name = "plus.circle.fill",
             .layers = {Layer{.glyph = "add_circle",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::green},
                        Layer{.glyph = "add",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 0.72F,
                              .multicolour = ColourRole::on_accent}},
             .count = 2},
      Recipe{.name = "questionmark.circle.fill",
             .layers = {Layer{.glyph = "help",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt},
                        Layer{.glyph = "help",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt}},
             .count = 2},
      Recipe{.name = "star.fill",
             .layers = {Layer{.glyph = "star",
                              .filled = true,
                              .tier = Tier::primary,
                              .scale = 1.0F,
                              .multicolour = ColourRole::yellow},
                        Layer{}},
             .count = 1},
      Recipe{.name = "xmark.circle.fill",
             .layers = {Layer{.glyph = "cancel",
                              .filled = true,
                              .tier = Tier::secondary,
                              .scale = 1.0F,
                              .multicolour = std::nullopt},
                        Layer{.glyph = "close",
                              .filled = false,
                              .tier = Tier::primary,
                              .scale = 0.6F,
                              .multicolour = std::nullopt}},
             .count = 2},
  }};
  return table;
}

auto SystemSymbols::series() noexcept -> std::span<const Series> {
  static constexpr std::array<Series, series_count> table{{
      Series{.name = "battery.100",
             .glyphs = {"battery_0_bar", "battery_1_bar", "battery_2_bar",
                        "battery_3_bar", "battery_4_bar", "battery_5_bar",
                        "battery_6_bar", "battery_full"},
             .count = 8},
      Series{.name = "speaker.wave.2",
             .glyphs = {"volume_down", "volume_up"},
             .count = 2},
      Series{.name = "speaker.wave.3",
             .glyphs = {"volume_down", "volume_up"},
             .count = 2},
      Series{.name = "speaker.wave.3.fill",
             .glyphs = {"volume_down", "volume_up"},
             .count = 2},
      Series{.name = "wifi",
             .glyphs = {"signal_wifi_0_bar", "network_wifi_1_bar",
                        "network_wifi_2_bar", "network_wifi_3_bar",
                        "signal_wifi_4_bar"},
             .count = 5},
  }};
  return table;
}

auto SystemSymbols::alias(std::string_view name) noexcept
    -> std::optional<Alias> {
  const std::span<const Alias> table = aliases();
  const auto found = std::ranges::lower_bound(table, name, {}, &Alias::name);
  if (found == table.end() || found->name != name) {
    return std::nullopt;
  }
  return *found;
}

auto SystemSymbols::index(std::string_view name) noexcept
    -> std::optional<std::size_t> {
  const auto found = std::ranges::lower_bound(names, name);
  if (found == names.end() || *found != name) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::ranges::distance(names.begin(), found));
}

auto SystemSymbols::codepoint(std::string_view name) noexcept
    -> std::optional<char32_t> {
  return index(name).transform([](std::size_t found) {
    return first_codepoint + static_cast<char32_t>(found);
  });
}

auto SystemSymbols::text(std::string_view name) -> std::optional<std::string> {
  return codepoint(name).transform(
      [](char32_t value) { return arisa::text::Utf8::encode(value); });
}

auto SystemSymbols::glyph(std::string_view name, bool filled)
    -> std::optional<std::string> {
  return index(name).transform([filled](std::size_t found) {
    return arisa::text::Utf8::encode(
        (filled ? filled_first_codepoint : first_codepoint) +
        static_cast<char32_t>(found));
  });
}

auto SystemSymbols::suffix(composition::SymbolVariants shape) noexcept
    -> std::string_view {
  if (shape.contains(composition::SymbolVariants::circle)) {
    return "_circle";
  }
  if (shape.contains(composition::SymbolVariants::square)) {
    return "_box";
  }
  return {};
}

auto SystemSymbols::variant_names(std::string_view name,
                                  composition::SymbolVariants variants)
    -> std::vector<std::string> {
  std::string shaped;
  if (variants.contains(composition::SymbolVariants::circle)) {
    shaped = ".circle";
  } else if (variants.contains(composition::SymbolVariants::square)) {
    shaped = ".square";
  } else if (variants.contains(composition::SymbolVariants::rectangle)) {
    shaped = ".rectangle";
  }
  const std::string slashed =
      variants.contains(composition::SymbolVariants::slash) ? ".slash" : "";
  const std::string filled =
      variants.contains(composition::SymbolVariants::fill) ? ".fill" : "";
  const std::string base{name};
  std::vector<std::string> candidates{base + shaped + slashed + filled,
                                      base + shaped + filled,
                                      base + slashed + filled,
                                      base + filled,
                                      base + shaped + slashed,
                                      base + shaped,
                                      base + slashed,
                                      base};
  const auto [first, last] = std::ranges::unique(candidates);
  candidates.erase(first, last);
  return candidates;
}

auto SystemSymbols::resolve(std::string_view name,
                            composition::SymbolVariants variants)
    -> std::optional<Symbol> {
  const bool fill = variants.contains(composition::SymbolVariants::fill);
  for (const std::string& candidate : variant_names(name, variants)) {
    const std::optional<Alias> found = alias(candidate);
    if (!found.has_value()) {
      continue;
    }
    const bool filled = found->filled || fill;
    std::optional<std::string> base = glyph(found->glyph, filled);
    if (!base.has_value()) {
      continue;
    }
    Symbol made{.base = std::move(*base), .layers = {}, .series = {}};
    const auto recipe = std::ranges::find(
        recipes(), std::string_view{candidate}, &Recipe::name);
    if (recipe != recipes().end()) {
      for (std::uint8_t at = 0; at < recipe->count; ++at) {
        const Layer& layer = recipe->layers.at(at);
        if (std::optional<std::string> text =
                glyph(layer.glyph, layer.filled)) {
          made.layers.push_back(Glyph{.text = std::move(*text),
                                      .tier = layer.tier,
                                      .scale = layer.scale,
                                      .multicolour = layer.multicolour});
        }
      }
    }
    const auto ladder =
        std::ranges::find(series(), std::string_view{candidate}, &Series::name);
    if (ladder != series().end()) {
      for (std::uint8_t at = 0; at < ladder->count; ++at) {
        if (std::optional<std::string> text =
                glyph(ladder->glyphs.at(at), filled)) {
          made.series.push_back(std::move(*text));
        }
      }
    }
    return made;
  }
  if (const std::optional<char32_t> point = codepoint(name, variants)) {
    return Symbol{
        .base = arisa::text::Utf8::encode(*point), .layers = {}, .series = {}};
  }
  return std::nullopt;
}

auto SystemSymbols::known(std::string_view name) -> bool {
  return alias(name).has_value() || index(name).has_value();
}

auto SystemSymbols::codepoint(std::string_view name,
                              composition::SymbolVariants variants)
    -> std::optional<char32_t> {
  const std::string_view shaped = suffix(variants.shape());
  const std::string_view slashed =
      variants.contains(composition::SymbolVariants::slash) ? "_off" : "";
  const std::array candidates{
      std::string{name} + std::string{shaped} + std::string{slashed},
      std::string{name} + std::string{shaped},
      std::string{name} + std::string{slashed}, std::string{name}};
  for (const std::string& candidate : candidates) {
    if (const auto found = index(candidate)) {
      const char32_t first =
          variants.contains(composition::SymbolVariants::fill)
              ? filled_first_codepoint
              : first_codepoint;
      return first + static_cast<char32_t>(*found);
    }
  }
  return std::nullopt;
}

auto SystemSymbols::text(std::string_view name,
                         composition::SymbolVariants variants)
    -> std::optional<std::string> {
  return codepoint(name, variants).transform([](char32_t value) {
    return arisa::text::Utf8::encode(value);
  });
}

}
