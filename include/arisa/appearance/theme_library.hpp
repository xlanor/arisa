#pragma once

#include <arisa/appearance/theme_error.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/storage/provider.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::appearance {

class ThemeLibrary {
 public:
  using StorageRef =
      std::optional<std::reference_wrapper<provider::StorageProvider>>;

  static constexpr std::string_view default_name = "default";
  static constexpr std::string_view pack_directory = "arisa/themes";
  static constexpr std::string_view user_directory = "themes";
  static constexpr std::string_view extension = ".toml";
  static constexpr std::size_t extends_limit = 8;

  enum class Fallback : std::uint8_t {
    bundled,
    none,
  };

  enum class Origin : std::uint8_t {
    user,
    folder,
    pack,
  };

  struct Location {
    Origin origin{Origin::pack};
    std::string path;

    friend auto operator==(const Location&, const Location&) -> bool = default;
  };

  ThemeLibrary(const provider::AssetProvider& assets, StorageRef storage,
               std::vector<std::string> packs,
               StorageRef folder = std::nullopt);

  auto names() const -> std::vector<std::string>;

  [[nodiscard]] auto load(std::string_view name,
                          Fallback strategy = Fallback::bundled) const
      -> std::expected<theme::Theme, ThemeError>;

  auto base_of(std::string_view name) const -> std::optional<std::string>;

  auto locations(std::string_view name) const -> std::vector<Location>;

  auto stamp(const Location& location) const
      -> std::optional<provider::FileStamp>;

  auto destination() const noexcept -> StorageRef;

  auto resolve(std::string_view name,
               provider::DiagnosticsProvider& diagnostics) const
      -> theme::Theme;

 private:
  struct Source {
    std::string text;
    std::string file;
    bool user{false};
  };

  static auto fingerprint(std::span<const std::byte> bytes) noexcept
      -> provider::FileStamp;

  [[nodiscard]] auto load(std::string_view name,
                          std::vector<std::string>& chain,
                          Fallback strategy) const
      -> std::expected<theme::Theme, ThemeError>;

  auto source(std::string_view name) const -> std::optional<Source>;

  auto resolve(std::string_view name,
               provider::DiagnosticsProvider& diagnostics,
               std::size_t depth) const -> theme::Theme;

  static auto text_of(std::span<const std::byte> bytes) -> std::string;

  std::reference_wrapper<const provider::AssetProvider> assets_;
  StorageRef storage_;
  std::vector<std::string> packs_;
  StorageRef folder_;

  auto source(std::string_view name, bool users) const -> std::optional<Source>;
};

}
