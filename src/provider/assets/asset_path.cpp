#include <arisa/provider/assets/asset_path.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

namespace arisa::provider {

AssetPath::AssetPath(std::string value) noexcept : value_(std::move(value)) {}

auto AssetPath::create(std::string_view path)
    -> std::expected<AssetPath, ProviderError> {
  const bool forbidden_characters =
      std::ranges::any_of(path, [](char character) {
        return character == '\\' || character == '\0' || character == ':';
      });
  const bool well_formed =
      !path.empty() && !forbidden_characters &&
      std::ranges::all_of(std::views::split(path, '/'), [](auto segment) {
        return valid_segment(std::string_view{segment});
      });
  if (!well_formed) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return AssetPath{std::string{path}};
}

auto AssetPath::valid_segment(std::string_view segment) noexcept -> bool {
  return !segment.empty() && segment != "." && segment != "..";
}

}
