#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/date_interval.hpp>
#include <arisa/components/system_format_style.hpp>
#include <arisa/components/time_data_source.hpp>
#include <arisa/components/timed_wake.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/composition/accessibility_text.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/text/text_fitting.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typesetting_language.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace arisa::composition {

class Node;

}

namespace arisa::components {

class Image;

class Text : public composition::Element {
 public:
  class Content;
  class Producer;
  class Interpolation;
  class AccessibilitySource;
  class Layout;
  class LayoutKey;
  class Glyphs;

  template <class Value, class Style>
  class Formatting;

  template <class Value, class Style>
  class Timing;

  using Date = std::chrono::system_clock::time_point;
  using LineStyle = composition::TextDecoration;
  using AlignmentStrategy = composition::TextAlignmentStrategy;
  using WritingDirectionStrategy = composition::WritingDirectionStrategy;

  static constexpr float marquee_speed = 30.0F;
  static constexpr float marquee_gap = 40.0F;
  static constexpr double marquee_delay = 1.0;
  static constexpr double marquee_frame_rate = 60.0;
  static constexpr float secondary_scale = 0.87F;
  static constexpr std::chrono::milliseconds live_margin{1};

  enum class DateStyle : std::uint8_t {
    date,
    time,
    relative,
    offset,
    timer,
  };

  explicit Text(std::string text);

  explicit Text(AttributedString text);

  explicit Text(text::LocalizedStringKey key);

  explicit Text(const Image& image);

  template <class Number>
    requires(std::is_arithmetic_v<Number> && !std::same_as<Number, bool>)
  Text(Number value, text::NumberFormatStyle format)
      : Text(number(value), std::move(format)) {}

  Text(std::chrono::sys_seconds date, text::DateFormatStyle format);

  Text(Date date, DateStyle style);

  explicit Text(DateInterval interval);

  template <class Value, class Style>
    requires(text::FormatStyleFor<Style, Value> &&
             !std::same_as<Style, text::NumberFormatStyle> &&
             !std::same_as<Style, text::DateFormatStyle>)
  Text(Value value, Style style)
      : Text(std::make_shared<const Formatting<Value, Style>>(
            std::move(value), std::move(style))) {}

  template <class Value, class Style>
  Text(TimeDataSource<Value> source, Style style)
      : Text(std::make_shared<const Timing<Value, Style>>(std::move(source),
                                                          std::move(style))) {}

  static auto relative(std::chrono::seconds offset) -> Text;

  static auto markdown(std::string_view source) -> Text;

  static auto timer_interval(DateInterval interval,
                             std::optional<Date> pause_time = std::nullopt,
                             bool counts_down = true, bool shows_hours = true)
      -> Text;

  static auto argument(Date date, DateStyle style)
      -> text::LocalizedStringKey::Argument;

  static auto timer_argument(DateInterval interval,
                             std::optional<Date> pause_time = std::nullopt,
                             bool counts_down = true, bool shows_hours = true)
      -> text::LocalizedStringKey::Argument;

  template <class Value, class Style>
  static auto argument(TimeDataSource<Value> source, Style style)
      -> text::LocalizedStringKey::Argument {
    return Text{std::move(source), std::move(style)}.interpolation();
  }

  static auto accessibility_name(theme::ColourValue colour)
      -> text::LocalizedStringKey::Argument;

  static auto colour_name(const theme::Colour& colour) -> std::string;

  auto interpolation() const -> text::LocalizedStringKey::Argument;

  template <TextAttribute T>
  auto custom_attribute(T value) && -> Text;

  auto font(theme::Font value) && -> Text;

  auto font_weight(theme::Font::Weight weight) && -> Text;

  using composition::Element::line_limit;

  auto line_limit(std::size_t lines) && -> Text;

  auto kerning(float kerning) && -> Text;

  auto drawing_options(text::DrawingOptions options) && -> Text;

  auto tracking(float tracking) && -> Text;

  auto baseline_offset(float offset) && -> Text;

  auto monospaced_digit(bool is_active = true) && -> Text;

  auto monospaced(bool is_active = true) && -> Text;

  auto marquee(bool is_active = true) && -> Text;

  auto bold(bool is_active = true) && -> Text;

  auto italic(bool is_active = true) && -> Text;

