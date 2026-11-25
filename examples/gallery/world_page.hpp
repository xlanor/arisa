#pragma once

#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class WorldPage final : public composition::Component {
 public:
  class Link;

  struct Language {
    std::string_view tag;
    std::string_view name;
  };

  static constexpr std::array<Language, 8> languages{
      Language{"", "System"},    Language{"en", "English"},
      Language{"ja", "日本語"},  Language{"fr", "Français"},
      Language{"de", "Deutsch"}, Language{"es", "Español"},
      Language{"ar", "العربية"}, Language{"he", "עברית"}};
  static constexpr std::array<int, 5> file_counts{0, 1, 2, 5, 11};
  static constexpr std::chrono::sys_seconds sample_date{
      std::chrono::seconds{1'795'352'700}};
  static constexpr double sample_number = 1234567.891;
  static constexpr double sample_share = 0.256;
  static constexpr std::string_view export_name = "arisa-export.txt";
  static constexpr std::string_view export_text = "Exported from arisa";

  static constexpr std::string_view close_button = "world-close";
  static constexpr std::string_view language_picker = "world-language";
  static constexpr std::string_view number_view = "world-number";
  static constexpr std::string_view percent_view = "world-percent";
  static constexpr std::string_view date_view = "world-date";
  static constexpr std::string_view time_view = "world-time";
  static constexpr std::string_view relative_view = "world-relative";
  static constexpr std::string_view mirrored_row = "world-mirrored";
  static constexpr std::string_view fixed_row = "world-fixed";
  static constexpr std::string_view fixed_first = "world-fixed-first";
  static constexpr std::string_view fixed_last = "world-fixed-last";
  static constexpr std::string_view chevron_view = "world-chevron";
  static constexpr std::string_view flipped_view = "world-flipped";
  static constexpr std::string_view grid_view = "world-grid";
  static constexpr std::string_view slider_view = "world-slider";
  static constexpr std::string_view heading_view = "world-heading";
  static constexpr std::string_view described_button = "world-described";
  static constexpr std::string_view combined_row = "world-combined";
  static constexpr std::string_view hidden_icon = "world-hidden";
  static constexpr std::string_view contrast_button = "world-contrast";
  static constexpr std::string_view import_button = "world-import";
  static constexpr std::string_view export_button = "world-export";
  static constexpr std::string_view move_button = "world-move";
  static constexpr std::string_view file_status = "world-file-status";
  static constexpr std::string_view user_button = "world-user";
  static constexpr std::string_view players_button = "world-players";
  static constexpr std::string_view error_button = "world-error";
  static constexpr std::string_view applet_status = "world-applet-status";

  static constexpr composition::StateKey<bool> importing{"world-importing",
                                                         false};
  static constexpr composition::StateKey<bool> exporting{"world-exporting",
                                                         false};
  static constexpr composition::StateKey<bool> moving{"world-moving", false};
  static constexpr composition::StateKey<std::string> imported{"world-imported",
                                                               std::string{}};
  static constexpr composition::StateKey<std::string> file_result{
      "world-file-result", std::string{}};
  static constexpr composition::StateKey<std::string> applet_result{
      "world-applet-result", std::string{}};
  static constexpr composition::StateKey<double> slider_value{"world-slider",
                                                              0.3};
  static const composition::AppStorage<std::string> language;

  explicit WorldPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const WorldPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto count_key(int count) -> std::string;

 private:
  static auto say(std::string_view key) -> composition::Element;

  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto chooser(composition::Context& context) -> composition::Element;

  static auto formats() -> composition::Element;

  static auto mirroring(composition::Context& context) -> composition::Element;

  static auto accessibility() -> composition::Element;

  static auto appearance(composition::Context& context) -> composition::Element;

  static auto files(composition::Context& context) -> composition::Element;

  static auto applets(composition::Context& context) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class WorldPage::Link final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"world-shown", false};
  static constexpr std::string_view button = "settings-world-button";

  auto operator==(const Link&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
