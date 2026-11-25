#include <arisa/composition/accessibility_text.hpp>

#include <variant>

namespace arisa::composition {

auto AccessibilityText::resolve(const text::Locale& locale,
                                const text::StringCatalog& catalog) const
    -> std::string {
  if (const auto* plain = std::get_if<std::string>(&value_)) {
    return *plain;
  }
  if (const auto* key = std::get_if<text::LocalizedStringKey>(&value_)) {
    return catalog.resolve(*key, locale);
  }
  const auto& source = std::get<std::shared_ptr<const Source>>(value_);
  return source != nullptr ? source->resolve(locale, catalog) : std::string{};
}

auto AccessibilityText::verbatim() const -> std::string {
  return resolve(text::Locale::undetermined(), text::StringCatalog{});
}

auto AccessibilityText::speech() const -> std::optional<AccessibilitySpeech> {
  const auto* source = std::get_if<std::shared_ptr<const Source>>(&value_);
  if (source == nullptr || *source == nullptr) {
    return std::nullopt;
  }
  return (*source)->speech();
}

auto operator==(const AccessibilityText& left, const AccessibilityText& right)
    -> bool {
  const auto* mine =
      std::get_if<std::shared_ptr<const AccessibilityText::Source>>(
          &left.value_);
  const auto* theirs =
      std::get_if<std::shared_ptr<const AccessibilityText::Source>>(
          &right.value_);
  if (mine != nullptr && theirs != nullptr) {
    return *mine == *theirs ||
           (*mine != nullptr && *theirs != nullptr && (*mine)->same(**theirs));
  }
  return left.value_ == right.value_;
}

}
