#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/text/case_mapping.hpp>
#include <arisa/text/font_weight.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/string_catalog.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::components {

const composition::StateKey<TimedWake> Text::Content::wake{"text_live"};

Text::Text(std::string text)
    : Element(std::make_unique<Content>(std::move(text))) {}

Text::Text(AttributedString text)
    : Element(std::make_unique<Content>(std::move(text))) {}

Text::Text(text::LocalizedStringKey key)
    : Element(std::make_unique<Content>(Content::Source{std::move(key)})) {}

Text::Text(const Image& image)
    : Element(std::make_unique<Content>(image_text(image))) {}

Text::Text(Number value, text::NumberFormatStyle format)
    : Element(std::make_unique<Content>(Content::Source{
          Content::Formatted{.value = value, .style = std::move(format)}})) {}

Text::Text(std::chrono::sys_seconds date, text::DateFormatStyle format)
    : Element(std::make_unique<Content>(Content::Source{
          Content::Dated{.instant = date, .style = std::move(format)}})) {}

Text::Text(std::unique_ptr<Content> content) : Element(std::move(content)) {}

auto Text::duplicate() const -> Text {
  Text made{std::make_unique<Content>(static_cast<const Content&>(content()))};
  made.edit_modifiers() = modifiers();
  return made;
}

Text::Text(std::shared_ptr<const Producer> producer)
    : Element(std::make_unique<Content>(
          Content::Source{Content::Produced{std::move(producer)}})) {}

Text::Text(DateInterval interval)
    : Text(std::make_shared<
           const Formatting<DateInterval, DateIntervalFormatStyle>>(
          interval, DateIntervalFormatStyle{})) {}

Text::Text(Date date, DateStyle style)
    : Text(std::make_unique<Content>(Content::Source{std::monostate{}})) {
  using Style = text::DateFormatStyle;
  Content& content = edit<Content>();
  const auto instant = std::chrono::floor<std::chrono::seconds>(date);
  switch (style) {
    case DateStyle::date:
      content.set_source(Content::Dated{
          .instant = instant,
          .style =
              Style{Style::DateStyle::long_form, Style::TimeStyle::omitted}});
      return;
    case DateStyle::time:
      content.set_source(Content::Dated{
          .instant = instant,
          .style =
              Style{Style::DateStyle::omitted, Style::TimeStyle::shortened}});
      return;
    case DateStyle::relative:
      content.set_source(Content::Produced{
          std::make_shared<const Timing<Date, SystemFormatStyle::DateOffset>>(
              TimeDataSource<Date>::current_date(),
              SystemFormatStyle::DateOffset{date,
                                            SystemFormatStyle::Fields::all, 2,
                                            SystemFormatStyle::Sign::never})});
      return;
    case DateStyle::offset:
      content.set_source(Content::Produced{
          std::make_shared<const Timing<Date, SystemFormatStyle::DateOffset>>(
              TimeDataSource<Date>::current_date(),
              SystemFormatStyle::DateOffset{date,
                                            SystemFormatStyle::Fields::all, 1,
                                            SystemFormatStyle::Sign::always})});
      return;
    case DateStyle::timer:
      content.set_source(
          Content::Produced{std::make_shared<const Timing<Date, Clock>>(
              TimeDataSource<Date>::current_date(), Clock{date})});
      return;
  }
}

auto Text::relative(std::chrono::seconds offset) -> Text {
  return Text{
      std::make_unique<Content>(Content::Source{Content::Relative{offset}})};
}

auto Text::timer_interval(DateInterval interval, std::optional<Date> pause_time,
                          bool counts_down, bool shows_hours) -> Text {
  const SystemFormatStyle::Timer timer =
      counts_down
          ? SystemFormatStyle::Timer::counting_down(interval, shows_hours)
          : SystemFormatStyle::Timer::counting_up(interval, shows_hours);
  return Text{std::make_shared<const Timing<Date, Paused>>(
      TimeDataSource<Date>::current_date(), Paused{timer, pause_time})};
}

auto Text::argument(Date date, DateStyle style)
    -> text::LocalizedStringKey::Argument {
  return Text{date, style}.interpolation();
}

auto Text::timer_argument(DateInterval interval, std::optional<Date> pause_time,
                          bool counts_down, bool shows_hours)
    -> text::LocalizedStringKey::Argument {
  return timer_interval(interval, pause_time, counts_down, shows_hours)
      .interpolation();
}

auto Text::accessibility_name(theme::ColourValue colour)
    -> text::LocalizedStringKey::Argument {
  if (const theme::ColourRole* role = colour.role()) {
    switch (*role) {
      case theme::ColourRole::red:
        return std::string{"red"};
      case theme::ColourRole::orange:
        return std::string{"orange"};
      case theme::ColourRole::yellow:
        return std::string{"yellow"};
      case theme::ColourRole::green:
        return std::string{"green"};
      case theme::ColourRole::mint:
        return std::string{"mint"};
      case theme::ColourRole::teal:
        return std::string{"teal"};
      case theme::ColourRole::cyan:
        return std::string{"cyan"};
      case theme::ColourRole::blue:
        return std::string{"blue"};
      case theme::ColourRole::indigo:
        return std::string{"indigo"};
      case theme::ColourRole::purple:
        return std::string{"purple"};
      case theme::ColourRole::pink:
        return std::string{"pink"};
      case theme::ColourRole::brown:
        return std::string{"brown"};
      case theme::ColourRole::gray:
      case theme::ColourRole::gray2:
      case theme::ColourRole::gray3:
      case theme::ColourRole::gray4:
      case theme::ColourRole::gray5:
      case theme::ColourRole::gray6:
        return std::string{"gray"};
      default:
        return std::string{"color"};
    }
  }
  if (const theme::Colour* literal = colour.literal()) {
    return colour_name(*literal);
  }
  return std::string{"color"};
}

