#pragma once

#include <arisa/text/locale.hpp>

#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/text_run.hpp>
#include <arisa/text/text_runs.hpp>
#include <arisa/text/typeface.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

template <class T>
concept TextAttribute =
    std::equality_comparable<T> && std::copy_constructible<T>;

class AttributedTextSelection;

class AttributedString {
 public:
  struct Speech {
    bool spells_out_characters{false};
    bool includes_punctuation{false};
    std::optional<double> adjusted_pitch{};
    std::optional<std::string> phonetic_notation{};
    std::optional<composition::AccessibilityHeadingLevel> heading_level{};
    std::optional<composition::AccessibilityTextContentType> content_type{};

    friend auto operator==(const Speech&, const Speech&) -> bool = default;
  };

  struct InlineImage {
    std::shared_ptr<const provider::Image> pixels{};
    float scale{1.0F};
    std::string name{};
    std::string label{};

    auto width() const noexcept -> float;

    auto height() const noexcept -> float;

    friend auto operator==(const InlineImage&, const InlineImage&)
        -> bool = default;
  };

  struct AdaptiveImageGlyph {
    std::shared_ptr<const provider::Image> image{};
    std::string identifier{};
    std::string content_description{};

    friend auto operator==(const AdaptiveImageGlyph&, const AdaptiveImageGlyph&)
        -> bool = default;
  };

  class Custom {
   public:
    template <TextAttribute T>
    auto with(T value) const -> Custom {
      Custom changed = *this;
      std::erase_if(changed.values_, [](const auto& held) {
        return held->template get<T>() != nullptr;
      });
      changed.values_.push_back(composition::TagValue::of(std::move(value)));
      return changed;
    }

    template <TextAttribute T>
    auto get() const -> std::optional<T> {
      for (const auto& held : values_) {
        if (const T* found = held->template get<T>()) {
          return *found;
        }
      }
      return std::nullopt;
    }

    auto empty() const noexcept -> bool { return values_.empty(); }

    static auto merged(const Custom& base, const Custom& over) -> Custom;

    friend auto operator==(const Custom& left, const Custom& right) -> bool;

   private:
    std::vector<std::shared_ptr<const composition::TagValue>> values_;
  };

  struct Attributes {
    std::optional<theme::Font::Weight> weight{};
    bool italic{false};
    std::optional<theme::Font::Design> design{};
    std::optional<theme::ColourValue> foreground{};
    bool underline{false};
    bool strikethrough{false};
    std::optional<std::string> link{};
    std::optional<theme::Font> font{};
    std::optional<float> kern{};
    std::optional<float> tracking{};
    std::optional<float> baseline_offset{};
    std::optional<theme::ColourValue> background{};
    std::optional<composition::TextDecoration> underline_style{};
    std::optional<composition::TextDecoration> strikethrough_style{};
    std::optional<text::LineHeight> line_height{};
    std::optional<composition::TextAlignment> alignment{};
    std::optional<Speech> accessibility{};
    std::optional<InlineImage> image{};
    std::optional<AdaptiveImageGlyph> adaptive_image_glyph{};
    std::optional<graphics::ShapeStyle> shading{};
    Custom custom{};

    friend auto operator==(const Attributes&, const Attributes&)
        -> bool = default;
  };

  auto interpolation() const -> text::LocalizedStringKey::Argument;

  struct Link {
    std::size_t index{0};
    std::uint32_t start{0};
    std::uint32_t end{0};
    std::string_view destination{};
  };

  template <class Value, Value Attributes::* Member>
  struct Field {
    using value_type = Value;

    static auto of(const Attributes& attributes) -> const Value& {
      return attributes.*Member;
    }

    static auto of(Attributes& attributes) -> Value& {
      return attributes.*Member;
    }
  };

  struct Scopes;

  static constexpr std::size_t markdown_depth = 64;
  static constexpr std::string_view replacement = "￼";
  static constexpr float glyph_height = 1.2F;

  AttributedString() = default;

  explicit AttributedString(std::string text);

  AttributedString(std::string text, const Attributes& attributes);

