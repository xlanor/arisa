#include <arisa/provider/sdl3/sdl3_fonts.hpp>

#include <utility>

namespace arisa::provider::sdl3 {

Sdl3Fonts::Sdl3Fonts(Faces faces) noexcept : faces_(std::move(faces)) {}

auto Sdl3Fonts::create(const AssetProvider& assets, const FontBank& fonts)
    -> std::expected<Sdl3Fonts, ProviderError> {
  Faces faces;
  for (const SystemFont font : all_system_fonts) {
    auto blob = assets.read(fonts.path(font));
    if (blob.has_value() && !blob->empty()) {
      faces[std::to_underlying(font)].emplace(std::move(*blob));
    }
  }
  if (!faces[std::to_underlying(SystemFont::standard)].has_value()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return Sdl3Fonts{std::move(faces)};
}

auto Sdl3Fonts::system_font(SystemFont font) const
    -> std::expected<FontData, ProviderError> {
  const std::optional<AssetBlob>& face = faces_[std::to_underlying(font)];
  if (!face.has_value()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return FontData{face->bytes()};
}

auto Sdl3Fonts::available(SystemFont font) const -> bool {
  return faces_[std::to_underlying(font)].has_value();
}

}