auto Text::colour_name(const theme::Colour& colour) -> std::string {
  const float high = std::max({colour.red, colour.green, colour.blue});
  const float low = std::min({colour.red, colour.green, colour.blue});
  const float chroma = high - low;
  const float saturation = high <= 0.0F ? 0.0F : chroma / high;
  if (saturation < 0.15F || chroma < 0.06F) {
    if (high < 0.12F) {
      return "black";
    }
    if (high < 0.4F) {
      return "dark gray";
    }
    if (high < 0.7F) {
      return "gray";
    }
    if (high < 0.93F) {
      return "light gray";
    }
    return "white";
  }
  float hue = 0.0F;
  if (high == colour.red) {
    hue = std::fmod((colour.green - colour.blue) / chroma, 6.0F);
  } else if (high == colour.green) {
    hue = ((colour.blue - colour.red) / chroma) + 2.0F;
  } else {
    hue = ((colour.red - colour.green) / chroma) + 4.0F;
  }
  hue *= 60.0F;
  if (hue < 0.0F) {
    hue += 360.0F;
  }
  std::string name;
  if (hue < 15.0F || hue >= 345.0F) {
    name = "red";
  } else if (hue < 45.0F) {
    name = high < 0.6F ? "brown" : "orange";
  } else if (hue < 70.0F) {
    name = "yellow";
  } else if (hue < 165.0F) {
    name = "green";
  } else if (hue < 200.0F) {
    name = "cyan";
  } else if (hue < 255.0F) {
    name = "blue";
  } else if (hue < 290.0F) {
    name = "purple";
  } else {
    name = "pink";
  }
  if (high < 0.45F && name != "brown") {
    return "dark " + name;
  }
  if (saturation < 0.45F && high > 0.85F) {
    return "light " + name;
  }
  return name;
}

auto Text::interpolation() const -> text::LocalizedStringKey::Argument {
  const auto& content = static_cast<const Content&>(this->content());
  return text::LocalizedStringKey::Interpolated{
      std::make_shared<const Interpolation>(
          std::make_shared<const Content>(content))};
}

auto Text::image_text(const Image& image) -> AttributedString {
  const auto& content = static_cast<const Image::Content&>(image.content());
  AttributedString::Attributes attributes;
  if (content.font().has_value()) {
    attributes.font = content.font();
  }
  if (content.system()) {
    const std::optional<SystemSymbols::Symbol> symbol =
        SystemSymbols::resolve(content.name(), composition::SymbolVariants{});
    if (symbol.has_value()) {
      return AttributedString{symbol->base, attributes};
    }
    return AttributedString{};
  }
  return AttributedString::image(
      AttributedString::InlineImage{
          .pixels = content.pixels(),
          .scale = content.scale(),
          .name = std::string{content.name()},
          .label = content.label().value_or(std::string{content.name()})},
      attributes);
}

Text::Content::Content(Source source) : source_(std::move(source)) {
  if (const auto* key = std::get_if<text::LocalizedStringKey>(&source_)) {
    text_ = AttributedString{std::string{key->key()}};
  }
}

auto operator==(const Text::Content::Joined& left,
                const Text::Content::Joined& right) -> bool {
  return std::ranges::equal(
      left.pieces, right.pieces,
      [](const std::shared_ptr<const Text::Content>& one,
         const std::shared_ptr<const Text::Content>& other) {
        return one == other ||
               (one != nullptr && other != nullptr && one->equals(*other));
      });
}

auto Text::markdown(std::string_view source) -> Text {
  return Text{AttributedString::markdown(source)};
}

auto Text::font(theme::Font value) && -> Text {
  edit<Content>().set_font(value);
  return std::move(*this);
}

auto Text::font_weight(theme::Font::Weight weight) && -> Text {
  edit<Content>().set_weight(weight);
  return std::move(*this);
}

auto Text::line_limit(std::size_t lines) && -> Text {
  edit<Content>().set_line_limit(lines);
  return std::move(*this);
}

auto Text::drawing_options(text::DrawingOptions options) && -> Text {
  edit<Content>().set_drawing_options(options);
  return std::move(*this);
}

auto Text::kerning(float kerning) && -> Text {
  edit<Content>().set_kerning(kerning);
  return std::move(*this);
}

auto Text::tracking(float tracking) && -> Text {
  edit<Content>().set_tracking(tracking);
  return std::move(*this);
}

auto Text::baseline_offset(float offset) && -> Text {
  edit<Content>().set_baseline_offset(offset);
  return std::move(*this);
}

auto Text::monospaced_digit(bool is_active) && -> Text {
  edit<Content>().set_monospaced_digit(is_active);
  return std::move(*this);
}

auto Text::monospaced(bool is_active) && -> Text {
  edit<Content>().set_design(
      is_active ? std::optional{theme::Font::Design::monospaced}
                : std::nullopt);
  return std::move(*this);
}

auto Text::bold(bool is_active) && -> Text {
  edit<Content>().set_weight(is_active ? theme::Font::Weight::bold
                                       : theme::Font::Weight::regular);
  return std::move(*this);
}

auto Text::italic(bool is_active) && -> Text {
  edit<Content>().set_italic(is_active);
  return std::move(*this);
}

auto Text::underline(bool is_active,
                     std::optional<theme::ColourValue> colour) && -> Text {
  edit<Content>().set_underline(
      composition::TextDecoration{.active = is_active, .colour = colour});
  return std::move(*this);
}

auto Text::underline(bool is_active, LineStyle::Pattern pattern,
                     std::optional<theme::ColourValue> colour) && -> Text {
  edit<Content>().set_underline(composition::TextDecoration{
      .active = is_active, .colour = colour, .pattern = pattern});
  return std::move(*this);
}

auto Text::strikethrough(bool is_active,
                         std::optional<theme::ColourValue> colour) && -> Text {
  edit<Content>().set_strikethrough(
      composition::TextDecoration{.active = is_active, .colour = colour});
  return std::move(*this);
}

