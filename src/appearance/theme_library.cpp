#include <arisa/appearance/theme_file.hpp>
#include <arisa/appearance/theme_library.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::appearance {

ThemeLibrary::ThemeLibrary(const provider::AssetProvider& assets,
                           StorageRef storage, std::vector<std::string> packs,
                           StorageRef folder)
    : assets_(assets),
      storage_(storage),
      packs_(std::move(packs)),
      folder_(folder) {}

auto ThemeLibrary::text_of(std::span<const std::byte> bytes) -> std::string {
  std::string text(bytes.size(), '\0');
  std::ranges::transform(bytes, text.begin(), [](std::byte value) {
    return static_cast<char>(value);
  });
  return text;
}

auto ThemeLibrary::names() const -> std::vector<std::string> {
  std::vector<std::string> found{std::string{default_name},
                                 std::string{theme::Theme::high_contrast_name}};
  const auto add = [&found](std::string name) {
    if (!std::ranges::contains(found, name)) {
      found.push_back(std::move(name));
    }
  };
  for (const std::string& pack : packs_) {
    add(pack);
  }
  for (const StorageRef& place : {storage_, folder_}) {
    if (!place.has_value()) {
      continue;
    }
    if (const auto directory = provider::AssetPath::create(user_directory)) {
      if (auto listed = place->get().list(*directory)) {
        for (const std::string& file : *listed) {
          if (file.ends_with(extension)) {
            add(file.substr(0, file.size() - extension.size()));
          }
        }
      }
    }
  }
  return found;
}

auto ThemeLibrary::source(std::string_view name) const
    -> std::optional<Source> {
  return source(name, true);
}

auto ThemeLibrary::source(std::string_view name, bool users) const
    -> std::optional<Source> {
  const std::string file = std::format("{}{}", name, extension);
  if (users) {
    for (const StorageRef& place : {storage_, folder_}) {
      if (!place.has_value()) {
        continue;
      }
      if (const auto path = provider::AssetPath::create(
              std::format("{}/{}", user_directory, file))) {
        if (auto blob = place->get().read(*path)) {
          return Source{.text = text_of(blob->bytes()),
                        .file = std::format("{}/{}", user_directory, file),
                        .user = true};
        }
      }
    }
  }
  if (const auto path = provider::AssetPath::create(
          std::format("{}/{}", pack_directory, file))) {
    if (auto blob = assets_.get().read(*path)) {
      return Source{.text = text_of(blob->bytes()),
                    .file = std::format("{}/{}", pack_directory, file),
                    .user = false};
    }
  }
  return std::nullopt;
}

auto ThemeLibrary::load(std::string_view name, Fallback strategy) const
    -> std::expected<theme::Theme, ThemeError> {
  std::vector<std::string> chain;
  return load(name, chain, strategy);
}

auto ThemeLibrary::load(std::string_view name, std::vector<std::string>& chain,
                        Fallback strategy) const
    -> std::expected<theme::Theme, ThemeError> {
  if (name == default_name) {
    return theme::Theme::defaults();
  }
  if (name == theme::Theme::high_contrast_name && !source(name).has_value()) {
    return theme::Theme::high_contrast();
  }
  if (std::ranges::contains(chain, name) || chain.size() >= extends_limit) {
    return std::unexpected(ThemeError{ThemeErrorCode::cyclic_extends,
                                      std::string{name}, 1U, "extends",
                                      "themes extend each other in a cycle"});
  }
  const std::optional<Source> found = source(name);
  if (!found.has_value()) {
    return std::unexpected(ThemeError{ThemeErrorCode::not_found,
                                      std::string{name},
                                      0U,
                                      {},
                                      "no theme with this name"});
  }
  chain.emplace_back(name);
  const auto bases = [this, &chain, strategy](std::string_view base) {
    return load(base, chain, strategy);
  };
  auto parsed = ThemeFile::parse(found->text, found->file, bases);
  if (!parsed.has_value() && found->user && strategy == Fallback::bundled) {
    if (const std::optional<Source> bundled = source(name, false)) {
      if (auto fallback = ThemeFile::parse(bundled->text, bundled->file, bases);
          fallback.has_value()) {
        parsed = std::move(fallback);
      }
    }
  }
  chain.pop_back();
  if (!parsed.has_value()) {
    return parsed;
  }
  if (parsed->name() == default_name) {
    return parsed->named(std::string{name});
  }
  return parsed;
}

