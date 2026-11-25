#include <arisa/text/case_mapping.hpp>
#include <arisa/text/utf8.hpp>

namespace arisa::text {

template <class Map>
auto CaseMapping::mapped(std::string_view text, Map map) -> std::string {
  std::string result;
  result.reserve(text.size());
  std::size_t offset = 0;
  while (offset < text.size()) {
    const Utf8::Decoded decoded = Utf8::decode(text, offset);
    const std::size_t length = decoded.length == 0 ? 1 : decoded.length;
    if (decoded.codepoint < 0x80U) {
      result.push_back(static_cast<char>(map(decoded.codepoint)));
    } else if (decoded.codepoint == U'\u00DF' && map(U'a') == U'A') {
      result.append("SS");
    } else {
      const char32_t changed = map(decoded.codepoint);
      if (changed == decoded.codepoint) {
        result.append(text.substr(offset, length));
      } else {
        result.append(Utf8::encode(changed));
      }
    }
    offset += length;
  }
  return result;
}

auto CaseMapping::uppercased(std::string_view text) -> std::string {
  return mapped(text, [](char32_t codepoint) { return upper(codepoint); });
}

auto CaseMapping::lowercased(std::string_view text) -> std::string {
  return mapped(text, [](char32_t codepoint) { return lower(codepoint); });
}

auto CaseMapping::rules(const Locale& locale) noexcept -> Rules {
  const std::string_view language = locale.language();
  if (language == "tr" || language == "az") {
    return Rules::turkic;
  }
  if (language == "el") {
    return Rules::greek;
  }
  if (language == "lt") {
    return Rules::lithuanian;
  }
  return Rules::standard;
}

auto CaseMapping::decoded(std::string_view text) -> std::vector<char32_t> {
  std::vector<char32_t> codepoints;
  codepoints.reserve(text.size());
  std::size_t offset = 0;
  while (offset < text.size()) {
    const Utf8::Decoded each = Utf8::decode(text, offset);
    codepoints.push_back(each.codepoint);
    offset += each.length == 0 ? 1 : each.length;
  }
  return codepoints;
}

auto CaseMapping::letter(char32_t codepoint) noexcept -> bool {
  return (codepoint >= U'A' && codepoint <= U'Z') ||
         (codepoint >= U'a' && codepoint <= U'z') ||
         (codepoint >= 0x00C0U && codepoint <= 0x024FU &&
          codepoint != 0x00D7U && codepoint != 0x00F7U) ||
         (codepoint >= 0x0370U && codepoint <= 0x03FFU) ||
         (codepoint >= 0x0400U && codepoint <= 0x052FU);
}

auto CaseMapping::combining_above(char32_t codepoint) noexcept -> bool {
  return (codepoint >= 0x0300U && codepoint <= 0x0314U) ||
         (codepoint >= 0x033DU && codepoint <= 0x0344U) ||
         codepoint == 0x0346U ||
         (codepoint >= 0x034AU && codepoint <= 0x034CU) ||
         (codepoint >= 0x0350U && codepoint <= 0x0352U) ||
         codepoint == 0x0357U || codepoint == 0x035BU ||
         (codepoint >= 0x0363U && codepoint <= 0x036FU);
}

auto CaseMapping::combining(char32_t codepoint) noexcept -> bool {
  return codepoint >= 0x0300U && codepoint <= 0x036FU;
}

auto CaseMapping::more_above(const std::vector<char32_t>& codepoints,
                             std::size_t index) noexcept -> bool {
  for (std::size_t at = index + 1; at < codepoints.size(); ++at) {
    if (combining_above(codepoints[at])) {
      return true;
    }
    if (!combining(codepoints[at])) {
      return false;
    }
  }
  return false;
}

auto CaseMapping::after_soft_dotted(const std::vector<char32_t>& codepoints,
                                    std::size_t index) noexcept -> bool {
  for (std::size_t at = index; at-- > 0;) {
    const char32_t before = codepoints[at];
    if (before == U'i' || before == U'j' || before == 0x012FU ||
        before == 0x0268U || before == 0x1E2DU || before == 0x1ECBU) {
      return true;
    }
    if (!combining(before) || combining_above(before)) {
      return false;
    }
  }
  return false;
}

auto CaseMapping::unaccented_greek(char32_t codepoint) noexcept -> char32_t {
  switch (codepoint) {
    case 0x0386U:
    case 0x03ACU:
      return 0x0391U;
    case 0x0388U:
    case 0x03ADU:
      return 0x0395U;
    case 0x0389U:
    case 0x03AEU:
      return 0x0397U;
    case 0x038AU:
    case 0x03AFU:
      return 0x0399U;
    case 0x038CU:
    case 0x03CCU:
      return 0x039FU;
    case 0x038EU:
    case 0x03CDU:
      return 0x03A5U;
    case 0x038FU:
    case 0x03CEU:
      return 0x03A9U;
    case 0x0390U:
    case 0x03CAU:
      return 0x03AAU;
    case 0x03B0U:
    case 0x03CBU:
      return 0x03ABU;
    default:
      break;
  }
  return upper(codepoint);
}

auto CaseMapping::final_sigma(const std::vector<char32_t>& codepoints,
                              std::size_t index) noexcept -> bool {
  bool before = false;
  for (std::size_t at = index; at > 0; --at) {
    const char32_t previous = codepoints[at - 1];
    if (combining_above(previous)) {
      continue;
    }
    before = letter(previous);
    break;
  }
  if (!before) {
    return false;
  }
  for (std::size_t at = index + 1; at < codepoints.size(); ++at) {
    if (combining_above(codepoints[at])) {
      continue;
    }
    return !letter(codepoints[at]);
  }
  return true;
}

auto CaseMapping::uppercased(std::string_view text, const Locale& locale)
    -> std::string {
  const Rules rule = rules(locale);
  if (rule == Rules::standard) {
    return uppercased(text);
  }
  const std::vector<char32_t> codepoints = decoded(text);
  std::string result;
  result.reserve(text.size());
  for (std::size_t index = 0; index < codepoints.size(); ++index) {
    const char32_t codepoint = codepoints[index];
    if (rule == Rules::turkic && codepoint == U'i') {
      result.append(Utf8::encode(0x0130U));
    } else if (rule == Rules::turkic && codepoint == 0x0131U) {
      result.push_back('I');
    } else if (rule == Rules::greek &&
               (codepoint == 0x0301U || codepoint == 0x0342U ||
                codepoint == 0x0313U)) {
      continue;
    } else if (rule == Rules::greek) {
      result.append(Utf8::encode(unaccented_greek(codepoint)));
    } else if (rule == Rules::lithuanian && codepoint == 0x0307U &&
               after_soft_dotted(codepoints, index)) {
      continue;
    } else if (codepoint == 0x00DFU) {
      result.append("SS");
    } else {
      result.append(Utf8::encode(upper(codepoint)));
    }
  }
  return result;
}

auto CaseMapping::lowercased(std::string_view text, const Locale& locale)
    -> std::string {
  const Rules rule = rules(locale);
  const std::vector<char32_t> codepoints = decoded(text);
  std::string result;
  result.reserve(text.size());
  for (std::size_t index = 0; index < codepoints.size(); ++index) {
    const char32_t codepoint = codepoints[index];
    if (rule == Rules::turkic && codepoint == U'I') {
      if (index + 1 < codepoints.size() && codepoints[index + 1] == 0x0307U) {
        result.push_back('i');
        ++index;
      } else {
        result.append(Utf8::encode(0x0131U));
      }
    } else if (codepoint == 0x0130U) {
      result.push_back('i');
      if (rule != Rules::turkic) {
        result.append(Utf8::encode(0x0307U));
      }
    } else if (rule == Rules::lithuanian &&
               (codepoint == U'I' || codepoint == U'J' ||
                codepoint == 0x012EU) &&
               more_above(codepoints, index)) {
      result.append(Utf8::encode(lower(codepoint)));
      result.append(Utf8::encode(0x0307U));
    } else if (rule == Rules::lithuanian &&
               (codepoint == 0x00CCU || codepoint == 0x00CDU ||
                codepoint == 0x0128U)) {
      result.push_back('i');
      result.append(Utf8::encode(0x0307U));
      result.append(Utf8::encode(codepoint == 0x00CCU   ? 0x0300U
                                 : codepoint == 0x00CDU ? 0x0301U
                                                        : 0x0303U));
    } else if (codepoint == 0x03A3U && final_sigma(codepoints, index)) {
      result.append(Utf8::encode(0x03C2U));
    } else {
      result.append(Utf8::encode(lower(codepoint)));
    }
  }
  return result;
}

auto CaseMapping::paired(char32_t codepoint) noexcept -> bool {
  return (codepoint >= 0x0100U && codepoint <= 0x012FU) ||
         (codepoint >= 0x0132U && codepoint <= 0x0137U) ||
         (codepoint >= 0x014AU && codepoint <= 0x0177U) ||
         (codepoint >= 0x01DEU && codepoint <= 0x01EFU) ||
         (codepoint >= 0x0460U && codepoint <= 0x0481U) ||
         (codepoint >= 0x048AU && codepoint <= 0x04BFU) ||
         (codepoint >= 0x04D0U && codepoint <= 0x052FU) ||
         (codepoint >= 0x1E00U && codepoint <= 0x1E95U) ||
         (codepoint >= 0x1EA0U && codepoint <= 0x1EFFU);
}

auto CaseMapping::upper(char32_t codepoint) noexcept -> char32_t {
  if (codepoint >= U'a' && codepoint <= U'z') {
    return codepoint - 0x20U;
  }
  if ((codepoint >= 0x00E0U && codepoint <= 0x00FEU && codepoint != 0x00F7U) ||
      (codepoint >= 0x03B1U && codepoint <= 0x03CBU && codepoint != 0x03C2U) ||
      (codepoint >= 0x0430U && codepoint <= 0x044FU)) {
    return codepoint - 0x20U;
  }
  if (codepoint == 0x00FFU) {
    return 0x0178U;
  }
  if (codepoint == 0x03C2U) {
    return 0x03A3U;
  }
  if (codepoint == 0x03ACU) {
    return 0x0386U;
  }
  if (codepoint >= 0x03ADU && codepoint <= 0x03AFU) {
    return codepoint - 0x25U;
  }
  if (codepoint == 0x03CCU) {
    return 0x038CU;
  }
  if (codepoint == 0x03CDU || codepoint == 0x03CEU) {
    return codepoint - 0x3FU;
  }
  if (codepoint >= 0x0450U && codepoint <= 0x045FU) {
    return codepoint - 0x50U;
  }
  if (codepoint >= 0x0561U && codepoint <= 0x0586U) {
    return codepoint - 0x30U;
  }
  if (paired(codepoint) && (codepoint & 1U) == 1U) {
    return codepoint - 1U;
  }
  if (((codepoint >= 0x0139U && codepoint <= 0x0148U) ||
       (codepoint >= 0x0179U && codepoint <= 0x017EU)) &&
      (codepoint & 1U) == 0U) {
    return codepoint - 1U;
  }
  return codepoint;
}

auto CaseMapping::lower(char32_t codepoint) noexcept -> char32_t {
  if (codepoint >= U'A' && codepoint <= U'Z') {
    return codepoint + 0x20U;
  }
  if ((codepoint >= 0x00C0U && codepoint <= 0x00DEU && codepoint != 0x00D7U) ||
      (codepoint >= 0x0391U && codepoint <= 0x03ABU && codepoint != 0x03A2U) ||
      (codepoint >= 0x0410U && codepoint <= 0x042FU)) {
    return codepoint + 0x20U;
  }
  if (codepoint == 0x0178U) {
    return 0x00FFU;
  }
  if (codepoint == 0x0386U) {
    return 0x03ACU;
  }
  if (codepoint >= 0x0388U && codepoint <= 0x038AU) {
    return codepoint + 0x25U;
  }
  if (codepoint == 0x038CU) {
    return 0x03CCU;
  }
  if (codepoint == 0x038EU || codepoint == 0x038FU) {
    return codepoint + 0x3FU;
  }
  if (codepoint >= 0x0400U && codepoint <= 0x040FU) {
    return codepoint + 0x50U;
  }
  if (codepoint >= 0x0531U && codepoint <= 0x0556U) {
    return codepoint + 0x30U;
  }
  if (paired(codepoint) && (codepoint & 1U) == 0U) {
    return codepoint + 1U;
  }
  if (((codepoint >= 0x0139U && codepoint <= 0x0148U) ||
       (codepoint >= 0x0179U && codepoint <= 0x017EU)) &&
      (codepoint & 1U) == 1U) {
    return codepoint + 1U;
  }
  return codepoint;
}

}
