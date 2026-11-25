#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/theme/typography.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class TileRow : public composition::Element {
 public:
  class Content;

  struct Tile {
    std::string title;
    std::string system_image;
    std::string hint;
    std::function<void()> action;
    bool prominent{false};
  };

  struct Options {
    float tile_width{168.0F};
    float tile_height{112.0F};
    std::optional<std::size_t> default_focus{};
    theme::Font label_font{theme::Font::headline};
    std::size_t label_lines{1};
  };

  static constexpr float tile_spacing = 12.0F;
  static constexpr float tile_radius = 18.0F;
  static constexpr float icon_size = 28.0F;
  static constexpr float label_spacing = 10.0F;
  static constexpr float hint_spacing = 14.0F;
  static constexpr std::size_t hint_lines = 2;
  static constexpr std::string_view tile_prefix = "arisa.tile";
  static constexpr std::string_view hint_key = "arisa.tile.hint";
  static constexpr composition::FocusState<std::size_t> tile_focus{
      "arisa-tile-row-focus"};

  explicit TileRow(std::vector<Tile> tiles);

  TileRow(std::vector<Tile> tiles, Options options);

  static auto tile_key(std::size_t index) -> std::string;
};

class TileRow::Content final : public composition::ElementContent {
 public:
  Content(std::vector<Tile> tiles, Options options) noexcept
      : tiles_(std::move(tiles)), options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto tiles() const noexcept -> const std::vector<Tile>& { return tiles_; }

 private:
  auto tile(std::size_t index,
            const composition::FocusBinding<std::size_t>& focus) const
      -> composition::Element;

  std::vector<Tile> tiles_;
  Options options_;
};

}
