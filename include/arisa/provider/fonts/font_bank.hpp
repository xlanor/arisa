#pragma once

#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/fonts/provider.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace arisa::provider {

class FontBank {
 public:
  FontBank();

  auto path(SystemFont font) const noexcept -> const AssetPath&;

  auto with(SystemFont font, AssetPath path) const -> FontBank;

  static auto font_name(SystemFont font) noexcept -> std::string_view;

  friend auto operator==(const FontBank&, const FontBank&) -> bool = default;

 private:
  template <std::size_t... Index>
  static auto default_paths(std::index_sequence<Index...>)
      -> std::array<AssetPath, sizeof...(Index)> {
    return {default_path(all_system_fonts[Index])...};
  }

  static auto default_path(SystemFont font) -> AssetPath;
  static auto index(SystemFont font) noexcept -> std::size_t;

  std::array<AssetPath, all_system_fonts.size()> paths_;
};

}
