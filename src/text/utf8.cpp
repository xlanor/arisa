#include <arisa/text/utf8.hpp>

#include <algorithm>

namespace arisa::text {

auto Utf8::continuation(unsigned char byte) noexcept -> bool {
  return (byte & 0xC0U) == 0x80U;
}

auto Utf8::valid(std::string_view text) noexcept -> bool {
  std::size_t offset = 0;
  while (offset < text.size()) {
    const auto lead = static_cast<unsigned char>(text[offset]);
    std::size_t length = 0;
    char32_t minimum = 0;
    char32_t codepoint = 0;
    if (lead < 0x80U) {
      ++offset;
      continue;
    }
    if ((lead & 0xE0U) == 0xC0U) {
      length = 2;
      minimum = 0x80;
      codepoint = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
      length = 3;
      minimum = 0x800;
      codepoint = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
      length = 4;
      minimum = 0x10000;
      codepoint = lead & 0x07U;
    } else {
      return false;
    }
    if (text.size() - offset < length) {
      return false;
    }
    for (std::size_t index = 1; index < length; ++index) {
      const auto byte = static_cast<unsigned char>(text[offset + index]);
      if (!continuation(byte)) {
        return false;
      }
      codepoint = (codepoint << 6U) | (byte & 0x3FU);
    }
    if (codepoint < minimum || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
      return false;
    }
    offset += length;
  }
  return true;
}

auto Utf8::decode(std::string_view text, std::size_t offset) noexcept
    -> Decoded {
  if (offset >= text.size()) {
    return {};
  }
  const auto lead = static_cast<unsigned char>(text[offset]);
  std::uint32_t length = 1;
  char32_t codepoint = lead;
  if ((lead & 0xE0U) == 0xC0U) {
    length = 2;
    codepoint = lead & 0x1FU;
  } else if ((lead & 0xF0U) == 0xE0U) {
    length = 3;
    codepoint = lead & 0x0FU;
  } else if ((lead & 0xF8U) == 0xF0U) {
    length = 4;
    codepoint = lead & 0x07U;
  }
  if (offset + length > text.size()) {
    return Decoded{.codepoint = 0xFFFD, .length = 1};
  }
  for (std::uint32_t index = 1; index < length; ++index) {
    codepoint = (codepoint << 6U) |
                (static_cast<unsigned char>(text[offset + index]) & 0x3FU);
  }
  return Decoded{.codepoint = codepoint, .length = length};
}

auto Utf8::previous(std::string_view text, std::size_t offset) noexcept
    -> std::size_t {
  if (offset == 0) {
    return 0;
  }
  std::size_t index = offset - 1;
  while (index > 0 && continuation(static_cast<unsigned char>(text[index]))) {
    --index;
  }
  return index;
}

auto Utf8::next(std::string_view text, std::size_t offset) noexcept
    -> std::size_t {
  if (offset >= text.size()) {
    return text.size();
  }
  std::size_t index = offset + 1;
  while (index < text.size() &&
         continuation(static_cast<unsigned char>(text[index]))) {
    ++index;
  }
  return index;
}

auto Utf8::boundary(std::string_view text, std::size_t offset) noexcept
    -> std::size_t {
  offset = std::min(offset, text.size());
  while (offset > 0 && offset < text.size() &&
         continuation(static_cast<unsigned char>(text[offset]))) {
    --offset;
  }
  return offset;
}

auto Utf8::encode(char32_t codepoint) -> std::string {
  std::string encoded;
  if (codepoint < 0x80) {
    encoded.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    encoded.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint < 0x10000) {
    encoded.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    encoded.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return encoded;
}

}
