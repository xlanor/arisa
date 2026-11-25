#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::provider {

enum class KeyboardType : std::uint8_t {
  automatic,
  ascii_capable,
  numbers_and_punctuation,
  url,
  number_pad,
  phone_pad,
  name_phone_pad,
  email_address,
  decimal_pad,
  web_search,
  ascii_capable_number_pad,
};

enum class TextContentType : std::uint8_t {
  name,
  given_name,
  family_name,
  nickname,
  username,
  password,
  new_password,
  one_time_code,
  email_address,
  telephone_number,
  url,
  full_street_address,
  postal_code,
  organization_name,
  job_title,
};

enum class TextInputAutocapitalization : std::uint8_t {
  never,
  words,
  sentences,
  characters,
};

enum class SubmitLabel : std::uint8_t {
  done,
  go,
  send,
  join,
  route,
  search,
  return_key,
  next,
  continue_key,
};

enum class TextInputTrait : std::uint8_t {
  keyboard_type,
  text_content_type,
  autocapitalization,
  autocorrection,
  submit_label,
  multiline,
};

class TextInputTraitSet {
 public:
  static constexpr std::uint8_t count = 6;

  constexpr TextInputTraitSet() noexcept = default;

  constexpr void insert(TextInputTrait trait) noexcept { bits_ |= bit(trait); }

  constexpr auto contains(TextInputTrait trait) const noexcept -> bool {
    return (bits_ & bit(trait)) != 0U;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  static auto name(TextInputTrait trait) noexcept -> std::string_view;

  friend constexpr auto operator&(TextInputTraitSet left,
                                  TextInputTraitSet right) noexcept
      -> TextInputTraitSet {
    TextInputTraitSet both;
    both.bits_ = static_cast<std::uint8_t>(left.bits_ & right.bits_);
    return both;
  }

  friend constexpr auto operator==(TextInputTraitSet, TextInputTraitSet)
      -> bool = default;

 private:
  static constexpr auto bit(TextInputTrait trait) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(trait));
  }

  std::uint8_t bits_{0};
};

struct TextInputTraits {
  KeyboardType keyboard{KeyboardType::automatic};
  std::optional<TextContentType> content{};
  std::optional<TextInputAutocapitalization> capitalization{};
  std::optional<bool> autocorrection_disabled{};
  SubmitLabel submit_label{SubmitLabel::return_key};
  bool multiline{false};

  static auto submit_text(SubmitLabel label) noexcept -> std::string_view;

  auto requested() const noexcept -> TextInputTraitSet;

  friend auto operator==(const TextInputTraits&, const TextInputTraits&)
      -> bool = default;
};

}
