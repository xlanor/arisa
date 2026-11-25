#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/plural_category.hpp>

#include <array>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class StringCatalog {
 public:
  static constexpr std::string_view default_table = "Localizable";

  class Entry {
   public:
    explicit Entry(std::string value) { forms_.back() = std::move(value); }

    explicit Entry(std::array<std::optional<std::string>, 6> forms)
        : forms_(std::move(forms)) {}

    auto form(PluralCategory category) const noexcept -> const std::string*;

    auto plural() const noexcept -> bool;

   private:
    std::array<std::optional<std::string>, 6> forms_{};
  };

  using Table = std::map<std::string, Entry, std::less<>>;

  struct Failure {
    std::string file;
    std::uint32_t line{0};
    std::string reason;
  };

  struct Loaded;

  StringCatalog() noexcept = default;

  [[nodiscard]] static auto parse(std::string_view source,
                                  std::string_view file)
      -> std::expected<std::map<std::string, Table, std::less<>>, Failure>;

  static auto load(const provider::AssetProvider& assets,
                   std::string_view directory) -> Loaded;

  auto with(const Locale& locale, std::string_view table, Table entries) const
      -> StringCatalog;

  auto merged(const StringCatalog& over) const -> StringCatalog;

  auto lookup(const LocalizedStringKey& key, const Locale& locale) const
      -> const Entry*;

  auto resolve(const LocalizedStringKey& key, const Locale& locale) const
      -> std::string;

  struct Segment {
    std::string_view literal{};
    std::optional<std::size_t> argument{};
  };

  auto pattern(const LocalizedStringKey& key, const Locale& locale) const
      -> std::string;

  auto pattern(const LocalizedStringKey& key, const Locale& locale,
               const LocalizedStringKey::Argument& count) const -> std::string;

  static auto segments(std::string_view pattern, std::size_t count)
      -> std::vector<Segment>;

  static auto argument_text(const LocalizedStringKey::Argument& argument,
                            const Locale& locale) -> std::string;

  auto locales() const -> std::vector<std::string>;

  auto empty() const noexcept -> bool;

  static auto interpolate(
      std::string_view pattern,
      const std::vector<LocalizedStringKey::Argument>& arguments,
      const Locale& locale) -> std::string;

  static auto fallbacks(const Locale& locale) -> std::vector<std::string>;

  friend auto operator==(const StringCatalog& left, const StringCatalog& right)
      -> bool {
    return left.tables_ == right.tables_;
  }

 private:
  using Tables =
      std::map<std::string, std::map<std::string, Table, std::less<>>,
               std::less<>>;

  explicit StringCatalog(std::shared_ptr<const Tables> tables) noexcept
      : tables_(std::move(tables)) {}

  static auto category(
      const std::vector<LocalizedStringKey::Argument>& arguments,
      const Locale& locale) -> PluralCategory;

  auto chosen(const LocalizedStringKey& key, const Locale& locale,
              const std::vector<LocalizedStringKey::Argument>& counted) const
      -> std::string;

  std::shared_ptr<const Tables> tables_;
};

struct StringCatalog::Loaded {
  StringCatalog catalog;
  std::vector<Failure> failures;
};

}
