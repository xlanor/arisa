#include <arisa/text/font_collection.hpp>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace arisa::text {

class FontCollection::ChainCache {
 public:
  auto find(FontRole cjk, const FaceQuery& query) const
      -> std::shared_ptr<const Chain> {
    const std::scoped_lock lock{mutex_};
    const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) {
      return entry.cjk == cjk && entry.query == query;
    });
    return found == entries_.end() ? nullptr : found->chain;
  }

  void store(FontRole cjk, const FaceQuery& query,
             std::shared_ptr<const Chain> chain) {
    const std::scoped_lock lock{mutex_};
    entries_.push_back(
        Entry{.cjk = cjk, .query = query, .chain = std::move(chain)});
  }

  auto derived(const std::shared_ptr<const FontFace>& base, float stretch,
               float slant) -> std::shared_ptr<const FontFace> {
    const std::scoped_lock lock{mutex_};
    const auto found = std::ranges::find_if(derived_, [&](const Derived& kept) {
      return kept.base == base->id() && kept.stretch == stretch &&
             kept.slant == slant;
    });
    if (found != derived_.end()) {
      return found->face;
    }
    FaceOptions options = base->options();
    options.stretch = stretch;
    options.slant = slant;
    auto made = base->instance(options);
    if (!made.has_value()) {
      return base;
    }
    auto shared = std::make_shared<const FontFace>(std::move(*made));
    derived_.push_back(Derived{.base = base->id(),
                               .stretch = stretch,
                               .slant = slant,
                               .face = shared});
    return shared;
  }

 private:
  struct Entry {
    FontRole cjk;
    FaceQuery query;
    std::shared_ptr<const Chain> chain;
  };

  struct Derived {
    FaceId base;
    float stretch;
    float slant;
    std::shared_ptr<const FontFace> face;
  };

  mutable std::mutex mutex_;
  std::vector<Entry> entries_;
  std::vector<Derived> derived_;
};

FontCollection::FontCollection(std::vector<FontEntry> entries)
    : entries_(std::move(entries)), chains_(std::make_unique<ChainCache>()) {}

FontCollection::FontCollection(FontCollection&& other) noexcept = default;

auto FontCollection::operator=(FontCollection&& other) noexcept
    -> FontCollection& = default;

FontCollection::~FontCollection() = default;

auto FontCollection::create(std::vector<FontEntry> entries)
    -> std::expected<FontCollection, TextError> {
  std::erase_if(entries,
                [](const FontEntry& entry) { return entry.face == nullptr; });
  if (entries.empty()) {
    return std::unexpected(TextError{TextErrorCode::missing_font});
  }
  return FontCollection{std::move(entries)};
}

auto FontCollection::bundled_path() -> provider::AssetPath {
  return *provider::AssetPath::create("arisa/fonts/InterVariable.ttf");
}

auto FontCollection::symbols_path() -> provider::AssetPath {
  return *provider::AssetPath::create("arisa/fonts/MaterialSymbolsRounded.ttf");
}

auto FontCollection::filled_symbols_path() -> provider::AssetPath {
  return *provider::AssetPath::create(
      "arisa/fonts/MaterialSymbolsRoundedFilled.ttf");
}

auto FontCollection::family_path(std::string_view name) -> provider::AssetPath {
  std::string path{"arisa/fonts/"};
  path.append(name);
  path.append(".ttf");
  return *provider::AssetPath::create(path);
}

auto FontCollection::bundled_families(const provider::AssetProvider& assets)
    -> std::vector<FontEntry> {
  std::vector<FontEntry> entries;
  for (const Family& family : families) {
    const provider::AssetPath path = family_path(family.name);
    if (!assets.exists(path)) {
      continue;
    }
    auto blob = assets.read(path);
    if (!blob.has_value()) {
      continue;
    }
    auto face = FontFace::create(
        std::make_shared<const provider::AssetBlob>(std::move(*blob)));
    if (!face.has_value()) {
      continue;
    }
    entries.push_back(
        FontEntry{.role = FontRole::latin,
                  .face = std::make_shared<const FontFace>(std::move(*face)),
                  .design = FontDesign::text,
                  .typeface = family.typeface,
                  .italic = family.italic});
  }
  return entries;
}

