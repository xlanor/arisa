#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::examples {

enum class GalleryPage : std::uint8_t {
  typography,
  multilingual,
  cards,
  layout,
  themes,
  counter,
  motion,
  effects,
  focus,
  scroll,
  navigation,
  presentations,
  controls,
  lists,
  icons,
  colours,
  fidelity,
  theming,
  settings,
  paths,
  feed,
  launch,
  materials,
  glass,
  feedback,
  panels,
  crashes,
};

class GalleryPages {
 public:
  static constexpr std::array all{
      GalleryPage::typography, GalleryPage::multilingual,
      GalleryPage::cards,      GalleryPage::layout,
      GalleryPage::themes,     GalleryPage::counter,
      GalleryPage::motion,     GalleryPage::effects,
      GalleryPage::focus,      GalleryPage::scroll,
      GalleryPage::navigation, GalleryPage::presentations,
      GalleryPage::controls,   GalleryPage::lists,
      GalleryPage::icons,      GalleryPage::colours,
      GalleryPage::fidelity,   GalleryPage::theming,
      GalleryPage::settings,   GalleryPage::paths,
      GalleryPage::feedback,   GalleryPage::feed,
      GalleryPage::launch,     GalleryPage::materials,
      GalleryPage::glass,      GalleryPage::panels,
      GalleryPage::crashes};

  static auto name(GalleryPage page) noexcept -> std::string_view;

  static auto title(GalleryPage page) noexcept -> std::string_view;

  static auto symbol(GalleryPage page) noexcept -> std::string_view;

  static auto parse(std::string_view name) noexcept
      -> std::optional<GalleryPage>;

  static auto next(GalleryPage page, bool forward) noexcept -> GalleryPage;
};

}
