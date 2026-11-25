#include <arisa/provider/browser/url.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace arisa::provider {

Url::Url(std::string value) noexcept : value_(std::move(value)) {}

auto Url::create(std::string_view text) -> std::expected<Url, ProviderError> {
  const std::size_t prefix = scheme_length(text);
  const std::string_view rest =
      prefix == 0 ? std::string_view{} : text.substr(prefix);
  const std::string_view authority = rest.substr(0, rest.find_first_of("/?#"));
  const bool printable = std::ranges::all_of(text, [](char character) {
    const auto code = static_cast<unsigned char>(character);
    return code > 0x20 && code < 0x7F;
  });
  if (prefix == 0 || authority.empty() || !printable) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return Url{std::string{text}};
}

auto Url::secure() const noexcept -> bool {
  return scheme_length(value_) == std::string_view{"https://"}.size();
}

auto Url::scheme_length(std::string_view text) noexcept -> std::size_t {
  const auto starts_with = [text](std::string_view scheme) {
    return text.size() >= scheme.size() &&
           std::ranges::equal(
               text.substr(0, scheme.size()), scheme,
               [](char left, char right) {
                 return std::tolower(static_cast<unsigned char>(left)) == right;
               });
  };
  if (starts_with("https://")) {
    return std::string_view{"https://"}.size();
  }
  if (starts_with("http://")) {
    return std::string_view{"http://"}.size();
  }
  return 0;
}

}
