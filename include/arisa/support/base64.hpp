#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::support {

class Base64 {
 public:
  static constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  static auto encode(std::span<const std::byte> bytes) -> std::string {
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t at = 0; at < bytes.size(); at += 3) {
      const std::size_t left = bytes.size() - at;
      const std::uint32_t chunk =
          (std::to_integer<std::uint32_t>(bytes[at]) << 16U) |
          (left > 1 ? std::to_integer<std::uint32_t>(bytes[at + 1]) << 8U
                    : 0U) |
          (left > 2 ? std::to_integer<std::uint32_t>(bytes[at + 2]) : 0U);
      out.push_back(alphabet[(chunk >> 18U) & 0x3FU]);
      out.push_back(alphabet[(chunk >> 12U) & 0x3FU]);
      out.push_back(left > 1 ? alphabet[(chunk >> 6U) & 0x3FU] : '=');
      out.push_back(left > 2 ? alphabet[chunk & 0x3FU] : '=');
    }
    return out;
  }

  static auto decode(std::string_view text)
      -> std::optional<std::vector<std::byte>> {
    if (text.size() % 4 != 0) {
      return std::nullopt;
    }
    std::vector<std::byte> out;
    out.reserve((text.size() / 4) * 3);
    for (std::size_t at = 0; at < text.size(); at += 4) {
      std::array<std::uint32_t, 4> sextets{};
      std::size_t padding = 0;
      for (std::size_t index = 0; index < 4; ++index) {
        const char symbol = text[at + index];
        if (symbol == '=' && at + 4 == text.size() && index >= 2) {
          ++padding;
          continue;
        }
        if (padding > 0) {
          return std::nullopt;
        }
        const std::size_t found = alphabet.find(symbol);
        if (found == std::string_view::npos) {
          return std::nullopt;
        }
        sextets.at(index) = static_cast<std::uint32_t>(found);
      }
      const std::uint32_t chunk = (sextets[0] << 18U) | (sextets[1] << 12U) |
                                  (sextets[2] << 6U) | sextets[3];
      out.push_back(static_cast<std::byte>((chunk >> 16U) & 0xFFU));
      if (padding < 2) {
        out.push_back(static_cast<std::byte>((chunk >> 8U) & 0xFFU));
      }
      if (padding < 1) {
        out.push_back(static_cast<std::byte>(chunk & 0xFFU));
      }
    }
    return out;
  }
};

}
