#pragma once

#include <arisa/provider/error.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace arisa::provider {

class Url {
 public:
  [[nodiscard]] static auto create(std::string_view text)
      -> std::expected<Url, ProviderError>;

  auto value() const noexcept -> std::string_view { return value_; }

  auto secure() const noexcept -> bool;

  friend auto operator==(const Url&, const Url&) -> bool = default;

 private:
  explicit Url(std::string value) noexcept;

  static auto scheme_length(std::string_view text) noexcept -> std::size_t;

  std::string value_;
};

}
