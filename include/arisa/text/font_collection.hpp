#pragma once

#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/fonts/provider.hpp>
#include <arisa/text/face_query.hpp>
#include <arisa/text/font_entry.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/text_style.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::text {

class FontCollection {
 public:
  using Chain = std::vector<std::shared_ptr<const FontFace>>;

  static constexpr std::array bundled_weights{
      font_weight::regular, font_weight::medium, font_weight::semibold,
      font_weight::bold};

  [[nodiscard]] static auto create(std::vector<FontEntry> entries)
      -> std::expected<FontCollection, TextError>;

  [[nodiscard]] static auto load(const provider::AssetProvider& assets,
                                 const provider::FontProvider& fonts,
                                 std::vector<FontEntry> app_fonts = {})
      -> std::expected<FontCollection, TextError>;

  static auto bundled_path() -> provider::AssetPath;

  static auto symbols_path() -> provider::AssetPath;

  static constexpr float synthetic_slant = 0.2126F;

  static auto filled_symbols_path() -> provider::AssetPath;

  FontCollection(FontCollection&& other) noexcept;
  auto operator=(FontCollection&& other) noexcept -> FontCollection&;
  FontCollection(const FontCollection&) = delete;
  auto operator=(const FontCollection&) -> FontCollection& = delete;
  ~FontCollection();

  auto chain(const Locale& locale, FontWeight weight, FontDesign design) const
      -> std::shared_ptr<const Chain>;

  auto chain(const Locale& locale, const FaceQuery& query) const
      -> std::shared_ptr<const Chain>;

  auto entries() const noexcept -> std::span<const FontEntry> {
    return entries_;
  }

  static auto preferred_cjk(const Locale& locale) noexcept
      -> std::optional<FontRole>;

 private:
  class ChainCache;

  static constexpr std::size_t role_count = 8;

  explicit FontCollection(std::vector<FontEntry> entries);

  static auto role_order(FontRole cjk) noexcept
      -> std::array<FontRole, role_count>;

  struct Family {
    std::string_view name;
    Typeface typeface;
    bool italic;
    FontWeight weight;
  };

  static constexpr std::array families{
      Family{"InterItalic-Regular", Typeface::standard, true,
             font_weight::regular},
      Family{"InterItalic-Medium", Typeface::standard, true,
             font_weight::medium},
      Family{"InterItalic-SemiBold", Typeface::standard, true,
             font_weight::semibold},
      Family{"InterItalic-Bold", Typeface::standard, true, font_weight::bold},
      Family{"JetBrainsMono-Regular", Typeface::monospaced, false,
             font_weight::regular},
      Family{"JetBrainsMono-Bold", Typeface::monospaced, false,
             font_weight::bold},
      Family{"NotoSerif-Regular", Typeface::serif, false, font_weight::regular},
      Family{"NotoSerif-Bold", Typeface::serif, false, font_weight::bold},
      Family{"Nunito-Regular", Typeface::rounded, false, font_weight::regular},
      Family{"Nunito-Bold", Typeface::rounded, false, font_weight::bold}};

  auto build(FontRole cjk, const FaceQuery& query) const -> Chain;

  auto admits(const FontEntry& entry, const FaceQuery& query) const noexcept
      -> bool;

  auto adapted(const FontEntry& entry, const FaceQuery& query,
               bool slanted) const -> std::shared_ptr<const FontFace>;

  static auto family_path(std::string_view name) -> provider::AssetPath;

  static auto bundled_families(const provider::AssetProvider& assets)
      -> std::vector<FontEntry>;

  static auto role(provider::SystemFont font) noexcept -> FontRole;

  [[nodiscard]] static auto bundled(const provider::AssetProvider& assets)
      -> std::expected<std::vector<FontEntry>, TextError>;

  std::vector<FontEntry> entries_;
  std::unique_ptr<ChainCache> chains_;
};

}
