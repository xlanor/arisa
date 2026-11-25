#pragma once

#include <arisa/text/face_query.hpp>
#include <arisa/text/font_name.hpp>
#include <arisa/text/small_caps.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::theme {

enum class TextRole : std::uint8_t {
  large_title,
  title,
  title2,
  title3,
  headline,
  subheadline,
  body,
  callout,
  footnote,
  caption,
  caption2,
};

inline constexpr std::size_t text_role_count = 11;

enum class TypeDesign : std::uint8_t {
  text,
  display,
};

struct TypeStyle {
  float size{17.0F};
  std::uint16_t weight{400};
  TypeDesign design{TypeDesign::text};
  float tracking{0.0F};
  float line_height{22.0F};

  friend constexpr auto operator==(const TypeStyle&, const TypeStyle&)
      -> bool = default;
};

class Font {
 public:
  enum class Weight : std::uint16_t {
    ultra_light = 100,
    thin = 200,
    light = 300,
    regular = 400,
    medium = 500,
    semibold = 600,
    bold = 700,
    heavy = 800,
    black = 900,
  };

  enum class Design : std::uint8_t {
    standard,
    rounded,
    serif,
    monospaced,
  };

  class Width {
   public:
    static const Width compressed;
    static const Width condensed;
    static const Width standard;
    static const Width expanded;

    constexpr explicit Width(float value) noexcept : value_(value) {}

    constexpr auto value() const noexcept -> float { return value_; }

    constexpr auto stretch() const noexcept -> float {
      constexpr std::array<std::array<float, 2>, 4> stops{
          {{-0.3F, 0.69F}, {-0.2F, 0.81F}, {0.0F, 1.0F}, {0.2F, 1.25F}}};
      if (value_ <= stops.front()[0]) {
        return std::max(stops.front()[1] + ((value_ - stops.front()[0]) *
                                            (stops[1][1] - stops[0][1]) /
                                            (stops[1][0] - stops[0][0])),
                        0.1F);
      }
      for (std::size_t index = 1; index < stops.size(); ++index) {
        if (value_ <= stops[index][0]) {
          const float share = (value_ - stops[index - 1][0]) /
                              (stops[index][0] - stops[index - 1][0]);
          return stops[index - 1][1] +
                 (share * (stops[index][1] - stops[index - 1][1]));
        }
      }
      return stops.back()[1] +
             ((value_ - stops.back()[0]) * (stops[3][1] - stops[2][1]) /
              (stops[3][0] - stops[2][0]));
    }

    friend constexpr auto operator==(Width, Width) -> bool = default;

   private:
    float value_;
  };

  enum class Leading : std::uint8_t {
    standard,
    tight,
    loose,
  };

  class Context;
  struct Resolved;

  struct FixedSize {
    float points{17.0F};
  };

  static constexpr float compressed_width = 0.69F;
  static constexpr float condensed_width = 0.81F;
  static constexpr float expanded_width = 1.25F;
  static constexpr float leading_step = 2.0F;

  static const Font large_title;
  static const Font title;
  static const Font title2;
  static const Font title3;
  static const Font headline;
  static const Font subheadline;
  static const Font body;
  static const Font callout;
  static const Font footnote;
  static const Font caption;
  static const Font caption2;

  constexpr explicit Font(TextRole role) noexcept : role_(role) {}

  static constexpr auto system(float size,
                               Weight weight = Weight::regular) noexcept
      -> Font {
    return Font{TextRole::body, size, weight};
  }

  static constexpr auto system(float size, Weight weight,
                               Design design) noexcept -> Font {
    Font font{TextRole::body, size, weight};
    font.design_ = design;
    return font;
  }

  static constexpr auto system(
      TextRole style, std::optional<Design> design = std::nullopt,
      std::optional<Weight> weight = std::nullopt) noexcept -> Font {
    Font font{style};
    font.design_ = design;
    font.weight_ = weight;
    return font;
  }

