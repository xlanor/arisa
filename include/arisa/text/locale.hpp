#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::text {

class Locale {
 public:
  static auto create(std::string_view tag) -> std::optional<Locale>;

  static auto undetermined() -> Locale;

  auto tag() const noexcept -> std::string_view { return tag_; }

  auto language() const noexcept -> std::string_view { return language_; }

  auto script() const noexcept -> std::string_view { return script_; }

  auto region() const noexcept -> std::string_view { return region_; }

  auto right_to_left() const noexcept -> bool;

  friend auto operator==(const Locale&, const Locale&) -> bool = default;
  friend auto operator<=>(const Locale&, const Locale&) = default;

 private:
  Locale(std::string language, std::string script, std::string region);

  static auto alphabetic(std::string_view subtag) noexcept -> bool;
  static auto alphanumeric(std::string_view subtag) noexcept -> bool;
  static auto lower(std::string_view subtag) -> std::string;
  static auto upper(std::string_view subtag) -> std::string;

  std::string tag_;
  std::string language_;
  std::string script_;
  std::string region_;
};

}
