#pragma once

#include <cstdint>
#include <string_view>

namespace arisa::layout {

enum class LayoutErrorCode : std::uint8_t {
  engine_failure,
};

class LayoutError {
 public:
  constexpr explicit LayoutError(LayoutErrorCode code) noexcept : code_(code) {}

  constexpr auto code() const noexcept -> LayoutErrorCode { return code_; }

  auto description() const noexcept -> std::string_view;

  friend constexpr auto operator==(const LayoutError&, const LayoutError&)
      -> bool = default;

 private:
  LayoutErrorCode code_;
};

}