auto Text::strikethrough(bool is_active, LineStyle::Pattern pattern,
                         std::optional<theme::ColourValue> colour) && -> Text {
  edit<Content>().set_strikethrough(composition::TextDecoration{
      .active = is_active, .colour = colour, .pattern = pattern});
  return std::move(*this);
}

auto Text::font_design(std::optional<theme::Font::Design> design) && -> Text {
  edit<Content>().set_design(design);
  return std::move(*this);
}

auto Text::font_width(std::optional<theme::Font::Width> width) && -> Text {
  edit<Content>().set_width(width);
  return std::move(*this);
}

auto Text::text_scale(composition::TextScale scale,
                      bool is_enabled) && -> Text {
  edit<Content>().set_scale(is_enabled ? scale
                                       : composition::TextScale::standard);
  return std::move(*this);
}

auto Text::foreground_color(theme::ColourValue colour) && -> Text {
  edit<Content>().set_foreground(colour);
  return std::move(*this);
}

auto Text::foreground_style(graphics::ShapeStyle style) && -> Text {
  if (const auto* colour = std::get_if<theme::ColourValue>(&style.value())) {
    edit<Content>().set_foreground(*colour);
  } else {
    edit<Content>().set_style(style);
    composition::Element& base = *this;
    base = std::move(base).foreground_style(std::move(style));
  }
  return std::move(*this);
}

auto Text::typesetting_language(text::TypesettingLanguage language,
                                bool is_enabled) && -> Text {
  edit<Content>().set_language(is_enabled ? language.language() : std::nullopt);
  return std::move(*this);
}

auto Text::text_variant(
    composition::TextVariantPreference preference) && -> Text {
  edit<Content>().set_variant(preference);
  return std::move(*this);
}

auto Text::accessibility_text_content_type(
    composition::AccessibilityTextContentType type) && -> Text {
  edit_accessibility().text_content_type = type;
  return std::move(*this);
}

auto Text::speech_adjusted_pitch(double value) && -> Text {
  edit<Content>().set_speech(
      composition::AccessibilitySpeech{.adjusted_pitch = value});
  return std::move(*this);
}

auto Text::speech_always_includes_punctuation(bool value) && -> Text {
  edit<Content>().set_speech(
      composition::AccessibilitySpeech{.always_includes_punctuation = value});
  return std::move(*this);
}

auto Text::speech_announcements_queued(bool value) && -> Text {
  edit<Content>().set_speech(
      composition::AccessibilitySpeech{.announcements_queued = value});
  return std::move(*this);
}

auto Text::speech_spells_out_characters(bool value) && -> Text {
  edit<Content>().set_speech(
      composition::AccessibilitySpeech{.spells_out_characters = value});
  return std::move(*this);
}

Text::operator composition::AccessibilityText() const {
  const auto& content = static_cast<const Content&>(this->content());
  return composition::AccessibilityText{
      std::make_shared<const AccessibilitySource>(
          std::make_shared<const Content>(content))};
}

auto Text::AccessibilitySource::resolve(
    const text::Locale& locale, const text::StringCatalog& catalog) const
    -> std::string {
  return content_->localized_accessibility(locale, catalog)
      .label.value_or(std::string{});
}

auto Text::AccessibilitySource::speech() const
    -> std::optional<composition::AccessibilitySpeech> {
  return content_->speech();
}

auto Text::AccessibilitySource::same(
    const composition::AccessibilityText::Source& other) const -> bool {
  if (other.tag() != tag()) {
    return false;
  }
  const auto& theirs = static_cast<const AccessibilitySource&>(other);
  return content_->equals(*theirs.content_);
}

auto operator+(Text left, Text right) -> Text {
  Text::Content& first = left.edit<Text::Content>();
  Text::Content& second = right.edit<Text::Content>();
  if (!first.localized() && !second.localized()) {
    AttributedString joined = first.attributed().applying(first.level()) +
                              second.attributed().applying(second.level());
    first.set_text(std::move(joined));
    first.clear_level();
    return left;
  }
  Text::Content::Joined joined;
  for (Text::Content* side : {&first, &second}) {
    const auto* held = std::get_if<Text::Content::Joined>(&side->source());
    if (held != nullptr && side->level() == AttributedString::Attributes{}) {
      joined.pieces.insert(joined.pieces.end(), held->pieces.begin(),
                           held->pieces.end());
    } else {
      joined.pieces.push_back(std::make_shared<const Text::Content>(*side));
    }
  }
  first.set_text(AttributedString{});
  first.set_source(std::move(joined));
  first.clear_level();
  return left;
}

auto operator==(const Text& left, const Text& right) -> bool {
  const auto& mine = left.interactions().accessibility;
  const auto& theirs = right.interactions().accessibility;
  const bool same_accessibility =
      mine == theirs ||
      (mine != nullptr && theirs != nullptr && *mine == *theirs);
  return static_cast<const Text::Content&>(left.content())
             .equals(static_cast<const Text::Content&>(right.content())) &&
         left.modifiers() == right.modifiers() && same_accessibility;
}

auto Text::marquee(bool is_active) && -> Text {
  edit<Content>().set_marquee(is_active);
  return std::move(*this);
}

