#include <arisa/text/locale_data.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/plural_rules.hpp>
#include <arisa/text/string_catalog.hpp>

#include <toml.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::text {

auto StringCatalog::Entry::form(PluralCategory category) const noexcept
    -> const std::string* {
  const auto& chosen = forms_[std::to_underlying(category)];
  if (chosen.has_value()) {
    return &*chosen;
  }
  const auto& other = forms_.back();
  return other.has_value() ? &*other : nullptr;
}

auto StringCatalog::Entry::plural() const noexcept -> bool {
  for (std::size_t index = 0; index + 1 < forms_.size(); ++index) {
    if (forms_[index].has_value()) {
      return true;
    }
  }
  return false;
}

auto StringCatalog::parse(std::string_view source, std::string_view file)
    -> std::expected<std::map<std::string, Table, std::less<>>, Failure> {
  toml::parse_result parsed = toml::parse(source, file);
  if (!parsed) {
    const toml::parse_error& error = parsed.error();
    return std::unexpected(
        Failure{.file = std::string{file},
                .line = static_cast<std::uint32_t>(error.source().begin.line),
                .reason = std::string{error.description()}});
  }
  std::map<std::string, Table, std::less<>> tables;
  const auto entry =
      [&file](const toml::node& node) -> std::expected<Entry, Failure> {
    if (const auto value = node.value<std::string>()) {
      return Entry{*value};
    }
    const toml::table* forms = node.as_table();
    if (forms == nullptr) {
      return std::unexpected(
          Failure{.file = std::string{file},
                  .line = static_cast<std::uint32_t>(node.source().begin.line),
                  .reason = "expected a string or a table of plural forms"});
    }
    std::array<std::optional<std::string>, 6> chosen{};
    for (const auto& [name, form] : *forms) {
      const auto category = PluralRules::parse(name.str());
      const auto value = form.value<std::string>();
      if (!category.has_value() || !value.has_value()) {
        return std::unexpected(Failure{
            .file = std::string{file},
            .line = static_cast<std::uint32_t>(form.source().begin.line),
            .reason = std::format("{} is not a plural form with a string",
                                  name.str())});
      }
      chosen[std::to_underlying(*category)] = *value;
    }
    if (!chosen.back().has_value()) {
      return std::unexpected(
          Failure{.file = std::string{file},
                  .line = static_cast<std::uint32_t>(node.source().begin.line),
                  .reason = "plural forms need an other form"});
    }
    return Entry{std::move(chosen)};
  };
  Table& base = tables[std::string{default_table}];
  for (const auto& [key, node] : parsed.table()) {
    const toml::table* nested = node.as_table();
    if (nested != nullptr && !nested->empty() &&
        !PluralRules::parse(nested->cbegin()->first.str()).has_value()) {
      Table& named = tables[std::string{key.str()}];
      for (const auto& [inner, value] : *nested) {
        auto made = entry(value);
        if (!made.has_value()) {
          return std::unexpected(std::move(made.error()));
        }
        named.insert_or_assign(std::string{inner.str()}, std::move(*made));
      }
      continue;
    }
    auto made = entry(node);
    if (!made.has_value()) {
      return std::unexpected(std::move(made.error()));
    }
    base.insert_or_assign(std::string{key.str()}, std::move(*made));
  }
  return tables;
}

auto StringCatalog::load(const provider::AssetProvider& assets,
                         std::string_view directory) -> Loaded {
  Loaded loaded{.catalog = StringCatalog{}, .failures = {}};
  auto tables = std::make_shared<Tables>();
  for (std::size_t index = 0; index < LocaleData::bundled().size(); ++index) {
    const std::string identifier{LocaleData::at(index).identifier()};
    const auto path = provider::AssetPath::create(
        std::format("{}/{}.toml", directory, identifier));
    if (!path.has_value() || !assets.exists(*path)) {
      continue;
    }
    const auto blob = assets.read(*path);
    if (!blob.has_value()) {
      loaded.failures.push_back(
          Failure{.file = std::string{path->value()},
                  .line = 0,
                  .reason = std::string{blob.error().explanation()}});
      continue;
    }
    std::string source(blob->bytes().size(), '\0');
    std::ranges::transform(blob->bytes(), source.begin(), [](std::byte value) {
      return static_cast<char>(value);
    });
    auto parsed = parse(source, path->value());
    if (!parsed.has_value()) {
      loaded.failures.push_back(std::move(parsed.error()));
      continue;
    }
    (*tables)[identifier] = std::move(*parsed);
  }
  loaded.catalog = StringCatalog{std::move(tables)};
  return loaded;
}

auto StringCatalog::with(const Locale& locale, std::string_view table,
                         Table entries) const -> StringCatalog {
  auto tables =
      tables_ ? std::make_shared<Tables>(*tables_) : std::make_shared<Tables>();
  Table& target = (*tables)[std::string{LocaleData::of(locale).identifier()}]
                           [std::string{table}];
  for (auto& [key, entry] : entries) {
    target.insert_or_assign(key, std::move(entry));
  }
  return StringCatalog{std::move(tables)};
}

auto StringCatalog::merged(const StringCatalog& over) const -> StringCatalog {
  if (over.empty()) {
    return *this;
  }
  if (empty()) {
    return over;
  }
  auto tables = std::make_shared<Tables>(*tables_);
  for (const auto& [locale, named] : *over.tables_) {
    for (const auto& [table, entries] : named) {
      Table& target = (*tables)[locale][table];
      for (const auto& [key, entry] : entries) {
        target.insert_or_assign(key, entry);
      }
    }
  }
  return StringCatalog{std::move(tables)};
}

