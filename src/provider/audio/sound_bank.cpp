#include <arisa/provider/audio/sound_bank.hpp>

#include <format>
#include <string>

namespace arisa::provider {

SoundBank::SoundBank()
    : paths_(
          default_paths(std::make_index_sequence<all_sound_roles.size()>{})) {}

auto SoundBank::path(SoundRole role) const noexcept -> const AssetPath& {
  return paths_[index(role)];
}

auto SoundBank::with(SoundRole role, AssetPath path) const -> SoundBank {
  SoundBank updated = *this;
  updated.paths_[index(role)] = std::move(path);
  return updated;
}

auto SoundBank::load(const AssetProvider& assets,
                     DiagnosticsProvider& diagnostics,
                     std::string_view category) const -> Sounds {
  Sounds loaded;
  for (const SoundRole role : all_sound_roles) {
    const AssetPath& location = path(role);
    const auto blob = assets.read(location);
    if (!blob.has_value()) {
      diagnostics.report(Diagnostic{
          .severity = Severity::info,
          .category = std::string{category},
          .message =
              std::format("sound {} has no asset at {}; it plays silently",
                          role_name(role), location.value())});
      continue;
    }
    auto sound = PcmSound::parse_wav(blob->bytes());
    if (!sound.has_value()) {
      diagnostics.report(
          Diagnostic{.severity = Severity::warning,
                     .category = std::string{category},
                     .message = std::format(
                         "sound {} at {} is not a PCM16 WAV; it plays silently",
                         role_name(role), location.value())});
      continue;
    }
    loaded[index(role)].emplace(std::move(*sound));
  }
  return loaded;
}

auto SoundBank::role_name(SoundRole role) noexcept -> std::string_view {
  switch (role) {
    case SoundRole::focus_change:
      return "focus_change";
    case SoundRole::focus_error:
      return "focus_error";
    case SoundRole::click:
      return "click";
    case SoundRole::back:
      return "back";
    case SoundRole::error:
      return "error";
    case SoundRole::slider_tick:
      return "slider_tick";
    case SoundRole::slider_release:
      return "slider_release";
    case SoundRole::touch:
      return "touch";
    case SoundRole::sidebar:
      return "sidebar";
  }
  return "unknown";
}

auto SoundBank::default_path(SoundRole role) -> AssetPath {
  return *AssetPath::create(
      std::format("arisa/sounds/{}.wav", role_name(role)));
}

auto SoundBank::index(SoundRole role) noexcept -> std::size_t {
  return std::to_underlying(role);
}

}