auto Text::text_style(const theme::Theme& theme, theme::Font font,
                      const text::Locale& locale,
                      std::optional<theme::Font::Weight> weight)
    -> text::TextStyle {
  const theme::TypeStyle type = theme.type(font.role());
  float size = font.size().value_or(type.size);
  if (font.size().has_value() && font.name().has_value() &&
      !font.is_fixed_size()) {
    const theme::TypeStyle& base = theme.base_type(font.role());
    if (base.size > 0.0F) {
      size *= type.size / base.size;
    }
  }
  size *= font.scale();
  const std::optional<theme::Font::Weight> chosen =
      weight.has_value() ? weight : font.weight();
  text::TextStyle style;
  style.size = text::PointSize::create(size).value_or(style.size);
  style.weight =
      text::FontWeight::create(chosen.has_value() ? std::to_underlying(*chosen)
                                                  : type.weight)
          .value_or(style.weight);
  style.design = type.design == theme::TypeDesign::display
                     ? text::FontDesign::display
                     : text::FontDesign::text;
  style.tracking = type.tracking;
  style.line_height =
      text::PointSize::create(type.line_height * (size / type.size));
  style.locale = locale;
  style.italic = font.is_italic();
  if (font.design().has_value()) {
    style.typeface = AttributedString::typeface(*font.design());
  }
  if (font.width().has_value()) {
    style.stretch = theme::Font::stretch(*font.width());
  }
  style.name = font.name();
  style.small_caps = font.capitals();
  style.tabular_digits = font.is_monospaced_digit();
  if (!font.size().has_value()) {
    switch (font.leading()) {
      case theme::Font::Leading::standard:
        break;
      case theme::Font::Leading::tight:
        style.line_spacing = -theme::Font::leading_step;
        break;
      case theme::Font::Leading::loose:
        style.line_spacing = theme::Font::leading_step;
        break;
    }
  }
  return style;
}

auto Text::text_style(const theme::Theme& theme, theme::Font font,
                      const text::Locale& locale,
                      std::optional<theme::Font::Weight> weight,
                      const composition::TextAttributes& attributes,
                      composition::LayoutDirection direction)
    -> text::TextStyle {
  text::TextStyle style = text_style(
      theme, font, locale, weight.has_value() ? weight : attributes.weight);
  if (attributes.legibility_weight == composition::LegibilityWeight::bold) {
    style.weight =
        text::FontWeight::create(
            std::to_underlying(embolden(
                static_cast<theme::Font::Weight>(style.weight.value()))))
            .value_or(style.weight);
  }
  style.italic = style.italic || attributes.italic;
  if (attributes.design.has_value()) {
    style.typeface = AttributedString::typeface(*attributes.design);
  }
  if (attributes.width.has_value()) {
    style.stretch = theme::Font::stretch(*attributes.width);
  }
  if (attributes.scale == composition::TextScale::secondary) {
    style.size = text::PointSize::create(style.size.value() * secondary_scale)
                     .value_or(style.size);
  }
  style.underline = attributes.underline.active;
  style.strikethrough = attributes.strikethrough.active;
  style.underline_pattern = attributes.underline.pattern;
  style.strikethrough_pattern = attributes.strikethrough.pattern;
  style.tracking += attributes.tracking;
  style.kerning = attributes.kerning;
  style.drawing = attributes.drawing;
  style.baseline_offset = attributes.baseline_offset;
  style.line_spacing += attributes.line_spacing;
  style.tabular_digits = style.tabular_digits || attributes.monospaced_digit;
  style.line_rule = attributes.line_height;
  style.language = attributes.typesetting_language;
  const bool mirrored =
      direction == composition::LayoutDirection::right_to_left;
  if (attributes.writing_direction ==
      composition::WritingDirectionStrategy::layout_based) {
    style.direction = mirrored ? text::WritingDirection::right_to_left
                               : text::WritingDirection::left_to_right;
  }
  switch (attributes.multiline_alignment) {
    case composition::TextAlignment::leading:
      switch (attributes.alignment_strategy) {
        case composition::TextAlignmentStrategy::layout_based:
          style.alignment =
              mirrored ? text::TextAlignment::right : text::TextAlignment::left;
          break;
        case composition::TextAlignmentStrategy::default_:
        case composition::TextAlignmentStrategy::writing_direction_based:
          style.alignment = text::TextAlignment::natural;
          break;
      }
      break;
    case composition::TextAlignment::center:
      style.alignment = text::TextAlignment::center;
      break;
    case composition::TextAlignment::trailing:
      switch (attributes.alignment_strategy) {
        case composition::TextAlignmentStrategy::writing_direction_based:
          style.alignment = text::TextAlignment::opposite;
          break;
        case composition::TextAlignmentStrategy::default_:
        case composition::TextAlignmentStrategy::layout_based:
          style.alignment =
              mirrored ? text::TextAlignment::left : text::TextAlignment::right;
          break;
      }
      break;
  }
  return style;
}

auto Text::styled_runs(const theme::Theme& theme, const text::TextStyle& base,
                       const AttributedString& attributed) -> text::TextRuns {
  if (attributed.fonts().empty() && attributed.glyphs().empty()) {
    return attributed.runs();
  }
  const theme::Font::Context context = theme.font_context();
  std::vector<text::TextRun> runs{attributed.runs().runs().begin(),
                                  attributed.runs().runs().end()};
  for (text::TextRun& run : runs) {
    if (run.font > 0 && run.font <= attributed.fonts().size()) {
      const theme::Font& font = attributed.fonts()[run.font - 1];
      const theme::Font::Resolved resolved = font.resolve(context);
      run.size = text::PointSize::create(resolved.point_size);
      if (!run.weight.has_value() &&
          (font.weight().has_value() ||
           resolved.weight != static_cast<theme::Font::Weight>(
                                  theme.type(font.role()).weight))) {
        run.weight =
            text::FontWeight::create(std::to_underlying(resolved.weight));
      } else if (!run.weight.has_value()) {
        run.weight = text::FontWeight::create(theme.type(font.role()).weight);
      }
      run.italic = run.italic || resolved.is_italic;
      if (!run.typeface.has_value() && font.design().has_value()) {
        run.typeface = AttributedString::typeface(*font.design());
      }
      if (font.width().has_value()) {
        run.stretch = font.width()->stretch();
      }
      if (font.name().has_value()) {
        run.name = font.name();
      }
      run.small_caps =
          text::SmallCapitals::combined(run.small_caps, font.capitals());
      run.tabular_digits = run.tabular_digits || font.is_monospaced_digit();
    }
    if (run.glyph > 0 && run.glyph <= attributed.glyphs().size()) {
      const AttributedString::AdaptiveImageGlyph& glyph =
          attributed.glyphs()[run.glyph - 1];
      const float height =
          run.size.value_or(base.size).value() * AttributedString::glyph_height;
      float width = height;
      if (glyph.image != nullptr && glyph.image->height() > 0) {
        width = height * static_cast<float>(glyph.image->width()) /
                static_cast<float>(glyph.image->height());
      }
      run.attachment = text::TextAttachment{
          .width = width, .height = height, .descent = height * 0.2F};
    }
  }
  return text::TextRuns{std::move(runs)};
}