auto StringCatalog::fallbacks(const Locale& locale)
    -> std::vector<std::string> {
  std::vector<std::string> chain;
  chain.emplace_back(LocaleData::of(locale).identifier());
  const std::string language{locale.language()};
  if (language != chain.front() && language != "und") {
    const Locale plain =
        Locale::create(language).value_or(Locale::undetermined());
    chain.emplace_back(LocaleData::of(plain).identifier());
  }
  return chain;
}

auto StringCatalog::lookup(const LocalizedStringKey& key,
                           const Locale& locale) const -> const Entry* {
  if (empty()) {
    return nullptr;
  }
  const std::string_view table =
      key.table().has_value() ? std::string_view{*key.table()} : default_table;
  for (const std::string& identifier : fallbacks(locale)) {
    const auto named = tables_->find(identifier);
    if (named == tables_->end()) {
      continue;
    }
    const auto entries = named->second.find(table);
    if (entries == named->second.end()) {
      continue;
    }
    const auto found = entries->second.find(key.key());
    if (found != entries->second.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

auto StringCatalog::category(
    const std::vector<LocalizedStringKey::Argument>& arguments,
    const Locale& locale) -> PluralCategory {
  const PluralRules rules{locale};
  for (const LocalizedStringKey::Argument& argument : arguments) {
    if (const auto* whole = std::get_if<std::int64_t>(&argument)) {
      return rules.category(*whole);
    }
    if (const auto* real = std::get_if<double>(&argument)) {
      const std::string shown =
          NumberFormatStyle::number().grouping(false).format(*real, locale);
      const std::size_t point =
          shown.find(LocaleData::of(locale).decimal_separator());
      const unsigned digits =
          point == std::string::npos
              ? 0U
              : static_cast<unsigned>(
                    shown.size() - point -
                    LocaleData::of(locale).decimal_separator().size());
      return rules.category(*real, digits);
    }
  }
  return PluralCategory::other;
}

auto StringCatalog::segments(std::string_view pattern, std::size_t count)
    -> std::vector<Segment> {
  std::vector<Segment> found;
  std::size_t at = 0;
  std::size_t literal = 0;
  std::size_t next = 0;
  while (at < pattern.size()) {
    if (pattern[at] == '{') {
      const std::size_t close = pattern.find('}', at);
      if (close != std::string_view::npos) {
        const std::string_view inside = pattern.substr(at + 1, close - at - 1);
        bool numbered = !inside.empty();
        std::size_t parsed = 0;
        for (const char digit : inside) {
          if (digit < '0' || digit > '9') {
            numbered = false;
            break;
          }
          parsed = (parsed * 10) + static_cast<std::size_t>(digit - '0');
        }
        if (inside.empty() || numbered) {
          const std::size_t index = numbered ? parsed : next;
          if (index < count) {
            if (at > literal) {
              found.push_back(
                  Segment{.literal = pattern.substr(literal, at - literal)});
            }
            found.push_back(Segment{.literal = {}, .argument = index});
            next = index + 1;
            at = close + 1;
            literal = at;
            continue;
          }
        }
      }
    }
    ++at;
  }
  if (literal < pattern.size()) {
    found.push_back(Segment{.literal = pattern.substr(literal)});
  }
  return found;
}

auto StringCatalog::argument_text(const LocalizedStringKey::Argument& argument,
                                  const Locale& locale) -> std::string {
  if (const auto* whole = std::get_if<std::int64_t>(&argument)) {
    return NumberFormatStyle::number().format(*whole, locale);
  }
  if (const auto* real = std::get_if<double>(&argument)) {
    return NumberFormatStyle::number().format(*real, locale);
  }
  if (const auto* written = std::get_if<std::string>(&argument)) {
    return *written;
  }
  const auto& embedded = std::get<LocalizedStringKey::Interpolated>(argument);
  return embedded != nullptr ? embedded->text(locale) : std::string{};
}

auto StringCatalog::interpolate(
    std::string_view pattern,
    const std::vector<LocalizedStringKey::Argument>& arguments,
    const Locale& locale) -> std::string {
  std::string result;
  result.reserve(pattern.size());
  for (const Segment& segment : segments(pattern, arguments.size())) {
    if (segment.argument.has_value()) {
      result += argument_text(arguments[*segment.argument], locale);
    } else {
      result += segment.literal;
    }
  }
  return result;
}

auto StringCatalog::chosen(
    const LocalizedStringKey& key, const Locale& locale,
    const std::vector<LocalizedStringKey::Argument>& counted) const
    -> std::string {
  const Entry* entry = lookup(key, locale);
  if (entry == nullptr) {
    return std::string{key.key()};
  }
  const std::string* form = entry->form(
      entry->plural() ? category(counted, locale) : PluralCategory::other);
  return form != nullptr ? *form : std::string{key.key()};
}

auto StringCatalog::pattern(const LocalizedStringKey& key,
                            const Locale& locale) const -> std::string {
  return chosen(key, locale, key.arguments());
}

auto StringCatalog::pattern(const LocalizedStringKey& key, const Locale& locale,
                            const LocalizedStringKey::Argument& count) const
    -> std::string {
  return chosen(key, locale, {count});
}

auto StringCatalog::resolve(const LocalizedStringKey& key,
                            const Locale& locale) const -> std::string {
  return interpolate(pattern(key, locale), key.arguments(), locale);
}

auto StringCatalog::locales() const -> std::vector<std::string> {
  if (empty()) {
    return {};
  }
  return std::ranges::to<std::vector<std::string>>(std::views::keys(*tables_));
}

auto StringCatalog::empty() const noexcept -> bool {
  return !tables_ || tables_->empty();
}

}
