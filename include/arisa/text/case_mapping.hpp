#pragma once

#include <arisa/text/locale.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

class CaseMapping {
 public:
  static auto uppercased(std::string_view text) -> std::string;

  static auto lowercased(std::string_view text) -> std::string;

  static auto uppercased(std::string_view text, const Locale& locale)
      -> std::string;

  static auto lowercased(std::string_view text, const Locale& locale)
      -> std::string;

 private:
  enum class Rules : std::uint8_t {
    standard,
    turkic,
    greek,
    lithuanian,
  };

  static auto rules(const Locale& locale) noexcept -> Rules;

  static auto upper(char32_t codepoint) noexcept -> char32_t;

  static auto lower(char32_t codepoint) noexcept -> char32_t;

  static auto paired(char32_t codepoint) noexcept -> bool;

  static auto letter(char32_t codepoint) noexcept -> bool;

  static auto combining_above(char32_t codepoint) noexcept -> bool;

  static auto combining(char32_t codepoint) noexcept -> bool;

  static auto more_above(const std::vector<char32_t>& codepoints,
                         std::size_t index) noexcept -> bool;

  static auto after_soft_dotted(const std::vector<char32_t>& codepoints,
                                std::size_t index) noexcept -> bool;

  static auto unaccented_greek(char32_t codepoint) noexcept -> char32_t;

  static auto decoded(std::string_view text) -> std::vector<char32_t>;

  static auto final_sigma(const std::vector<char32_t>& codepoints,
                          std::size_t index) noexcept -> bool;

  template <class Map>
  static auto mapped(std::string_view text, Map map) -> std::string;
};

}