auto Text::inherited(const composition::Node& node,
                     composition::TextAttributes attributes)
    -> composition::TextAttributes {
  using composition::TextEnvironment;
  if (const auto found =
          node.override_of(TextEnvironment::multiline_text_alignment)) {
    attributes.multiline_alignment = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::line_limit)) {
    attributes.line_limit = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::truncation_mode)) {
    attributes.truncation = found->get();
  }
  if (const auto found =
          node.override_of(TextEnvironment::minimum_scale_factor)) {
    attributes.minimum_scale_factor = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::line_spacing)) {
    attributes.line_spacing = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::allows_tightening)) {
    attributes.allows_tightening = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::text_case)) {
    attributes.text_case = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::kerning)) {
    attributes.kerning = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::tracking)) {
    attributes.tracking = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::baseline_offset)) {
    attributes.baseline_offset = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::monospaced_digit)) {
    attributes.monospaced_digit = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::italic)) {
    attributes.italic = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::font_weight)) {
    attributes.weight = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::font_design)) {
    attributes.design = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::font_width)) {
    attributes.width = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::text_scale)) {
    attributes.scale = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::underline)) {
    attributes.underline = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::strikethrough)) {
    attributes.strikethrough = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::string_catalog)) {
    attributes.catalog = found->get().value_or(text::StringCatalog{});
  }
  if (const auto found = node.override_of(TextEnvironment::legibility_weight)) {
    attributes.legibility_weight = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::line_height)) {
    attributes.line_height = found->get();
  }
  if (const auto found =
          node.override_of(TextEnvironment::typesetting_language)) {
    attributes.typesetting_language = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::writing_direction)) {
    attributes.writing_direction = found->get();
  }
  if (const auto found =
          node.override_of(TextEnvironment::alignment_strategy)) {
    attributes.alignment_strategy = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::text_variant)) {
    attributes.variant = found->get();
  }
  if (const auto found = node.override_of(Environment::time_zone)) {
    attributes.zone_offset = found->get().offset();
  }
  if (const auto found = node.override_of(TextEnvironment::publishes_layout)) {
    attributes.publishes_layout = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::custom_rendered)) {
    attributes.custom_rendered = found->get();
  }
  if (const auto found = node.override_of(TextEnvironment::selectable)) {
    attributes.selectable = found->get();
  }
  if (const auto found =
          node.override_of(TextEnvironment::selection_affinity)) {
    attributes.selection_affinity = found->get();
  }
  return attributes;
}

auto Text::embolden(theme::Font::Weight weight) noexcept
    -> theme::Font::Weight {
  const auto value = std::to_underlying(weight);
  if (value < 400) {
    return static_cast<theme::Font::Weight>(value + 200);
  }
  if (value < 500) {
    return theme::Font::Weight::semibold;
  }
  if (value < 700) {
    return theme::Font::Weight::bold;
  }
  if (value < 900) {
    return static_cast<theme::Font::Weight>(value + 100);
  }
  return theme::Font::Weight::black;
}

auto Text::fitting(const composition::TextAttributes& attributes, bool marquee)
    -> text::TextFitting {
  text::TextFitting fitting;
  fitting.lines = attributes.line_limit.lines;
  if (marquee) {
    fitting.lines = 1;
  }
  switch (attributes.truncation) {
    case composition::TruncationMode::head:
      fitting.truncation = text::TruncationMode::head;
      break;
    case composition::TruncationMode::middle:
      fitting.truncation = text::TruncationMode::middle;
      break;
    case composition::TruncationMode::tail:
      fitting.truncation = text::TruncationMode::tail;
      break;
  }
  fitting.minimum_scale_factor = attributes.minimum_scale_factor;
  fitting.allows_tightening = attributes.allows_tightening;
  return fitting;
}

auto Text::cased(std::string_view text,
                 const composition::TextAttributes& attributes)
    -> std::optional<std::string> {
  if (!attributes.text_case.has_value()) {
    return std::nullopt;
  }
  return *attributes.text_case == composition::TextCase::uppercase
             ? text::CaseMapping::uppercased(text)
             : text::CaseMapping::lowercased(text);
}

auto Text::cased(std::string_view text,
                 const composition::TextAttributes& attributes,
                 const text::Locale& locale) -> std::optional<std::string> {
  if (!attributes.text_case.has_value()) {
    return std::nullopt;
  }
  return *attributes.text_case == composition::TextCase::uppercase
             ? text::CaseMapping::uppercased(text, locale)
             : text::CaseMapping::lowercased(text, locale);
}

auto Text::Content::embedded(const text::LocalizedStringKey& key)
    -> std::vector<const Content*> {
  std::vector<const Content*> found;
  for (const text::LocalizedStringKey::Argument& argument : key.arguments()) {
    const auto* held =
        std::get_if<text::LocalizedStringKey::Interpolated>(&argument);
    if (held != nullptr && *held != nullptr && (*held)->rich()) {
      found.push_back(&static_cast<const Interpolation&>(**held).content());
    }
  }
  return found;
}

