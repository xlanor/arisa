#pragma once

#include <arisa/text/locale.hpp>

#include <optional>
#include <utility>

namespace arisa::text {

class TypesettingLanguage {
 public:
  static const TypesettingLanguage automatic;

  static auto explicit_(Locale language) -> TypesettingLanguage {
    TypesettingLanguage chosen;
    chosen.language_ = std::move(language);
    return chosen;
  }

  auto language() const noexcept -> const std::optional<Locale>& {
    return language_;
  }

  friend auto operator==(const TypesettingLanguage&, const TypesettingLanguage&)
      -> bool = default;

 private:
  TypesettingLanguage() = default;

  std::optional<Locale> language_;
};

inline const TypesettingLanguage TypesettingLanguage::automatic{};

}