  constexpr Font(text::FontName name, float size) noexcept
      : role_(TextRole::body), size_(size), name_(name), fixed_(true) {}

  static constexpr auto custom(std::string_view name, float size) noexcept
      -> Font {
    Font font{TextRole::body};
    font.size_ = size;
    font.name_ = text::FontName{name};
    return font;
  }

  static constexpr auto custom(std::string_view name, float size,
                               TextRole relative_to) noexcept -> Font {
    Font font{relative_to};
    font.size_ = size;
    font.name_ = text::FontName{name};
    return font;
  }

  static constexpr auto custom(std::string_view name, FixedSize size) noexcept
      -> Font {
    Font font{TextRole::body};
    font.size_ = size.points;
    font.name_ = text::FontName{name};
    font.fixed_ = true;
    return font;
  }

  constexpr auto role() const noexcept -> TextRole { return role_; }

  constexpr auto name() const noexcept -> std::optional<text::FontName> {
    return name_;
  }

  constexpr auto design() const noexcept -> std::optional<Design> {
    return design_;
  }

  constexpr auto width() const noexcept -> std::optional<Width> {
    return width_;
  }

  constexpr auto is_italic() const noexcept -> bool { return italic_; }

  constexpr auto italic() const noexcept -> Font { return italic(true); }

  constexpr auto italic(bool is_active) const noexcept -> Font {
    Font changed = *this;
    changed.italic_ = is_active;
    return changed;
  }

  constexpr auto bold() const noexcept -> Font { return weight(Weight::bold); }

  constexpr auto bold(bool is_active) const noexcept -> Font {
    Font changed = *this;
    if (is_active) {
      changed.weight_ = Weight::bold;
    } else if (changed.weight_ == Weight::bold) {
      changed.weight_.reset();
    }
    return changed;
  }

  constexpr auto leading(Leading value) const noexcept -> Font {
    Font changed = *this;
    changed.leading_ = value;
    return changed;
  }

  constexpr auto leading() const noexcept -> Leading { return leading_; }

  constexpr auto point_size(float points) const noexcept -> Font {
    Font changed = *this;
    changed.size_ = points;
    changed.fixed_ = true;
    return changed;
  }

  constexpr auto small_caps() const noexcept -> Font {
    return small_caps(true);
  }

  constexpr auto small_caps(bool is_active) const noexcept -> Font {
    return capitals(text::SmallCaps::all, is_active);
  }

  constexpr auto lowercase_small_caps() const noexcept -> Font {
    return lowercase_small_caps(true);
  }

  constexpr auto lowercase_small_caps(bool is_active) const noexcept -> Font {
    return capitals(text::SmallCaps::lowercase, is_active);
  }

  constexpr auto uppercase_small_caps() const noexcept -> Font {
    return uppercase_small_caps(true);
  }

  constexpr auto uppercase_small_caps(bool is_active) const noexcept -> Font {
    return capitals(text::SmallCaps::uppercase, is_active);
  }

  constexpr auto capitals() const noexcept -> text::SmallCaps {
    return small_caps_;
  }

  constexpr auto monospaced_digit() const noexcept -> Font {
    Font changed = *this;
    changed.monospaced_digit_ = true;
    return changed;
  }

  constexpr auto is_monospaced_digit() const noexcept -> bool {
    return monospaced_digit_;
  }

  constexpr auto is_fixed_size() const noexcept -> bool { return fixed_; }

  auto resolve(const Context& context) const -> Resolved;

  constexpr auto weight(Weight value) const noexcept -> Font {
    Font changed = *this;
    changed.weight_ = value;
    return changed;
  }

  constexpr auto width(Width value) const noexcept -> Font {
    Font changed = *this;
    changed.width_ = value;
    return changed;
  }

  constexpr auto monospaced() const noexcept -> Font {
    return monospaced(true);
  }

