#pragma once

#include <arisa/provider/error.hpp>

#include <compare>
#include <expected>
#include <string>
#include <string_view>

namespace arisa::provider {

class AssetPath {
 public:
  [[nodiscard]] static auto create(std::string_view path)
      -> std::expected<AssetPath, ProviderError>;

  auto value() const noexcept -> std::string_view { return value_; }

  friend auto operator==(const AssetPath&, const AssetPath&) -> bool = default;
  friend auto operator<=>(const AssetPath&, const AssetPath&) = default;

 private:
  explicit AssetPath(std::string value) noexcept;

  static auto valid_segment(std::string_view segment) noexcept -> bool;

  std::string value_;
};

}
