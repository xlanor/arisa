#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::text {

class Utf8 {
 public:
  struct Decoded {
    char32_t codepoint{0};
    std::uint32_t length{0};
  };

  static auto valid(std::string_view text) noexcept -> bool;

  static auto decode(std::string_view text, std::size_t offset) noexcept
      -> Decoded;

  static auto previous(std::string_view text, std::size_t offset) noexcept
      -> std::size_t;

  static auto next(std::string_view text, std::size_t offset) noexcept
      -> std::size_t;

  static auto boundary(std::string_view text, std::size_t offset) noexcept
      -> std::size_t;

  static auto encode(char32_t codepoint) -> std::string;

 private:
  static auto continuation(unsigned char byte) noexcept -> bool;
};

}
