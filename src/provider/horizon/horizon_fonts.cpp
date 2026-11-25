#include <arisa/provider/horizon/horizon_fonts.hpp>

#include <cstddef>
#include <span>
#include <utility>

namespace arisa::provider::horizon {

HorizonFonts::HorizonFonts(ServiceSession<PlService> pl) noexcept
    : pl_(std::move(pl)) {}

auto HorizonFonts::create() -> std::expected<HorizonFonts, ProviderError> {
  return ServiceSession<PlService>::create().transform(
      [](ServiceSession<PlService> pl) { return HorizonFonts{std::move(pl)}; });
}

auto HorizonFonts::system_font(SystemFont font) const
    -> std::expected<FontData, ProviderError> {
  PlFontData shared{};
  const Result found = plGetSharedFontByType(&shared, shared_type(font));
  if (R_FAILED(found)) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found, found});
  }
  if (shared.address == nullptr || shared.size == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return FontData{
      std::span{static_cast<const std::byte*>(shared.address), shared.size}};
}

auto HorizonFonts::available(SystemFont font) const -> bool {
  return system_font(font).has_value();
}

auto HorizonFonts::shared_type(SystemFont font) noexcept -> PlSharedFontType {
  switch (font) {
    case SystemFont::standard:
      return PlSharedFontType_Standard;
    case SystemFont::chinese_simplified:
      return PlSharedFontType_ChineseSimplified;
    case SystemFont::chinese_simplified_extended:
      return PlSharedFontType_ExtChineseSimplified;
    case SystemFont::chinese_traditional:
      return PlSharedFontType_ChineseTraditional;
    case SystemFont::korean:
      return PlSharedFontType_KO;
    case SystemFont::nintendo_symbols:
      return PlSharedFontType_NintendoExt;
  }
  return PlSharedFontType_Standard;
}

}