auto FontCollection::bundled(const provider::AssetProvider& assets)
    -> std::expected<std::vector<FontEntry>, TextError> {
  auto blob = assets.read(bundled_path());
  if (!blob.has_value()) {
    return std::unexpected(TextError::from(blob.error()));
  }
  auto shared = std::make_shared<const provider::AssetBlob>(std::move(*blob));
  constexpr std::array designs{
      std::pair{FontDesign::text, 14.0F},
      std::pair{FontDesign::display, 32.0F},
  };
  std::vector<FontEntry> entries;
  for (const auto& [design, optical_size] : designs) {
    for (const FontWeight weight : bundled_weights) {
      auto face =
          FontFace::create(shared, FaceOptions{.index = 0,
                                               .weight = weight,
                                               .optical_size = optical_size});
      if (!face.has_value()) {
        return std::unexpected(face.error());
      }
      entries.push_back(
          FontEntry{.role = FontRole::latin,
                    .face = std::make_shared<const FontFace>(std::move(*face)),
                    .design = design});
    }
  }
  std::ranges::move(bundled_families(assets), std::back_inserter(entries));
  for (const provider::AssetPath& path :
       {symbols_path(), filled_symbols_path()}) {
    if (!assets.exists(path)) {
      continue;
    }
    auto symbols = assets.read(path);
    if (!symbols.has_value()) {
      continue;
    }
    auto face = FontFace::create(
        std::make_shared<const provider::AssetBlob>(std::move(*symbols)));
    if (face.has_value()) {
      entries.push_back(
          FontEntry{.role = FontRole::symbols,
                    .face = std::make_shared<const FontFace>(std::move(*face)),
                    .design = FontDesign::text});
    }
  }
  return entries;
}

auto FontCollection::load(const provider::AssetProvider& assets,
                          const provider::FontProvider& fonts,
                          std::vector<FontEntry> app_fonts)
    -> std::expected<FontCollection, TextError> {
  auto entries = bundled(assets);
  if (!entries.has_value()) {
    return std::unexpected(entries.error());
  }
  for (const provider::SystemFont system : provider::all_system_fonts) {
    if (!fonts.available(system)) {
      continue;
    }
    auto data = fonts.system_font(system);
    if (!data.has_value()) {
      continue;
    }
    auto face = FontFace::create(*data);
    if (!face.has_value()) {
      continue;
    }
    entries->push_back(
        FontEntry{.role = role(system),
                  .face = std::make_shared<const FontFace>(std::move(*face)),
                  .design = FontDesign::text});
  }
  std::ranges::move(app_fonts, std::back_inserter(*entries));
  return create(std::move(*entries));
}

auto FontCollection::role(provider::SystemFont font) noexcept -> FontRole {
  switch (font) {
    case provider::SystemFont::standard:
      return FontRole::japanese;
    case provider::SystemFont::chinese_simplified:
    case provider::SystemFont::chinese_simplified_extended:
      return FontRole::chinese_simplified;
    case provider::SystemFont::chinese_traditional:
      return FontRole::chinese_traditional;
    case provider::SystemFont::korean:
      return FontRole::korean;
    case provider::SystemFont::nintendo_symbols:
      return FontRole::symbols;
  }
  return FontRole::symbols;
}

auto FontCollection::preferred_cjk(const Locale& locale) noexcept
    -> std::optional<FontRole> {
  const std::string_view language = locale.language();
  if (language == "ja") {
    return FontRole::japanese;
  }
  if (language == "ko") {
    return FontRole::korean;
  }
  if (language == "zh" || language == "yue") {
    const std::string_view script = locale.script();
    const std::string_view region = locale.region();
    if (script == "Hant" ||
        (script.empty() &&
         (region == "TW" || region == "HK" || region == "MO"))) {
      return FontRole::chinese_traditional;
    }
    return FontRole::chinese_simplified;
  }
  return std::nullopt;
}