  template <class Transferable>
    requires requires(const Transferable& transferable) {
      { transferable.resolved() } -> std::convertible_to<AttributedString>;
    }
  explicit AttributedString(const Transferable& transferable)
      : AttributedString(transferable.resolved()) {}

  static auto markdown(std::string_view source) -> AttributedString;

  auto markdown_source() const -> std::string;

  static auto image(InlineImage image) -> AttributedString;

  static auto image(InlineImage image, const Attributes& attributes)
      -> AttributedString;

  static auto glyph(AdaptiveImageGlyph glyph) -> AttributedString;

  static auto glyph(AdaptiveImageGlyph glyph, const Attributes& attributes)
      -> AttributedString;

  auto append(std::string_view text) -> AttributedString&;

  auto append(std::string_view text, const Attributes& attributes)
      -> AttributedString&;

  auto append(const AttributedString& other) -> AttributedString&;

  auto applying(const Attributes& attributes) const -> AttributedString;

  auto applying(const Attributes& attributes, std::uint32_t start,
                std::uint32_t end) const -> AttributedString;

  template <class Attribute>
  auto setting(const typename Attribute::value_type& value, std::uint32_t start,
               std::uint32_t end) const -> AttributedString {
    return transformed(start, end, [&value](Attributes& attributes) {
      Attribute::of(attributes) = value;
    });
  }

  template <class Attribute>
  auto value(std::uint32_t offset) const -> typename Attribute::value_type {
    return Attribute::of(attributes(offset));
  }

  auto cased(composition::TextCase text_case) const -> AttributedString;

  auto cased(composition::TextCase text_case, const text::Locale& locale) const
      -> AttributedString;

  auto resolving(const std::function<std::shared_ptr<const provider::Image>(
                     std::string_view)>& load) const -> AttributedString;

  auto text() const noexcept -> std::string_view { return text_; }

  auto runs() const noexcept -> const text::TextRuns& { return runs_; }

  auto inks() const noexcept -> std::span<const theme::ColourValue> {
    return inks_;
  }

  auto destinations() const noexcept -> std::span<const std::string> {
    return destinations_;
  }

  auto fonts() const noexcept -> std::span<const theme::Font> { return fonts_; }

  auto images() const noexcept -> std::span<const InlineImage> {
    return images_;
  }

  auto glyphs() const noexcept -> std::span<const AdaptiveImageGlyph> {
    return glyphs_;
  }

  auto speech() const noexcept -> std::span<const Speech> { return speech_; }

  auto shadings() const noexcept -> std::span<const graphics::ShapeStyle> {
    return shadings_;
  }

  auto replacing(std::uint32_t start, std::uint32_t end,
                 const AttributedString& inserted) const -> AttributedString;

  void replace_selection(AttributedTextSelection& selection,
                         const AttributedString& inserted);

  void replace_selection(AttributedTextSelection& selection,
                         std::string_view characters);

  void transform_attributes(AttributedTextSelection& selection,
                            const std::function<void(Attributes&)>& body);

  void transform(AttributedTextSelection& selection,
                 const std::function<void(AttributedString&)>& body);

  auto transforming(std::uint32_t start, std::uint32_t end,
                    const std::function<void(Attributes&)>& change) const
      -> AttributedString;

  auto substring(std::uint32_t start, std::uint32_t end) const
      -> AttributedString;

  auto unresolved() const noexcept -> bool;

  auto links() const -> std::vector<Link>;

  auto attributes(std::uint32_t offset) const -> Attributes;

  static auto typeface(theme::Font::Design design) noexcept -> text::Typeface;

  friend auto operator+(AttributedString left, const AttributedString& right)
      -> AttributedString;

  friend auto operator==(const AttributedString& left,
                         const AttributedString& right) -> bool;

 private:
  auto run_for(const Attributes& attributes, std::uint32_t start,
               std::uint32_t end) -> std::optional<text::TextRun>;

  static auto merged(const Attributes& base, const Attributes& over)
      -> Attributes;

  template <class Value>
  static auto index_of(std::vector<Value>& table, const Value& value)
      -> std::uint16_t;

  auto transformed(std::uint32_t start, std::uint32_t end,
                   const std::function<void(Attributes&)>& change) const
      -> AttributedString;

