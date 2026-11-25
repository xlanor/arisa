#pragma once

#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/string_catalog.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace arisa::composition {

class AccessibilityText {
 public:
  class Source {
   public:
    Source() = default;
    Source(const Source&) = delete;
    auto operator=(const Source&) -> Source& = delete;
    Source(Source&&) = delete;
    auto operator=(Source&&) -> Source& = delete;
    virtual ~Source() = default;

    virtual auto resolve(const text::Locale& locale,
                         const text::StringCatalog& catalog) const
        -> std::string = 0;

    virtual auto speech() const -> std::optional<AccessibilitySpeech> = 0;

    virtual auto tag() const noexcept -> const void* = 0;

    virtual auto same(const Source& other) const -> bool = 0;
  };

  AccessibilityText(std::string text) : value_(std::move(text)) {}

  AccessibilityText(std::string_view text) : value_(std::string{text}) {}

  AccessibilityText(const char* text) : value_(std::string{text}) {}

  AccessibilityText(text::LocalizedStringKey key) : value_(std::move(key)) {}

  explicit AccessibilityText(std::shared_ptr<const Source> source)
      : value_(std::move(source)) {}

  auto resolve(const text::Locale& locale,
               const text::StringCatalog& catalog) const -> std::string;

  auto verbatim() const -> std::string;

  auto speech() const -> std::optional<AccessibilitySpeech>;

  auto localized() const noexcept -> bool {
    return !std::holds_alternative<std::string>(value_);
  }

  friend auto operator==(const AccessibilityText& left,
                         const AccessibilityText& right) -> bool;

 private:
  std::variant<std::string, text::LocalizedStringKey,
               std::shared_ptr<const Source>>
      value_;
};

}