auto Text::Content::live() const -> bool {
  if (const auto* produced = std::get_if<Produced>(&source_)) {
    return produced->producer != nullptr && produced->producer->live();
  }
  if (const auto* joined = std::get_if<Joined>(&source_)) {
    return std::ranges::any_of(joined->pieces,
                               [](const std::shared_ptr<const Content>& piece) {
                                 return piece->live();
                               });
  }
  if (const auto* key = std::get_if<text::LocalizedStringKey>(&source_)) {
    return std::ranges::any_of(
        embedded(*key), [](const Content* piece) { return piece->live(); });
  }
  return false;
}

auto Text::Content::next(Date now) const -> std::optional<Date> {
  std::optional<Date> soonest;
  const auto consider = [&soonest](std::optional<Date> candidate) {
    if (candidate.has_value() &&
        (!soonest.has_value() || *candidate < *soonest)) {
      soonest = candidate;
    }
  };
  if (const auto* produced = std::get_if<Produced>(&source_)) {
    if (produced->producer != nullptr && produced->producer->live()) {
      consider(produced->producer->next(now));
    }
  } else if (const auto* joined = std::get_if<Joined>(&source_)) {
    for (const std::shared_ptr<const Content>& piece : joined->pieces) {
      consider(piece->next(now));
    }
  } else if (const auto* key =
                 std::get_if<text::LocalizedStringKey>(&source_)) {
    for (const Content* piece : embedded(*key)) {
      consider(piece->next(now));
    }
  }
  return soonest;
}

auto Text::Content::refreshes() const noexcept -> bool {
  return std::holds_alternative<Produced>(source_) ||
         std::holds_alternative<Joined>(source_) ||
         std::holds_alternative<text::LocalizedStringKey>(source_) ||
         text_.unresolved();
}

auto Text::Content::refresh(composition::Context& context,
                            const composition::Node&) -> bool {
  bool changed = false;
  if (text_.unresolved()) {
    const std::optional<ImageLibrary>& library =
        context.environment(Environment::image_library);
    AttributedString loaded = text_.resolving(
        [&library](
            std::string_view name) -> std::shared_ptr<const provider::Image> {
          if (!library.has_value()) {
            return nullptr;
          }
          ImageLibrary::Loaded found = library->image(name);
          return found.has_value() ? *found : nullptr;
        });
    changed = !loaded_.has_value() || *loaded_ != loaded;
    loaded_ = std::move(loaded);
    resolution_.reset();
  }
  const bool ticking = live();
  if (ticking) {
    moment_ = TimedWake::now(context);
  }
  if (ticking || std::holds_alternative<Produced>(source_) ||
      std::holds_alternative<text::LocalizedStringKey>(source_)) {
    composition::TextAttributes attributes;
    attributes.catalog =
        context.environment(composition::TextEnvironment::string_catalog)
            .value_or(text::StringCatalog{});
    std::string shown{source_text(attributes,
                                  context.environment(Environment::locale),
                                  moment_.value_or(Date{}))
                          .text()};
    changed = changed || shown_ != shown;
    shown_ = std::move(shown);
  }
  if (!ticking) {
    return changed;
  }
  const Date now = *moment_;
  TimedWake& pending = context.retained(wake);
  if (const std::optional<Date> due = next(now)) {
    pending.schedule(context, std::max(*due, now) + live_margin, now);
  } else {
    pending.cancel();
  }
  return changed;
}

auto Text::Content::plain(const text::Locale& locale) const -> std::string {
  return std::string{source_text(composition::TextAttributes{}, locale,
                                 moment_.value_or(Date{}))
                         .text()};
}

auto Text::Content::interpolated(const text::LocalizedStringKey& key,
                                 const composition::TextAttributes& attributes,
                                 const text::Locale& locale, Date moment) const
    -> AttributedString {
  const std::string pattern = attributes.catalog.pattern(key, locale);
  const std::vector<text::LocalizedStringKey::Argument>& arguments =
      key.arguments();
  AttributedString result;
  for (const text::StringCatalog::Segment& segment :
       text::StringCatalog::segments(pattern, arguments.size())) {
    if (!segment.argument.has_value()) {
      result.append(segment.literal);
      continue;
    }
    const text::LocalizedStringKey::Argument& argument =
        arguments[*segment.argument];
    const auto* held =
        std::get_if<text::LocalizedStringKey::Interpolated>(&argument);
    if (held != nullptr && *held != nullptr && (*held)->rich()) {
      const Content& piece =
          static_cast<const Interpolation&>(**held).content();
      result.append(piece.source_text(attributes, locale, moment)
                        .applying(piece.level()));
      continue;
    }
    result.append(text::StringCatalog::argument_text(argument, locale));
  }
  return result;
}

auto Text::Content::source_text(const composition::TextAttributes& attributes,
                                const text::Locale& locale, Date moment) const
    -> AttributedString {
  if (const auto* key = std::get_if<text::LocalizedStringKey>(&source_)) {
    return interpolated(*key, attributes, locale, moment);
  }
  if (const auto* formatted = std::get_if<Formatted>(&source_)) {
    if (const auto* whole = std::get_if<std::int64_t>(&formatted->value)) {
      return AttributedString{formatted->style.format(*whole, locale)};
    }
    return AttributedString{
        formatted->style.format(std::get<double>(formatted->value), locale)};
  }
  if (const auto* dated = std::get_if<Dated>(&source_)) {
    return AttributedString{
        dated->style.format(dated->instant, attributes.zone_offset, locale)};
  }
  if (const auto* relative = std::get_if<Relative>(&source_)) {
    return AttributedString{
        text::RelativeDateTimeStyle::format(relative->offset, locale)};
  }
  if (const auto* produced = std::get_if<Produced>(&source_)) {
    if (produced->producer == nullptr) {
      return AttributedString{};
    }
    return AttributedString{produced->producer->produce(
        Producer::Moment{.now = moment,
                         .locale = std::cref(locale),
                         .zone = attributes.zone_offset})};
  }
  if (const auto* joined = std::get_if<Joined>(&source_)) {
    AttributedString result;
    for (const std::shared_ptr<const Content>& piece : joined->pieces) {
      result.append(piece->source_text(attributes, locale, moment)
                        .applying(piece->level()));
    }
    return result;
  }
  return loaded_.has_value() ? *loaded_ : text_;
}