  void push(text::TextRun run);

  void freeze();

  auto boundaries() const -> std::vector<std::uint32_t>;

  auto attributes_of(const text::TextRun& run) const -> Attributes;

  std::string text_;
  std::vector<text::TextRun> pieces_;
  text::TextRuns runs_;
  std::vector<theme::ColourValue> inks_;
  std::vector<std::string> destinations_;
  std::vector<theme::Font> fonts_;
  std::vector<InlineImage> images_;
  std::vector<AdaptiveImageGlyph> glyphs_;
  std::vector<Speech> speech_;
  std::vector<graphics::ShapeStyle> shadings_;
  std::vector<Custom> customs_;
};

struct AttributedString::Scopes {
  struct FoundationAttributes {
    struct LinkAttribute
        : Field<std::optional<std::string>, &Attributes::link> {
      static constexpr std::string_view name = "NSLink";
    };
  };

  struct SwiftUIAttributes {
    struct FontAttribute
        : Field<std::optional<theme::Font>, &Attributes::font> {
      static constexpr std::string_view name = "SwiftUI.Font";
    };

    struct ForegroundColorAttribute
        : Field<std::optional<theme::ColourValue>, &Attributes::foreground> {
      static constexpr std::string_view name = "SwiftUI.ForegroundColor";
    };

    struct BackgroundColorAttribute
        : Field<std::optional<theme::ColourValue>, &Attributes::background> {
      static constexpr std::string_view name = "SwiftUI.BackgroundColor";
    };

    struct KerningAttribute : Field<std::optional<float>, &Attributes::kern> {
      static constexpr std::string_view name = "SwiftUI.Kern";
    };

    struct TrackingAttribute
        : Field<std::optional<float>, &Attributes::tracking> {
      static constexpr std::string_view name = "SwiftUI.Tracking";
    };

    struct BaselineOffsetAttribute
        : Field<std::optional<float>, &Attributes::baseline_offset> {
      static constexpr std::string_view name = "SwiftUI.BaselineOffset";
    };

    struct UnderlineStyleAttribute
        : Field<std::optional<composition::TextDecoration>,
                &Attributes::underline_style> {
      static constexpr std::string_view name = "SwiftUI.UnderlineStyle";
    };

    struct StrikethroughStyleAttribute
        : Field<std::optional<composition::TextDecoration>,
                &Attributes::strikethrough_style> {
      static constexpr std::string_view name = "SwiftUI.StrikethroughStyle";
    };

    struct LineHeightAttribute
        : Field<std::optional<text::LineHeight>, &Attributes::line_height> {
      static constexpr std::string_view name = "SwiftUI.LineHeight";
    };

    struct AlignmentAttribute : Field<std::optional<composition::TextAlignment>,
                                      &Attributes::alignment> {
      static constexpr std::string_view name = "SwiftUI.Alignment";
    };

    struct AccessibilityAttribute
        : Field<std::optional<Speech>, &Attributes::accessibility> {
      static constexpr std::string_view name = "SwiftUI.Accessibility";
    };

    struct AdaptiveImageGlyphAttribute
        : Field<std::optional<AdaptiveImageGlyph>,
                &Attributes::adaptive_image_glyph> {
      static constexpr std::string_view name = "SwiftUI.AdaptiveImageGlyph";
    };

    static constexpr std::array<std::string_view, 12> names{
        FontAttribute::name,
        ForegroundColorAttribute::name,
        BackgroundColorAttribute::name,
        KerningAttribute::name,
        TrackingAttribute::name,
        BaselineOffsetAttribute::name,
        UnderlineStyleAttribute::name,
        StrikethroughStyleAttribute::name,
        LineHeightAttribute::name,
        AlignmentAttribute::name,
        AccessibilityAttribute::name,
        AdaptiveImageGlyphAttribute::name};

    static constexpr auto foundation() noexcept -> std::string_view {
      return FoundationAttributes::LinkAttribute::name;
    }
  };

  static constexpr auto swift_ui() noexcept
      -> std::span<const std::string_view> {
    return SwiftUIAttributes::names;
  }
};

}
