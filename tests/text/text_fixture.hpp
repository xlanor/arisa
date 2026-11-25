#pragma once

#include <arisa/provider/headless/memory_fonts.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_style.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::text {

class TextFixture {
 public:
  struct Script {
    std::string_view file;
    FontRole role;
  };

  static auto read(const std::filesystem::path& path)
      -> std::vector<std::byte> {
    std::ifstream file{path, std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    return bytes;
  }

  static auto fixture_font(std::string_view file) -> std::filesystem::path {
    return std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa" / "fonts" /
           file;
  }

  static auto fonts() -> const FontCollection& {
    return *storage_instance().collection;
  }

  static auto face(FontRole role) -> std::shared_ptr<const FontFace> {
    for (const FontEntry& entry : fonts().entries()) {
      if (entry.role == role) {
        return entry.face;
      }
    }
    return nullptr;
  }

  static auto script_face(std::string_view file)
      -> std::shared_ptr<const FontFace> {
    for (const auto& [name, face] : storage_instance().scripts) {
      if (name == file) {
        return face;
      }
    }
    return nullptr;
  }

  static auto style(std::string_view locale, float size = 20.0F) -> TextStyle {
    TextStyle style;
    style.size = PointSize::create(size).value();
    style.locale = Locale::create(locale).value_or(Locale::undetermined());
    return style;
  }

  static auto layout(std::string_view text, const TextStyle& style,
                     std::optional<float> width = std::nullopt,
                     float scale = 1.0F) -> TextLayout {
    auto created = TextLayout::create(
        fonts(), text, style,
        width.has_value() ? LayoutWidth::points(*width).value()
                          : LayoutWidth::unbounded(),
        provider::SurfaceScale::create(scale).value());
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  static auto glyph_at(const TextLayout& layout, std::size_t cluster)
      -> std::optional<PositionedGlyph> {
    for (const PositionedGlyph& glyph : layout.glyphs()) {
      if (glyph.cluster == cluster) {
        return glyph;
      }
    }
    return std::nullopt;
  }

 private:
  struct Storage {
    provider::headless::MemoryFonts system;
    std::vector<std::pair<std::string_view, std::shared_ptr<const FontFace>>>
        scripts;
    std::optional<FontCollection> collection;

    Storage() {
      system.add(provider::SystemFont::standard,
                 read(fixture_font("NotoSansJP-Subset.otf")));
      system.add(provider::SystemFont::chinese_simplified,
                 read(fixture_font("NotoSansSC-Subset.otf")));
      system.add(provider::SystemFont::chinese_traditional,
                 read(fixture_font("NotoSansTC-Subset.otf")));
      system.add(provider::SystemFont::korean,
                 read(fixture_font("NotoSansKR-Subset.otf")));
      std::vector<FontEntry> app;
      for (const std::string_view file :
           {std::string_view{"NotoSansArabic-Subset.ttf"},
            std::string_view{"NotoSansHebrew-Subset.ttf"},
            std::string_view{"NotoSansThai-Subset.ttf"},
            std::string_view{"NotoSansDevanagari-Subset.ttf"}}) {
        auto blob = std::make_shared<const provider::AssetBlob>(
            read(fixture_font(file)));
        auto face = FontFace::create(blob);
        EXPECT_TRUE(face.has_value()) << file;
        auto shared = std::make_shared<const FontFace>(std::move(*face));
        scripts.emplace_back(file, shared);
        app.push_back(FontEntry{.role = FontRole::script, .face = shared});
      }
      const provider::standard::DirectoryAssets assets{
          std::filesystem::path{ARISA_SOURCE_ASSETS}};
      auto loaded = FontCollection::load(assets, system, std::move(app));
      EXPECT_TRUE(loaded.has_value());
      collection.emplace(std::move(*loaded));
    }
  };

  static auto storage_instance() -> const Storage& {
    static const Storage storage;
    return storage;
  }
};

}
