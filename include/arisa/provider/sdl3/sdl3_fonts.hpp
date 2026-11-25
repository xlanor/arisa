#pragma once

#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/fonts/font_bank.hpp>
#include <arisa/provider/fonts/provider.hpp>

#include <array>
#include <expected>
#include <optional>

namespace arisa::provider::sdl3 {

class Sdl3Fonts final : public FontProvider {
 public:
  [[nodiscard]] static auto create(const AssetProvider& assets,
                                   const FontBank& fonts = {})
      -> std::expected<Sdl3Fonts, ProviderError>;

  [[nodiscard]] auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> override;

  auto available(SystemFont font) const -> bool override;

 private:
  using Faces = std::array<std::optional<AssetBlob>, all_system_fonts.size()>;

  explicit Sdl3Fonts(Faces faces) noexcept;

  Faces faces_;
};

}
