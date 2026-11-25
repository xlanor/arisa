#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::text {

class ByteCountFormatStyle {
 public:
  enum class Style : std::uint8_t {
    file,
    memory,
    decimal,
    binary,
  };

  enum class Unit : std::uint8_t {
    bytes = 1U << 0U,
    kb = 1U << 1U,
    mb = 1U << 2U,
    gb = 1U << 3U,
    tb = 1U << 4U,
    pb = 1U << 5U,
  };

  class Units {
   public:
    static const Units all;
    static const Units default_;

    constexpr Units(Unit unit) noexcept
        : bits_(static_cast<std::uint8_t>(unit)) {}

    constexpr auto contains(Unit unit) const noexcept -> bool {
      return (bits_ & static_cast<std::uint8_t>(unit)) != 0;
    }

    friend constexpr auto operator|(Units left, Units right) noexcept -> Units {
      return Units{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
    }

    friend constexpr auto operator==(Units, Units) -> bool = default;

   private:
    constexpr explicit Units(std::uint8_t bits) noexcept : bits_(bits) {}

    std::uint8_t bits_;
  };

  static constexpr double threshold = 0.999;

  explicit ByteCountFormatStyle(Style style = Style::file,
                                Units allowed = Units::all,
                                bool spells_out_zero = true,
                                bool includes_actual_byte_count = false)
      : style_(style),
        allowed_(allowed),
        spells_out_zero_(spells_out_zero),
        includes_actual_(includes_actual_byte_count) {}

  auto locale(Locale value) const -> ByteCountFormatStyle {
    ByteCountFormatStyle changed = *this;
    changed.locale_ = std::move(value);
    return changed;
  }

  auto style() const noexcept -> Style { return style_; }

  auto format(std::int64_t bytes, const Locale& locale) const -> std::string;

  auto parse(std::string_view text, const Locale& locale) const
      -> std::optional<std::int64_t>;

  friend auto operator==(const ByteCountFormatStyle&,
                         const ByteCountFormatStyle&) -> bool = default;

 private:
  static constexpr std::size_t unit_count = 6;

  auto chosen(std::int64_t bytes) const noexcept -> std::size_t;

  auto base() const noexcept -> double;

  static auto counted(std::int64_t bytes, LocaleData data, const Locale& locale)
      -> std::string;

  Style style_;
  Units allowed_;
  bool spells_out_zero_;
  bool includes_actual_;
  std::optional<Locale> locale_;
};

inline constexpr ByteCountFormatStyle::Units ByteCountFormatStyle::Units::all{
    static_cast<std::uint8_t>(0x3FU)};
inline constexpr ByteCountFormatStyle::Units
    ByteCountFormatStyle::Units::default_{static_cast<std::uint8_t>(0x3FU)};

}