auto FontCollection::role_order(FontRole cjk) noexcept
    -> std::array<FontRole, role_count> {
  switch (cjk) {
    case FontRole::chinese_simplified:
      return {FontRole::latin,
              FontRole::script,
              FontRole::chinese_simplified,
              FontRole::chinese_traditional,
              FontRole::japanese,
              FontRole::korean,
              FontRole::symbols,
              FontRole::emoji};
    case FontRole::chinese_traditional:
      return {FontRole::latin,
              FontRole::script,
              FontRole::chinese_traditional,
              FontRole::chinese_simplified,
              FontRole::japanese,
              FontRole::korean,
              FontRole::symbols,
              FontRole::emoji};
    case FontRole::korean:
      return {FontRole::latin,
              FontRole::script,
              FontRole::korean,
              FontRole::japanese,
              FontRole::chinese_simplified,
              FontRole::chinese_traditional,
              FontRole::symbols,
              FontRole::emoji};
    case FontRole::latin:
    case FontRole::script:
    case FontRole::japanese:
    case FontRole::symbols:
    case FontRole::emoji:
      break;
  }
  return {FontRole::latin,
          FontRole::script,
          FontRole::japanese,
          FontRole::chinese_simplified,
          FontRole::chinese_traditional,
          FontRole::korean,
          FontRole::symbols,
          FontRole::emoji};
}

auto FontCollection::chain(const Locale& locale, FontWeight weight,
                           FontDesign design) const
    -> std::shared_ptr<const Chain> {
  return chain(locale, FaceQuery{.weight = weight, .design = design});
}

auto FontCollection::chain(const Locale& locale, const FaceQuery& query) const
    -> std::shared_ptr<const Chain> {
  const FontRole cjk = preferred_cjk(locale).value_or(FontRole::japanese);
  if (auto found = chains_->find(cjk, query)) {
    return found;
  }
  auto built = std::make_shared<const Chain>(build(cjk, query));
  chains_->store(cjk, query, built);
  return built;
}

auto FontCollection::admits(const FontEntry& entry,
                            const FaceQuery& query) const noexcept -> bool {
  if (!entry.name.empty()) {
    return false;
  }
  if (entry.typeface != Typeface::standard &&
      entry.typeface != query.typeface) {
    return false;
  }
  return !entry.italic || query.italic;
}

auto FontCollection::adapted(const FontEntry& entry, const FaceQuery& query,
                             bool slanted) const
    -> std::shared_ptr<const FontFace> {
  const bool symbolic =
      entry.role == FontRole::symbols || entry.role == FontRole::emoji;
  const bool alphabetic =
      entry.role == FontRole::latin || entry.role == FontRole::script;
  const float slant = slanted && !symbolic ? synthetic_slant : 0.0F;
  const float stretch = alphabetic ? query.stretch : 1.0F;
  if (slant == 0.0F && stretch == 1.0F) {
    return entry.face;
  }
  return chains_->derived(entry.face, stretch, slant);
}

auto FontCollection::build(FontRole cjk, const FaceQuery& query) const
    -> Chain {
  Chain faces;
  faces.reserve(entries_.size());
  if (query.name.has_value()) {
    for (const FontEntry& entry : entries_) {
      if (!entry.name.empty() && FontName{entry.name} == *query.name) {
        faces.push_back(adapted(entry, query, query.italic && !entry.italic));
      }
    }
  }
  for (const FontRole wanted : role_order(cjk)) {
    std::vector<const FontEntry*> matching;
    for (const FontEntry& entry : entries_) {
      if (entry.role == wanted && admits(entry, query)) {
        matching.push_back(&entry);
      }
    }
    const int requested = query.weight.value();
    const auto distance = [requested, &query](const FontEntry* entry) {
      return std::tuple{entry->typeface == query.typeface ? 0 : 1,
                        entry->italic == query.italic ? 0 : 1,
                        entry->design == query.design ? 0 : 1,
                        std::abs(entry->face->weight().value() - requested)};
    };
    std::ranges::stable_sort(matching, {}, distance);
    std::vector<const FontEntry*> kept;
    for (const FontEntry* entry : matching) {
      const bool duplicate =
          std::ranges::any_of(kept, [entry](const FontEntry* other) {
            return other->face->shares_data_with(*entry->face) ||
                   ((entry->typeface != Typeface::standard || entry->italic) &&
                    other->typeface == entry->typeface &&
                    other->italic == entry->italic &&
                    other->face->family() == entry->face->family());
          });
      if (!duplicate) {
        kept.push_back(entry);
      }
    }
    bool italic_seen = false;
    for (const FontEntry* entry : kept) {
      faces.push_back(adapted(*entry, query,
                              query.italic && !entry->italic && !italic_seen));
      italic_seen = italic_seen || entry->italic;
    }
  }
  return faces;
}

}
