#include <arisa/text/locale.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace arisa::text {

Locale::Locale(std::string language, std::string script, std::string region)
    : language_(std::move(language)),
      script_(std::move(script)),
      region_(std::move(region)) {
  tag_ = language_;
  if (!script_.empty()) {
    tag_ += '-';
    tag_ += script_;
  }
  if (!region_.empty()) {
    tag_ += '-';
    tag_ += region_;
  }
}

auto Locale::create(std::string_view tag) -> std::optional<Locale> {
  std::string language;
  std::string script;
  std::string region;
  std::size_t index = 0;
  std::size_t start = 0;
  while (start <= tag.size()) {
    const std::size_t end = tag.find_first_of("-_", start);
    const std::string_view subtag =
        tag.substr(start, end == std::string_view::npos ? std::string_view::npos
                                                        : end - start);
    if (subtag.empty() || subtag.size() > 8 || !alphanumeric(subtag)) {
      return std::nullopt;
    }
    if (index == 0) {
      if (subtag.size() < 2 || subtag.size() > 3 || !alphabetic(subtag)) {
        return std::nullopt;
      }
      language = lower(subtag);
    } else if (subtag.size() == 4 && alphabetic(subtag) && script.empty() &&
               region.empty()) {
      script = lower(subtag);
      script.front() = static_cast<char>(
          std::toupper(static_cast<unsigned char>(script.front())));
    } else if (((subtag.size() == 2 && alphabetic(subtag)) ||
                (subtag.size() == 3 &&
                 std::ranges::all_of(subtag,
                                     [](char character) {
                                       return std::isdigit(
                                                  static_cast<unsigned char>(
                                                      character)) != 0;
                                     }))) &&
               region.empty()) {
      region = upper(subtag);
    }
    ++index;
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return Locale{std::move(language), std::move(script), std::move(region)};
}

auto Locale::undetermined() -> Locale { return Locale{"und", "", ""}; }

auto Locale::right_to_left() const noexcept -> bool {
  constexpr std::array<std::string_view, 12> languages{
      "ar", "he", "fa", "ur", "yi", "ps", "dv", "ckb", "sd", "ug", "syr", "iw"};
  if (script_ == "Arab" || script_ == "Hebr" || script_ == "Thaa" ||
      script_ == "Syrc") {
    return true;
  }
  if (!script_.empty()) {
    return false;
  }
  return std::ranges::contains(languages, std::string_view{language_});
}

auto Locale::alphabetic(std::string_view subtag) noexcept -> bool {
  return std::ranges::all_of(subtag, [](char character) {
    return std::isalpha(static_cast<unsigned char>(character)) != 0;
  });
}

auto Locale::alphanumeric(std::string_view subtag) noexcept -> bool {
  return std::ranges::all_of(subtag, [](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) != 0;
  });
}

auto Locale::lower(std::string_view subtag) -> std::string {
  std::string result{subtag};
  std::ranges::transform(result, result.begin(), [](char character) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  });
  return result;
}

auto Locale::upper(std::string_view subtag) -> std::string {
  std::string result{subtag};
  std::ranges::transform(result, result.begin(), [](char character) {
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(character)));
  });
  return result;
}

}
