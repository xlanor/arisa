#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/seed.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>
#include <colours_page.hpp>
#include <controls_page.hpp>
#include <effects_page.hpp>
#include <fidelity_page.hpp>
#include <focus_page.hpp>
#include <frame_meter.hpp>
#include <gallery_page.hpp>
#include <icons_page.hpp>
#include <lists_page.hpp>
#include <motion_page.hpp>
#include <navigation_page.hpp>
#include <presentations_page.hpp>
#include <scroll_page.hpp>
#include <settings_page.hpp>
#include <theming_page.hpp>
#include <typography_page.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <arisa/composition/overlay_layer.hpp>
#include <feed_page.hpp>
#include <feedback_page.hpp>
#include <glass_page.hpp>
#include <launch_page.hpp>
#include <launch_screen.hpp>
#include <materials_page.hpp>

namespace arisa::examples {

class GalleryScreen final : public composition::Component {
 public:
  static constexpr composition::StateKey<std::optional<GalleryPage>> selected{
      "page", std::nullopt};
  static constexpr composition::StateKey<int> clicks{"clicks", 0};
  static constexpr composition::StateKey<std::optional<theme::ColorScheme>>
      appearance{"appearance", std::nullopt};
  static constexpr std::string_view scheme_button = "scheme";
  static constexpr std::string_view strings = "gallery/strings";
  static constexpr std::string_view increment_button = "increment";
  static constexpr std::string_view reset_button = "reset";
  static constexpr std::string_view theme_menu = "theme-menu";
  static constexpr std::string_view frame_leading = "frame-leading";
  static constexpr std::string_view frame_center = "frame-center";
  static constexpr std::string_view frame_trailing = "frame-trailing";
  static constexpr std::string_view ideal_strip = "ideal-strip";
  static constexpr std::string_view hidden_tile = "hidden-tile";
  static constexpr std::string_view grid_table = "layout-grid-table";

  using NoticeRef = std::optional<
      std::reference_wrapper<const composition::Observable<std::string>>>;

  static constexpr composition::StateKey<std::optional<std::string>> notice{
      "notice", std::nullopt};

  static constexpr composition::StateKey<std::optional<LaunchScreen::Style>>
      replaying{"replaying", std::nullopt};
  static constexpr composition::StateKey<bool> launched{"launched", false};
  static constexpr composition::StateKey<bool> warmed{"warm-theme", false};
  static const composition::StateKey<std::vector<FeedPage::Item>> feed_items;
  static constexpr composition::StateKey<std::optional<int>> feed_position{
      "feed-position", std::nullopt};
  static constexpr composition::OverlayLayer launch_layer{450};

  explicit GalleryScreen(
      GalleryPage initial, theme::Seed seed = theme::Seed{},
      MotionPage::MeterRef meter = std::nullopt,
      NoticeRef notices = std::nullopt,
      std::optional<LaunchScreen::Style> launch = std::nullopt,
      LaunchScreen::Font lettering = nullptr,
      LaunchScreen::Photo photo = nullptr) noexcept
      : initial_(initial),
        seed_(seed),
        meter_(meter),
        notices_(notices),
        launch_(launch),
        lettering_(std::move(lettering)),
        photo_(std::move(photo)) {}

  auto operator==(const GalleryScreen& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto policy(std::optional<theme::ColorScheme> scheme)
      -> arisa::appearance::AppearancePolicy;

 private:
  struct Shared {
    composition::StateHandle<int> count;
    MotionPage::MeterRef meter;
    composition::StateHandle<std::vector<FeedPage::Item>> feed;
    composition::StateHandle<std::optional<int>> feed_position;
    LaunchPage::Replay replay;
    bool lettering_font{false};
    composition::StateHandle<bool> warm;
    LaunchScreen::Photo photo;
  };

  static auto tabs(GalleryPage shown,
                   composition::StateHandle<std::optional<GalleryPage>> chosen,
                   const Shared& shared) -> composition::Element;

  static auto flip(
      composition::StateHandle<std::optional<theme::ColorScheme>> scheme,
      theme::ColorScheme current) -> void;

  static auto flip(const arisa::appearance::AppearanceModel& model,
                   theme::ColorScheme current) -> void;

  static auto theme_picker(const arisa::appearance::AppearanceModel& model)
      -> composition::Element;

  static auto scheme_label(theme::ColorScheme current) -> std::string_view;

  static auto page(GalleryPage shown, const Shared& shared)
      -> composition::Element;

  static auto languages() -> composition::Element;

  static auto cards() -> composition::Element;

  static auto layouts() -> composition::Element;

  static auto frames() -> composition::Element;

  static auto table() -> composition::Element;

  static auto themes() -> composition::Element;

  static auto counter(composition::StateHandle<int> count)
      -> composition::Element;

  static auto tally(composition::StateHandle<int> count)
      -> composition::Element;

  static auto card(std::string_view title, theme::Elevation elevation,
                   theme::Shape shape) -> composition::Element;

  template <class... Attributes>
  static auto swatch(composition::Length wide, composition::Length tall,
                     theme::ColourValue fill, theme::Shape shape,
                     Attributes&&... attributes) -> composition::Element;

  static auto preview(std::string_view name, theme::ColorScheme scheme)
      -> composition::Element;

  template <class... Attributes>
  static auto caption(std::string_view label, Attributes&&... attributes)
      -> composition::Element;

  GalleryPage initial_;
  theme::Seed seed_;
  MotionPage::MeterRef meter_;
  NoticeRef notices_;
  std::optional<LaunchScreen::Style> launch_;
  LaunchScreen::Font lettering_;
  LaunchScreen::Photo photo_;
};

}
