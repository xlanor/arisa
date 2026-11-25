#include <arisa/provider/fonts/font_bank.hpp>

#include <format>

namespace arisa::provider {

FontBank::FontBank()
    : paths_(
          default_paths(std::make_index_sequence<all_system_fonts.size()>{})) {}

auto FontBank::path(SystemFont font) const noexcept -> const AssetPath& {
  return paths_[index(font)];
}

auto FontBank::with(SystemFont font, AssetPath path) const -> FontBank {
  FontBank updated = *this;
  updated.paths_[index(font)] = std::move(path);
  return updated;
}

auto FontBank::font_name(SystemFont font) noexcept -> std::string_view {
  switch (font) {
    case SystemFont::standard:
      return "standard";
    case SystemFont::chinese_simplified:
      return "chinese_simplified";
    case SystemFont::chinese_simplified_extended:
      return "chinese_simplified_extended";
    case SystemFont::chinese_traditional:
      return "chinese_traditional";
    case SystemFont::korean:
      return "korean";
    case SystemFont::nintendo_symbols:
      return "nintendo_symbols";
  }
  return "unknown";
}

auto FontBank::default_path(SystemFont font) -> AssetPath {
  return *AssetPath::create(std::format("arisa/fonts/{}.ttf", font_name(font)));
}

auto FontBank::index(SystemFont font) noexcept -> std::size_t {
  return std::to_underlying(font);
}

}
