#pragma once

#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/audio/pcm_sound.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/diagnostics/provider.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::provider {

class SoundBank {
 public:
  using Sounds = std::array<std::optional<PcmSound>, all_sound_roles.size()>;

  SoundBank();

  auto path(SoundRole role) const noexcept -> const AssetPath&;

  auto with(SoundRole role, AssetPath path) const -> SoundBank;

  auto load(const AssetProvider& assets, DiagnosticsProvider& diagnostics,
            std::string_view category) const -> Sounds;

  static auto role_name(SoundRole role) noexcept -> std::string_view;

  friend auto operator==(const SoundBank&, const SoundBank&) -> bool = default;

 private:
  template <std::size_t... Index>
  static auto default_paths(std::index_sequence<Index...>)
      -> std::array<AssetPath, sizeof...(Index)> {
    return {default_path(all_sound_roles[Index])...};
  }

  static auto default_path(SoundRole role) -> AssetPath;
  static auto index(SoundRole role) noexcept -> std::size_t;

  std::array<AssetPath, all_sound_roles.size()> paths_;
};

}
