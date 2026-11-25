#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/theme/typography.hpp>

#include <cstddef>

#include <optional>

namespace arisa::attr {

struct TextOptions {
  theme::Font font{theme::Font::body};
  std::optional<theme::Font::Weight> weight;
};

class FontOption {
 public:
  constexpr explicit FontOption(theme::Font value) noexcept : value_(value) {}

  template <class Options>
    requires requires(Options& options, theme::Font font) {
      options.font = font;
    }
  constexpr void configure(Options& options) const noexcept {
    options.font = value_;
  }

 private:
  theme::Font value_;
};

class FontWeight {
 public:
  constexpr explicit FontWeight(theme::Font::Weight value) noexcept
      : value_(value) {}

  constexpr void configure(TextOptions& options) const noexcept {
    options.weight = value_;
  }

 private:
  theme::Font::Weight value_;
};

template <class Value, auto Modifier>
class TextValue {
 public:
  constexpr explicit TextValue(Value value) noexcept : value_(value) {}

  auto apply(composition::Element element) const -> composition::Element {
    return (std::move(element).*Modifier)(value_);
  }

 private:
  Value value_;
};

class LineLimit {
 public:
  constexpr explicit LineLimit(std::size_t lines,
                               bool reserves_space = false) noexcept
      : lines_(lines), reserves_space_(reserves_space) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).line_limit(lines_, reserves_space_);
  }

 private:
  std::size_t lines_;
  bool reserves_space_;
};

using font_weight = FontWeight;
using line_limit = LineLimit;
using multiline_text_alignment =
    TextValue<composition::TextAlignment,
              static_cast<composition::Element (composition::Element::*)(
                  composition::TextAlignment) &&>(
                  &composition::Element::multiline_text_alignment)>;
using truncation_mode = TextValue<composition::TruncationMode,
                                  &composition::Element::truncation_mode>;
using minimum_scale_factor =
    TextValue<float, &composition::Element::minimum_scale_factor>;
using line_spacing = TextValue<float, &composition::Element::line_spacing>;
using allows_tightening =
    TextValue<bool, &composition::Element::allows_tightening>;
using text_case = TextValue<std::optional<composition::TextCase>,
                            &composition::Element::text_case>;
using kerning = TextValue<float, &composition::Element::kerning>;
using tracking = TextValue<float, &composition::Element::tracking>;
using baseline_offset =
    TextValue<float, &composition::Element::baseline_offset>;
using monospaced_digit =
    TextValue<bool, &composition::Element::monospaced_digit>;

namespace font {

using of = FontOption;

inline constexpr FontOption large_title{theme::Font::large_title};
inline constexpr FontOption title{theme::Font::title};
inline constexpr FontOption title2{theme::Font::title2};
inline constexpr FontOption title3{theme::Font::title3};
inline constexpr FontOption headline{theme::Font::headline};
inline constexpr FontOption subheadline{theme::Font::subheadline};
inline constexpr FontOption body{theme::Font::body};
inline constexpr FontOption callout{theme::Font::callout};
inline constexpr FontOption footnote{theme::Font::footnote};
inline constexpr FontOption caption{theme::Font::caption};
inline constexpr FontOption caption2{theme::Font::caption2};

}

}