  constexpr auto monospaced(bool is_active) const noexcept -> Font {
    Font changed = *this;
    if (is_active) {
      changed.design_ = Design::monospaced;
    } else if (changed.design_ == Design::monospaced) {
      changed.design_.reset();
    }
    return changed;
  }

  static constexpr auto stretch(Width value) noexcept -> float {
    return value.stretch();
  }

  constexpr auto size() const noexcept -> std::optional<float> { return size_; }

  constexpr auto weight() const noexcept -> std::optional<Weight> {
    return weight_;
  }

  constexpr auto scale() const noexcept -> float { return scale_; }

  constexpr auto scaled(float factor) const noexcept -> Font {
    Font scaled_font = *this;
    scaled_font.scale_ = scale_ * factor;
    return scaled_font;
  }

  friend constexpr auto operator==(const Font&, const Font&) -> bool = default;

 private:
  constexpr Font(TextRole role, float size, Weight weight) noexcept
      : role_(role), size_(size), weight_(weight) {}

  constexpr auto capitals(text::SmallCaps caps, bool is_active) const noexcept
      -> Font {
    Font changed = *this;
    const auto bits = static_cast<std::uint8_t>(caps);
    const auto held = static_cast<std::uint8_t>(small_caps_);
    changed.small_caps_ = static_cast<text::SmallCaps>(
        is_active ? (held | bits) : (held & static_cast<std::uint8_t>(~bits)));
    return changed;
  }

  TextRole role_;
  std::optional<float> size_;
  std::optional<Weight> weight_;
  float scale_{1.0F};
  std::optional<text::FontName> name_;
  std::optional<Design> design_;
  std::optional<Width> width_;
  bool italic_{false};
  Leading leading_{Leading::standard};
  text::SmallCaps small_caps_{text::SmallCaps::none};
  bool monospaced_digit_{false};
  bool fixed_{false};
};

inline constexpr Font::Width Font::Width::compressed{-0.3F};
inline constexpr Font::Width Font::Width::condensed{-0.2F};
inline constexpr Font::Width Font::Width::standard{0.0F};
inline constexpr Font::Width Font::Width::expanded{0.2F};

class Font::Context {
 public:
  using Styles = std::array<TypeStyle, text_role_count>;

  Context(Styles styles, Styles base, bool bold_text) noexcept
      : styles_(styles), base_(base), bold_text_(bold_text) {}

  auto style(TextRole role) const noexcept -> const TypeStyle& {
    return styles_.at(static_cast<std::size_t>(role));
  }

  auto base(TextRole role) const noexcept -> const TypeStyle& {
    return base_.at(static_cast<std::size_t>(role));
  }

  auto bold_text() const noexcept -> bool { return bold_text_; }

  friend auto operator==(const Context&, const Context&) -> bool = default;

 private:
  Styles styles_;
  Styles base_;
  bool bold_text_;
};

struct Font::Resolved {
  float point_size{17.0F};
  Weight weight{Weight::regular};
  Width width{Width::standard};
  Leading leading{Leading::standard};
  bool is_bold{false};
  bool is_italic{false};
  bool is_monospaced{false};
  bool is_small_caps{false};
  bool is_lowercase_small_caps{false};
  bool is_uppercase_small_caps{false};
  text::FaceQuery face{};

  friend auto operator==(const Resolved&, const Resolved&) -> bool = default;
};

inline constexpr Font Font::large_title{TextRole::large_title};
inline constexpr Font Font::title{TextRole::title};
inline constexpr Font Font::title2{TextRole::title2};
inline constexpr Font Font::title3{TextRole::title3};
inline constexpr Font Font::headline{TextRole::headline};
inline constexpr Font Font::subheadline{TextRole::subheadline};
inline constexpr Font Font::body{TextRole::body};
inline constexpr Font Font::callout{TextRole::callout};
inline constexpr Font Font::footnote{TextRole::footnote};
inline constexpr Font Font::caption{TextRole::caption};
inline constexpr Font Font::caption2{TextRole::caption2};

}