auto ThemeLibrary::resolve(std::string_view name,
                           provider::DiagnosticsProvider& diagnostics) const
    -> theme::Theme {
  return resolve(name, diagnostics, 0);
}

auto ThemeLibrary::resolve(std::string_view name,
                           provider::DiagnosticsProvider& diagnostics,
                           std::size_t depth) const -> theme::Theme {
  auto loaded = load(name);
  if (loaded.has_value()) {
    return std::move(*loaded);
  }
  diagnostics.report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "theme",
      .message = std::format("theme '{}' failed to load ({}); using its base",
                             name, loaded.error().description())});
  std::optional<std::string> base;
  if (const std::optional<Source> found = source(name)) {
    base = ThemeFile::base_of(found->text);
  }
  if (!base.has_value() || *base == name || depth >= extends_limit) {
    return theme::Theme::defaults();
  }
  return resolve(*base, diagnostics, depth + 1);
}

auto ThemeLibrary::base_of(std::string_view name) const
    -> std::optional<std::string> {
  if (const std::optional<Source> found = source(name)) {
    return ThemeFile::base_of(found->text);
  }
  return std::nullopt;
}

auto ThemeLibrary::locations(std::string_view name) const
    -> std::vector<Location> {
  std::vector<Location> found;
  std::vector<std::string> visited;
  std::optional<std::string> at{std::string{name}};
  while (at.has_value() && *at != default_name &&
         !std::ranges::contains(visited, *at) &&
         visited.size() < extends_limit) {
    const std::string file = std::format("{}{}", *at, extension);
    if (storage_.has_value()) {
      found.push_back(
          Location{.origin = Origin::user,
                   .path = std::format("{}/{}", user_directory, file)});
    }
    if (folder_.has_value()) {
      found.push_back(
          Location{.origin = Origin::folder,
                   .path = std::format("{}/{}", user_directory, file)});
    }
    found.push_back(
        Location{.origin = Origin::pack,
                 .path = std::format("{}/{}", pack_directory, file)});
    visited.push_back(*at);
    at = base_of(*at);
  }
  return found;
}

auto ThemeLibrary::fingerprint(std::span<const std::byte> bytes) noexcept
    -> provider::FileStamp {
  constexpr std::uint64_t basis = 14'695'981'039'346'656'037ULL;
  constexpr std::uint64_t prime = 1'099'511'628'211ULL;
  std::uint64_t hash = basis;
  for (const std::byte value : bytes) {
    hash = (hash ^ std::to_integer<std::uint64_t>(value)) * prime;
  }
  return provider::FileStamp{hash};
}

auto ThemeLibrary::stamp(const Location& location) const
    -> std::optional<provider::FileStamp> {
  const auto path = provider::AssetPath::create(location.path);
  if (!path.has_value()) {
    return std::nullopt;
  }
  if (location.origin == Origin::pack) {
    if (auto stamped = assets_.get().stamp(*path)) {
      return stamped;
    }
    if (auto blob = assets_.get().read(*path)) {
      return fingerprint(blob->bytes());
    }
    return std::nullopt;
  }
  const StorageRef& place =
      location.origin == Origin::user ? storage_ : folder_;
  if (!place.has_value()) {
    return std::nullopt;
  }
  if (auto stamped = place->get().stamp(*path)) {
    return stamped;
  }
  if (auto blob = place->get().read(*path)) {
    return fingerprint(blob->bytes());
  }
  return std::nullopt;
}

auto ThemeLibrary::destination() const noexcept -> StorageRef {
  return folder_.has_value() ? folder_ : storage_;
}

}