auto Text::Content::resolved(const composition::TextAttributes& attributes,
                             const text::Locale& locale) const
    -> const AttributedString& {
  if (!localized() && !attributes.text_case.has_value()) {
    return loaded_.has_value() ? *loaded_ : text_;
  }
  if (resolution_.has_value() && resolution_->locale == locale.tag() &&
      resolution_->catalog == attributes.catalog &&
      resolution_->zone == attributes.zone_offset &&
      resolution_->text_case == attributes.text_case &&
      resolution_->moment == moment_) {
    return resolution_->text;
  }
  AttributedString shown =
      source_text(attributes, locale, moment_.value_or(Date{}));
  if (attributes.text_case.has_value()) {
    shown = shown.cased(*attributes.text_case, locale);
  }
  resolution_ = Resolution{.locale = std::string{locale.tag()},
                           .catalog = attributes.catalog,
                           .zone = attributes.zone_offset,
                           .text_case = attributes.text_case,
                           .moment = moment_,
                           .text = std::move(shown)};
  return resolution_->text;
}

auto Text::Content::variants(const composition::TextAttributes& attributes,
                             const text::Locale& locale) const
    -> std::vector<std::string> {
  if (attributes.variant !=
      composition::TextVariantPreference::size_dependent) {
    return {};
  }
  using Style = text::DateFormatStyle;
  if (const auto* dated = std::get_if<Dated>(&source_)) {
    std::vector<std::string> found;
    const Style::TimeStyle time = dated->style.time();
    for (const Style::DateStyle date :
         {Style::DateStyle::complete, Style::DateStyle::long_form,
          Style::DateStyle::abbreviated, Style::DateStyle::numeric}) {
      if (dated->style.date() == Style::DateStyle::omitted) {
        break;
      }
      std::string shown = Style{date, time}.format(
          dated->instant, attributes.zone_offset, locale);
      if (std::ranges::find(found, shown) == found.end()) {
        found.push_back(std::move(shown));
      }
    }
    return found;
  }
  if (const auto* produced = std::get_if<Produced>(&source_)) {
    if (produced->producer != nullptr) {
      return produced->producer->variants(
          Producer::Moment{.now = moment_.value_or(Date{}),
                           .locale = std::cref(locale),
                           .zone = attributes.zone_offset});
    }
  }
  return {};
}

auto Text::Content::attributes_in(const composition::TextAttributes& inherited)
    const -> composition::TextAttributes {
  composition::TextAttributes attributes = inherited;
  if (line_limit_.has_value()) {
    attributes.line_limit.lines = line_limit_;
  }
  attributes.kerning = kerning_.value_or(attributes.kerning);
  attributes.drawing = drawing_.value_or(attributes.drawing);
  attributes.tracking = tracking_.value_or(attributes.tracking);
  attributes.baseline_offset =
      baseline_offset_.value_or(attributes.baseline_offset);
  attributes.monospaced_digit =
      monospaced_digit_.value_or(attributes.monospaced_digit);
  attributes.italic = italic_.value_or(attributes.italic);
  if (design_.has_value()) {
    attributes.design = design_;
  }
  if (width_.has_value()) {
    attributes.width = width_;
  }
  attributes.scale = scale_.value_or(attributes.scale);
  attributes.underline = underline_.value_or(attributes.underline);
  attributes.strikethrough = strikethrough_.value_or(attributes.strikethrough);
  if (language_.has_value()) {
    attributes.typesetting_language = language_;
  }
  attributes.variant = variant_.value_or(attributes.variant);
  return attributes;
}

auto Text::Content::cased(const composition::TextAttributes& attributes) const
    -> std::optional<AttributedString> {
  if (!attributes.text_case.has_value()) {
    return std::nullopt;
  }
  return text_.cased(*attributes.text_case);
}

void Text::Content::set_text(AttributedString text) {
  text_ = std::move(text);
  link_count_ = text_.links().size();
  resolution_.reset();
}

void Text::Content::set_source(Source source) {
  source_ = std::move(source);
  resolution_.reset();
  if (const auto* key = std::get_if<text::LocalizedStringKey>(&source_)) {
    text_ = AttributedString{std::string{key->key()}};
  }
}

auto Text::Content::level() const -> AttributedString::Attributes {
  AttributedString::Attributes attributes;
  if (weight_.has_value() && weight_ != theme::Font::Weight::regular) {
    attributes.weight = weight_;
  }
  attributes.italic = italic_.value_or(false);
  attributes.design = design_;
  attributes.foreground = foreground_;
  attributes.font = font_;
  attributes.shading = style_;
  if (underline_.has_value() && underline_->active) {
    if (underline_->pattern != text::LinePattern::solid ||
        underline_->colour.has_value()) {
      attributes.underline_style = underline_;
    } else {
      attributes.underline = true;
    }
  }
  if (strikethrough_.has_value() && strikethrough_->active) {
    if (strikethrough_->pattern != text::LinePattern::solid ||
        strikethrough_->colour.has_value()) {
      attributes.strikethrough_style = strikethrough_;
    } else {
      attributes.strikethrough = true;
    }
  }
  if (kerning_.has_value()) {
    attributes.kern = kerning_;
  }
  if (tracking_.has_value()) {
    attributes.tracking = tracking_;
  }
  if (baseline_offset_.has_value()) {
    attributes.baseline_offset = baseline_offset_;
  }
  attributes.custom = custom_;
  return attributes;
}

void Text::Content::clear_level() noexcept {
  weight_.reset();
  italic_.reset();
  design_.reset();
  foreground_.reset();
  font_.reset();
  style_.reset();
  underline_.reset();
  strikethrough_.reset();
  kerning_.reset();
  tracking_.reset();
  baseline_offset_.reset();
  custom_ = AttributedString::Custom{};
}