  auto underline(
      bool is_active = true,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Text;

  auto underline(
      bool is_active, LineStyle::Pattern pattern,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Text;

  auto strikethrough(
      bool is_active = true,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Text;

  auto strikethrough(
      bool is_active, LineStyle::Pattern pattern,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Text;

  auto font_design(std::optional<theme::Font::Design> design) && -> Text;

  auto font_width(std::optional<theme::Font::Width> width) && -> Text;

  auto text_scale(composition::TextScale scale,
                  bool is_enabled = true) && -> Text;

  auto foreground_color(theme::ColourValue colour) && -> Text;

  auto foreground_style(graphics::ShapeStyle style) && -> Text;

  auto typesetting_language(text::TypesettingLanguage language,
                            bool is_enabled = true) && -> Text;

  auto text_variant(composition::TextVariantPreference preference) && -> Text;

  auto accessibility_text_content_type(
      composition::AccessibilityTextContentType type) && -> Text;

  auto speech_adjusted_pitch(double value) && -> Text;

  auto speech_always_includes_punctuation(bool value = true) && -> Text;

  auto speech_announcements_queued(bool value = true) && -> Text;

  auto speech_spells_out_characters(bool value = true) && -> Text;

  operator composition::AccessibilityText() const;

  friend auto operator+(Text left, Text right) -> Text;

  friend auto operator==(const Text& left, const Text& right) -> bool;

  static auto text_style(
      const theme::Theme& theme, theme::Font font, const text::Locale& locale,
      std::optional<theme::Font::Weight> weight = std::nullopt)
      -> text::TextStyle;

  static auto text_style(const theme::Theme& theme, theme::Font font,
                         const text::Locale& locale,
                         std::optional<theme::Font::Weight> weight,
                         const composition::TextAttributes& attributes,
                         composition::LayoutDirection direction)
      -> text::TextStyle;

  static auto styled_runs(const theme::Theme& theme,
                          const text::TextStyle& base,
                          const AttributedString& attributed) -> text::TextRuns;

  template <class Measure>
  static auto variant_for(const std::vector<std::string>& variants,
                          float available, Measure measure)
      -> std::optional<std::size_t> {
    for (std::size_t index = 0; index < variants.size(); ++index) {
      if (measure(variants[index]) <= available + 0.01F) {
        return index;
      }
    }
    return variants.empty() ? std::nullopt : std::optional{variants.size() - 1};
  }

  static auto inherited(const composition::Node& node,
                        composition::TextAttributes attributes)
      -> composition::TextAttributes;

  static auto fitting(const composition::TextAttributes& attributes,
                      bool marquee) -> text::TextFitting;

  static auto cased(std::string_view text,
                    const composition::TextAttributes& attributes)
      -> std::optional<std::string>;

  static auto cased(std::string_view text,
                    const composition::TextAttributes& attributes,
                    const text::Locale& locale) -> std::optional<std::string>;

  static auto embolden(theme::Font::Weight weight) noexcept
      -> theme::Font::Weight;

  auto duplicate() const -> Text;

 private:
  using Number = std::variant<std::int64_t, double>;

  explicit Text(std::unique_ptr<Content> content);

  explicit Text(std::shared_ptr<const Producer> producer);

  class Clock {
   public:
    explicit Clock(Date anchor) noexcept : anchor_(anchor) {}

    auto format(Date now, const text::Locale& locale) const -> std::string;

    auto discrete_input(Date after) const -> std::optional<Date>;

    friend auto operator==(const Clock&, const Clock&) -> bool = default;

   private:
    Date anchor_;
  };

  class Paused {
   public:
    Paused(SystemFormatStyle::Timer timer, std::optional<Date> pause) noexcept
        : timer_(std::move(timer)), pause_(pause) {}

    auto format(Date now, const text::Locale& locale) const -> std::string;

    auto discrete_input(Date after) const -> std::optional<Date>;

    friend auto operator==(const Paused&, const Paused&) -> bool = default;

   private:
    SystemFormatStyle::Timer timer_;
    std::optional<Date> pause_;
  };

  static auto image_text(const Image& image) -> AttributedString;

  template <class Value>
  static auto number(Value value) noexcept -> Number {
    if constexpr (std::integral<Value>) {
      return Number{static_cast<std::int64_t>(value)};
    } else {
      return Number{static_cast<double>(value)};
    }
  }

  Text(Number value, text::NumberFormatStyle format);
};

class Text::Producer {
 public:
  struct Moment {
    Date now{};
    std::reference_wrapper<const text::Locale> locale;
    std::chrono::seconds zone{0};
  };

  Producer() = default;
  Producer(const Producer&) = delete;
  auto operator=(const Producer&) -> Producer& = delete;
  Producer(Producer&&) = delete;
  auto operator=(Producer&&) -> Producer& = delete;
  virtual ~Producer() = default;

  virtual auto produce(const Moment& moment) const -> std::string = 0;

  virtual auto next(Date now) const -> std::optional<Date> = 0;

  virtual auto live() const noexcept -> bool = 0;

  virtual auto tag() const noexcept -> const void* = 0;

  virtual auto same(const Producer& other) const -> bool = 0;

  virtual auto variants(const Moment&) const -> std::vector<std::string> {
    return {};
  }
};

template <class Value, class Style>
class Text::Formatting final : public Text::Producer {
 public:
  Formatting(Value value, Style style)
      : value_(std::move(value)), style_(std::move(style)) {}

  auto produce(const Moment& moment) const -> std::string override {
    return std::string{style_.format(value_, moment.locale.get())};
  }

  auto next(Date) const -> std::optional<Date> override { return std::nullopt; }

  auto live() const noexcept -> bool override { return false; }

  auto tag() const noexcept -> const void* override { return &marker; }

  auto same(const Producer& other) const -> bool override {
    if (other.tag() != tag()) {
      return false;
    }
    const auto& theirs = static_cast<const Formatting&>(other);
    if constexpr (std::equality_comparable<Value> &&
                  std::equality_comparable<Style>) {
      return value_ == theirs.value_ && style_ == theirs.style_;
    } else {
      return false;
    }
  }

 private:
  static constexpr char marker = 0;

  Value value_;
  Style style_;
};

template <class Value, class Style>
class Text::Timing final : public Text::Producer {
 public:
  Timing(TimeDataSource<Value> source, Style style)
      : source_(std::move(source)), style_(std::move(style)) {}

  auto produce(const Moment& moment) const -> std::string override {
    const Value value = source_.value(moment.now);
    if constexpr (requires { style_.format(value, moment.locale.get()); }) {
      return std::string{style_.format(value, moment.locale.get())};
    } else {
      return std::string{style_.format(value)};
    }
  }

  auto next(Date now) const -> std::optional<Date> override {
    if constexpr (requires { style_.discrete_input(source_.value(now)); }) {
      const auto found = style_.discrete_input(source_.value(now));
      if (!found.has_value()) {
        return std::nullopt;
      }
      return source_.date(*found);
    } else {
      return now + std::chrono::seconds{1};
    }
  }

  auto live() const noexcept -> bool override { return true; }

  auto tag() const noexcept -> const void* override { return &marker; }

  auto same(const Producer& other) const -> bool override {
    if (other.tag() != tag()) {
      return false;
    }
    const auto& theirs = static_cast<const Timing&>(other);
    if constexpr (std::equality_comparable<Style>) {
      return source_ == theirs.source_ && style_ == theirs.style_;
    } else {
      return false;
    }
  }

 private:
  static constexpr char marker = 0;

  TimeDataSource<Value> source_;
  Style style_;
};

class Text::Content final : public composition::ElementContent {
 public:
  struct Formatted {
    Number value;
    text::NumberFormatStyle style;

    friend auto operator==(const Formatted&, const Formatted&)
        -> bool = default;
  };

  struct Dated {
    std::chrono::sys_seconds instant;
    text::DateFormatStyle style;

    friend auto operator==(const Dated&, const Dated&) -> bool = default;
  };

  struct Relative {
    std::chrono::seconds offset;

    friend auto operator==(const Relative&, const Relative&) -> bool = default;
  };

  struct Produced {
    std::shared_ptr<const Producer> producer;

    friend auto operator==(const Produced& left, const Produced& right)
        -> bool {
      return left.producer == right.producer ||
             (left.producer != nullptr && right.producer != nullptr &&
              left.producer->same(*right.producer));
    }
  };

  struct Joined {
    std::vector<std::shared_ptr<const Content>> pieces;

    friend auto operator==(const Joined& left, const Joined& right) -> bool;
  };

  using Source = std::variant<std::monostate, text::LocalizedStringKey,
                              Formatted, Dated, Relative, Produced, Joined>;

  explicit Content(std::string text) : text_(std::move(text)) {}

  explicit Content(Source source);

  explicit Content(AttributedString text)
      : text_(std::move(text)), link_count_(text_.links().size()) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto activatable() const noexcept -> bool override;

  auto accessibility() const -> composition::AccessibilityDefaults override;

  auto localized_accessibility(const text::Locale& locale,
                               const text::StringCatalog& catalog) const
      -> composition::AccessibilityDefaults override;

  auto speech() const -> std::optional<composition::AccessibilitySpeech>;

  void set_speech(composition::AccessibilitySpeech speech) {
    speech_ = speech.over(speech_.value_or(composition::AccessibilitySpeech{}));
  }

  auto activate(composition::Context& context, const composition::Node& node,
                std::optional<composition::Point> at) -> bool override;

  auto steer(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) -> bool override;

  void enter(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) override;

  auto refreshes() const noexcept -> bool override;

  auto refresh(composition::Context& context, const composition::Node& node)
      -> bool override;

  auto text() const noexcept -> std::string_view {
    return shown_.has_value() ? std::string_view{*shown_} : text_.text();
  }

  auto attributed() const noexcept -> const AttributedString& { return text_; }

  auto cased(const composition::TextAttributes& attributes) const
      -> std::optional<AttributedString>;

  auto resolved(const composition::TextAttributes& attributes,
                const text::Locale& locale) const -> const AttributedString&;

  auto variants(const composition::TextAttributes& attributes,
                const text::Locale& locale) const -> std::vector<std::string>;

  auto source() const noexcept -> const Source& { return source_; }

  auto localized() const noexcept -> bool {
    return !std::holds_alternative<std::monostate>(source_);
  }

  auto live() const -> bool;

  auto next(Date now) const -> std::optional<Date>;

  auto moment() const noexcept -> const std::optional<Date>& { return moment_; }

  void set_moment(Date now) noexcept { moment_ = now; }

  auto link_count() const noexcept -> std::size_t { return link_count_; }

  auto foreground() const noexcept -> const std::optional<theme::ColourValue>& {
    return foreground_;
  }

  auto style() const noexcept -> const std::optional<graphics::ShapeStyle>& {
    return style_;
  }

  auto level() const -> AttributedString::Attributes;

  void set_text(AttributedString text);

  void set_source(Source source);

  void clear_level() noexcept;

  void set_foreground(theme::ColourValue colour) noexcept {
    foreground_ = colour;
  }

  void set_style(graphics::ShapeStyle style) { style_ = std::move(style); }

  void set_italic(bool is_active) noexcept { italic_ = is_active; }

  void set_custom(AttributedString::Custom custom) {
    custom_ = std::move(custom);
  }

  auto custom() const noexcept -> const AttributedString::Custom& {
    return custom_;
  }

  void set_underline(composition::TextDecoration decoration) {
    underline_ = std::move(decoration);
  }

  void set_strikethrough(composition::TextDecoration decoration) {
    strikethrough_ = std::move(decoration);
  }

  void set_design(std::optional<theme::Font::Design> design) noexcept {
    design_ = design;
  }

  void set_width(std::optional<theme::Font::Width> width) noexcept {
    width_ = width;
  }

  void set_scale(composition::TextScale scale) noexcept { scale_ = scale; }

  void set_language(std::optional<text::Locale> language) {
    language_ = std::move(language);
  }

  void set_variant(composition::TextVariantPreference preference) noexcept {
    variant_ = preference;
  }

  auto role() const noexcept -> theme::TextRole { return font().role(); }

  auto font() const noexcept -> theme::Font {
    return font_.value_or(theme::Font::body);
  }

  auto explicit_font() const noexcept -> const std::optional<theme::Font>& {
    return font_;
  }

  auto font_in(const std::optional<theme::Font>& inherited) const noexcept
      -> theme::Font {
    return font_.value_or(inherited.value_or(theme::Font::body));
  }

  auto weight() const noexcept -> std::optional<theme::Font::Weight> {
    return weight_;
  }

  void set_font(theme::Font value) noexcept { font_ = value; }

  void set_weight(theme::Font::Weight weight) noexcept { weight_ = weight; }

  auto line_limit() const noexcept -> std::optional<std::size_t> {
    return line_limit_;
  }

  void set_line_limit(std::size_t lines) noexcept { line_limit_ = lines; }

  void set_kerning(float kerning) noexcept { kerning_ = kerning; }

  void set_drawing_options(text::DrawingOptions options) noexcept {
    drawing_ = options;
  }

  void set_tracking(float tracking) noexcept { tracking_ = tracking; }

  void set_baseline_offset(float offset) noexcept { baseline_offset_ = offset; }

  void set_monospaced_digit(bool is_active) noexcept {
    monospaced_digit_ = is_active;
  }

  auto marquee() const noexcept -> bool { return marquee_; }

  void set_marquee(bool is_active) noexcept { marquee_ = is_active; }

  auto attributes_in(const composition::TextAttributes& inherited) const
      -> composition::TextAttributes;

  auto equals(const Content& other) const -> bool;

 private:
  struct Resolution {
    std::string locale;
    text::StringCatalog catalog;
    std::chrono::seconds zone{0};
    std::optional<composition::TextCase> text_case;
    std::optional<Date> moment;
    AttributedString text;
  };

  static const composition::StateKey<TimedWake> wake;

  static auto link_at(const composition::Node& node,
                      composition::Point at) noexcept
      -> std::optional<std::size_t>;

  auto source_text(const composition::TextAttributes& attributes,
                   const text::Locale& locale, Date moment) const
      -> AttributedString;

  auto interpolated(const text::LocalizedStringKey& key,
                    const composition::TextAttributes& attributes,
                    const text::Locale& locale, Date moment) const
      -> AttributedString;

  static auto embedded(const text::LocalizedStringKey& key)
      -> std::vector<const Content*>;

  auto plain(const text::Locale& locale) const -> std::string;

  AttributedString text_;
  Source source_;
  mutable std::optional<Resolution> resolution_;
  std::optional<Date> moment_;
  std::optional<std::string> shown_;
  std::optional<AttributedString> loaded_;
  std::optional<theme::ColourValue> foreground_;
  std::optional<graphics::ShapeStyle> style_;
  std::optional<bool> italic_;
  std::optional<composition::TextDecoration> underline_;
  std::optional<composition::TextDecoration> strikethrough_;
  std::optional<theme::Font::Design> design_;
  std::optional<theme::Font::Width> width_;
  std::optional<composition::TextScale> scale_;
  std::optional<text::Locale> language_;
  std::optional<composition::TextVariantPreference> variant_;
  std::size_t link_count_{0};
  std::optional<theme::Font> font_;
  std::optional<theme::Font::Weight> weight_;
  std::optional<std::size_t> line_limit_;
  std::optional<float> kerning_;
  std::optional<text::DrawingOptions> drawing_;
  std::optional<float> tracking_;
  std::optional<float> baseline_offset_;
  std::optional<bool> monospaced_digit_;
  bool marquee_{false};
  AttributedString::Custom custom_;
  std::optional<composition::AccessibilitySpeech> speech_;

  friend class Text;
};

class Text::AccessibilitySource final
    : public composition::AccessibilityText::Source {
 public:
  explicit AccessibilitySource(std::shared_ptr<const Content> content)
      : content_(std::move(content)) {}

  auto resolve(const text::Locale& locale,
               const text::StringCatalog& catalog) const
      -> std::string override;

  auto speech() const
      -> std::optional<composition::AccessibilitySpeech> override;

  auto tag() const noexcept -> const void* override { return &marker; }

  auto same(const composition::AccessibilityText::Source& other) const
      -> bool override;

 private:
  static constexpr char marker = 0;

  std::shared_ptr<const Content> content_;
};

class Text::Interpolation final : public text::Interpolation {
 public:
  explicit Interpolation(std::shared_ptr<const Content> content)
      : content_(std::move(content)) {}

  auto text(const text::Locale& locale) const -> std::string override;

  auto tag() const noexcept -> const void* override { return &marker; }

  auto same(const text::Interpolation& other) const -> bool override;

  auto rich() const noexcept -> bool override { return true; }

  auto content() const noexcept -> const Content& { return *content_; }

 private:
  static constexpr char marker = 0;

  std::shared_ptr<const Content> content_;
};

template <TextAttribute T>
auto Text::custom_attribute(T value) && -> Text {
  Content& content = edit<Content>();
  content.set_custom(content.custom().with(std::move(value)));
  return std::move(*this);
}

}