auto Text::Content::accessibility() const
    -> composition::AccessibilityDefaults {
  using composition::AccessibilityTrait;
  composition::AccessibilityTraits traits{AccessibilityTrait::is_static_text};
  if (link_count_ > 0) {
    traits = traits.with(AccessibilityTrait::is_link);
  }
  const std::string_view shown =
      resolution_.has_value() ? resolution_->text.text() : text_.text();
  std::string label{shown};
  if (shown_.has_value() && !resolution_.has_value()) {
    label = *shown_;
  }
  return composition::AccessibilityDefaults{
      .label = std::move(label), .value = std::nullopt, .traits = traits};
}

auto Text::Content::localized_accessibility(
    const text::Locale& locale, const text::StringCatalog& catalog) const
    -> composition::AccessibilityDefaults {
  composition::AccessibilityDefaults defaults = accessibility();
  if (localized() && !shown_.has_value()) {
    composition::TextAttributes attributes;
    attributes.catalog = catalog;
    defaults.label = std::string{
        source_text(attributes, locale, moment_.value_or(Date{})).text()};
  }
  defaults.speech = speech();
  return defaults;
}

auto Text::Content::speech() const
    -> std::optional<composition::AccessibilitySpeech> {
  if (speech_.has_value()) {
    return speech_;
  }
  const std::span<const AttributedString::Speech> runs = text_.speech();
  if (runs.empty()) {
    return std::nullopt;
  }
  const AttributedString::Speech& first = runs.front();
  return composition::AccessibilitySpeech{
      .spells_out_characters = first.spells_out_characters,
      .always_includes_punctuation = first.includes_punctuation,
      .announcements_queued = std::nullopt,
      .adjusted_pitch = first.adjusted_pitch};
}

auto Text::Content::activatable() const noexcept -> bool {
  return link_count_ > 0;
}

auto Text::Content::link_at(const composition::Node& node,
                            composition::Point at) noexcept
    -> std::optional<std::size_t> {
  constexpr float reach = 8.0F;
  std::optional<std::size_t> best;
  float nearest = std::numeric_limits<float>::max();
  for (const composition::LinkFrame& found : node.link_frames()) {
    const composition::Rect& frame = found.frame;
    const float dx =
        std::max({frame.x - at.x, 0.0F, at.x - (frame.x + frame.width)});
    const float dy =
        std::max({frame.y - at.y, 0.0F, at.y - (frame.y + frame.height)});
    const float distance = std::hypot(dx, dy);
    if (distance <= reach && distance < nearest) {
      nearest = distance;
      best = found.link;
    }
  }
  return best;
}

auto Text::Content::activate(composition::Context& context,
                             const composition::Node& node,
                             std::optional<composition::Point> at) -> bool {
  if (link_count_ == 0) {
    return false;
  }
  if (at.has_value()) {
    const std::optional<std::size_t> touched = link_at(node, *at);
    if (!touched.has_value()) {
      return false;
    }
    context.select_link(*touched);
  }
  const std::vector<AttributedString::Link> links = text_.links();
  const std::size_t chosen =
      at.has_value() ? *link_at(node, *at) : node.selected_link();
  if (chosen >= links.size()) {
    return false;
  }
  const OpenURLAction& open = context.environment(Environment::open_url);
  return open(links[chosen].destination);
}

auto Text::Content::steer(composition::Context& context,
                          const composition::Node& node,
                          composition::MoveCommandDirection direction) -> bool {
  if (link_count_ < 2) {
    return false;
  }
  const bool mirrored = context.environment(Environment::layout_direction) ==
                        composition::LayoutDirection::right_to_left;
  const std::size_t selected = std::min(node.selected_link(), link_count_ - 1);
  const auto forward = mirrored ? composition::MoveCommandDirection::left
                                : composition::MoveCommandDirection::right;
  const auto backward = mirrored ? composition::MoveCommandDirection::right
                                 : composition::MoveCommandDirection::left;
  if (direction == forward && selected + 1 < link_count_) {
    context.select_link(selected + 1);
    return true;
  }
  if (direction == backward && selected > 0) {
    context.select_link(selected - 1);
    return true;
  }
  return false;
}

void Text::Content::enter(composition::Context& context,
                          const composition::Node&,
                          composition::MoveCommandDirection direction) {
  if (link_count_ < 2) {
    return;
  }
  const bool mirrored = context.environment(Environment::layout_direction) ==
                        composition::LayoutDirection::right_to_left;
  const auto trailing = mirrored ? composition::MoveCommandDirection::right
                                 : composition::MoveCommandDirection::left;
  context.select_link(direction == trailing ? link_count_ - 1 : 0);
}

auto Text::Content::equals(const Content& theirs) const -> bool {
  return text_ == theirs.text_ && source_ == theirs.source_ &&
         shown_ == theirs.shown_ && loaded_ == theirs.loaded_ &&
         font_ == theirs.font_ && foreground_ == theirs.foreground_ &&
         style_ == theirs.style_ && italic_ == theirs.italic_ &&
         underline_ == theirs.underline_ &&
         strikethrough_ == theirs.strikethrough_ && design_ == theirs.design_ &&
         width_ == theirs.width_ && scale_ == theirs.scale_ &&
         language_ == theirs.language_ && variant_ == theirs.variant_ &&
         weight_ == theirs.weight_ && line_limit_ == theirs.line_limit_ &&
         kerning_ == theirs.kerning_ && drawing_ == theirs.drawing_ &&
         tracking_ == theirs.tracking_ &&
         baseline_offset_ == theirs.baseline_offset_ &&
         monospaced_digit_ == theirs.monospaced_digit_ &&
         marquee_ == theirs.marquee_ && custom_ == theirs.custom_ &&
         speech_ == theirs.speech_;
}

auto Text::Content::paints_like(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  return equals(static_cast<const Content&>(other));
}

auto Text::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

}
