#include <arisa/components/attributed_string.hpp>

#include <arisa/components/attributed_text_selection.hpp>
#include <arisa/components/text.hpp>
#include <arisa/text/case_mapping.hpp>
#include <components/inline_markdown.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace arisa::components {

auto AttributedString::interpolation() const
    -> text::LocalizedStringKey::Argument {
  return Text{*this}.interpolation();
}

auto AttributedString::InlineImage::width() const noexcept -> float {
  if (pixels == nullptr || scale <= 0.0F) {
    return 0.0F;
  }
  return static_cast<float>(pixels->width()) / scale;
}

auto AttributedString::InlineImage::height() const noexcept -> float {
  if (pixels == nullptr || scale <= 0.0F) {
    return 0.0F;
  }
  return static_cast<float>(pixels->height()) / scale;
}

AttributedString::AttributedString(std::string text)
    : AttributedString(std::move(text), Attributes{}) {}

AttributedString::AttributedString(std::string text,
                                   const Attributes& attributes)
    : text_(std::move(text)) {
  if (text_.size() >= std::numeric_limits<std::uint32_t>::max()) {
    text_.clear();
  }
  if (auto run =
          run_for(attributes, 0, static_cast<std::uint32_t>(text_.size()))) {
    pieces_.push_back(*run);
  }
  freeze();
}

auto AttributedString::markdown(std::string_view source) -> AttributedString {
  return InlineMarkdown{source}.parsed();
}

auto AttributedString::image(InlineImage image) -> AttributedString {
  return AttributedString::image(std::move(image), Attributes{});
}

auto AttributedString::glyph(AdaptiveImageGlyph glyph) -> AttributedString {
  return AttributedString::glyph(std::move(glyph), Attributes{});
}

auto AttributedString::image(InlineImage image, const Attributes& attributes)
    -> AttributedString {
  Attributes held = attributes;
  held.image = std::move(image);
  return AttributedString{std::string{replacement}, held};
}

auto AttributedString::glyph(AdaptiveImageGlyph glyph,
                             const Attributes& attributes) -> AttributedString {
  Attributes held = attributes;
  held.adaptive_image_glyph = std::move(glyph);
  return AttributedString{std::string{replacement}, held};
}

auto AttributedString::typeface(theme::Font::Design design) noexcept
    -> text::Typeface {
  switch (design) {
    case theme::Font::Design::standard:
      return text::Typeface::standard;
    case theme::Font::Design::rounded:
      return text::Typeface::rounded;
    case theme::Font::Design::serif:
      return text::Typeface::serif;
    case theme::Font::Design::monospaced:
      return text::Typeface::monospaced;
  }
  return text::Typeface::standard;
}

template <class Value>
auto AttributedString::index_of(std::vector<Value>& table, const Value& value)
    -> std::uint16_t {
  const auto found = std::ranges::find(table, value);
  if (found == table.end()) {
    table.push_back(value);
    return static_cast<std::uint16_t>(table.size());
  }
  return static_cast<std::uint16_t>(found - table.begin() + 1);
}

auto AttributedString::run_for(const Attributes& attributes,
                               std::uint32_t start, std::uint32_t end)
    -> std::optional<text::TextRun> {
  if (attributes == Attributes{} || end <= start) {
    return std::nullopt;
  }
  text::TextRun run{.start = start,
                    .end = end,
                    .weight = std::nullopt,
                    .italic = attributes.italic,
                    .typeface = std::nullopt,
                    .underline = attributes.underline,
                    .strikethrough = attributes.strikethrough,
                    .ink = 0,
                    .link = 0};
  if (attributes.weight.has_value()) {
    run.weight =
        text::FontWeight::create(std::to_underlying(*attributes.weight));
  }
  if (attributes.design.has_value()) {
    run.typeface = typeface(*attributes.design);
  }
  if (attributes.foreground.has_value()) {
    run.ink = index_of(inks_, *attributes.foreground);
  }
  if (attributes.link.has_value()) {
    run.link = index_of(destinations_, *attributes.link);
  }
  if (attributes.font.has_value()) {
    run.font = index_of(fonts_, *attributes.font);
  }
  run.kerning = attributes.kern;
  run.tracking = attributes.tracking;
  run.baseline_offset = attributes.baseline_offset;
  if (attributes.background.has_value()) {
    run.background = index_of(inks_, *attributes.background);
  }
  if (attributes.underline_style.has_value()) {
    const composition::TextDecoration& style = *attributes.underline_style;
    run.underline = run.underline || style.active;
    run.underline_pattern = style.pattern;
    if (style.colour.has_value()) {
      run.underline_ink = index_of(inks_, *style.colour);
    }
  }
  if (attributes.strikethrough_style.has_value()) {
    const composition::TextDecoration& style = *attributes.strikethrough_style;
    run.strikethrough = run.strikethrough || style.active;
    run.strikethrough_pattern = style.pattern;
    if (style.colour.has_value()) {
      run.strikethrough_ink = index_of(inks_, *style.colour);
    }
  }
  run.line_height = attributes.line_height;
  if (attributes.alignment.has_value()) {
    switch (*attributes.alignment) {
      case composition::TextAlignment::leading:
        run.alignment = text::TextAlignment::natural;
        break;
      case composition::TextAlignment::center:
        run.alignment = text::TextAlignment::center;
        break;
      case composition::TextAlignment::trailing:
        run.alignment = text::TextAlignment::opposite;
        break;
    }
  }
  if (attributes.accessibility.has_value()) {
    run.extra = index_of(speech_, *attributes.accessibility);
  }
  if (attributes.image.has_value()) {
    run.image = index_of(images_, *attributes.image);
    run.attachment = text::TextAttachment{.width = attributes.image->width(),
                                          .height = attributes.image->height(),
                                          .descent = 0.0F};
  }
  if (attributes.adaptive_image_glyph.has_value()) {
    run.glyph = index_of(glyphs_, *attributes.adaptive_image_glyph);
  }
  if (attributes.shading.has_value()) {
    run.shade = index_of(shadings_, *attributes.shading);
  }
  if (!attributes.custom.empty()) {
    run.custom = index_of(customs_, attributes.custom);
  }
  return run;
}

void AttributedString::push(text::TextRun run) {
  if (!pieces_.empty()) {
    text::TextRun& last = pieces_.back();
    text::TextRun widened = last;
    widened.end = run.end;
    text::TextRun compared = run;
    compared.start = last.start;
    if (last.end == run.start && widened == compared &&
        !run.attachment.has_value() && run.glyph == 0) {
      last.end = run.end;
      return;
    }
  }
  pieces_.push_back(run);
}

void AttributedString::freeze() { runs_ = text::TextRuns{pieces_}; }

auto AttributedString::append(std::string_view text) -> AttributedString& {
  return append(text, Attributes{});
}

auto AttributedString::append(std::string_view text,
                              const Attributes& attributes)
    -> AttributedString& {
  if (text.empty() ||
      text_.size() + text.size() >= std::numeric_limits<std::uint32_t>::max()) {
    return *this;
  }
  const auto start = static_cast<std::uint32_t>(text_.size());
  text_.append(text);
  if (auto run = run_for(attributes, start,
                         static_cast<std::uint32_t>(text_.size()))) {
    push(*run);
  }
  freeze();
  return *this;
}

auto AttributedString::append(const AttributedString& other)
    -> AttributedString& {
  if (text_.size() + other.text_.size() >=
      std::numeric_limits<std::uint32_t>::max()) {
    return *this;
  }
  const auto offset = static_cast<std::uint32_t>(text_.size());
  text_.append(other.text_);
  for (const text::TextRun& run : other.pieces_) {
    if (auto moved = run_for(other.attributes_of(run), run.start + offset,
                             run.end + offset)) {
      push(*moved);
    }
  }
  freeze();
  return *this;
}

auto AttributedString::merged(const Attributes& base, const Attributes& over)
    -> Attributes {
  Attributes result = over;
  const auto keep = [](auto& field, const auto& fallback) {
    if (!field.has_value()) {
      field = fallback;
    }
  };
  keep(result.weight, base.weight);
  result.italic = result.italic || base.italic;
  keep(result.design, base.design);
  keep(result.foreground, base.foreground);
  result.underline = result.underline || base.underline;
  result.strikethrough = result.strikethrough || base.strikethrough;
  keep(result.link, base.link);
  keep(result.font, base.font);
  keep(result.kern, base.kern);
  keep(result.tracking, base.tracking);
  keep(result.baseline_offset, base.baseline_offset);
  keep(result.background, base.background);
  keep(result.underline_style, base.underline_style);
  keep(result.strikethrough_style, base.strikethrough_style);
  keep(result.line_height, base.line_height);
  keep(result.alignment, base.alignment);
  keep(result.accessibility, base.accessibility);
  keep(result.shading, base.shading);
  result.custom = Custom::merged(base.custom, over.custom);
  return result;
}

auto AttributedString::Custom::merged(const Custom& base, const Custom& over)
    -> Custom {
  Custom result = over;
  for (const auto& held : base.values_) {
    const bool present = std::ranges::any_of(
        result.values_,
        [&held](const auto& kept) { return kept->kind() == held->kind(); });
    if (!present) {
      result.values_.push_back(held);
    }
  }
  return result;
}

auto operator==(const AttributedString::Custom& left,
                const AttributedString::Custom& right) -> bool {
  return std::ranges::equal(
      left.values_, right.values_,
      [](const auto& one, const auto& other) { return one->equals(*other); });
}

auto AttributedString::substring(std::uint32_t start, std::uint32_t end) const
    -> AttributedString {
  const auto size = static_cast<std::uint32_t>(text_.size());
  start = std::min(start, size);
  end = std::clamp(end, start, size);
  std::vector<std::uint32_t> cuts = boundaries();
  cuts.push_back(start);
  cuts.push_back(end);
  std::ranges::sort(cuts);
  const auto [first, last] = std::ranges::unique(cuts);
  cuts.erase(first, last);
  AttributedString result;
  for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
    if (cuts[index] < start || cuts[index + 1] > end) {
      continue;
    }
    result.append(std::string_view{text_}.substr(cuts[index],
                                                 cuts[index + 1] - cuts[index]),
                  attributes(cuts[index]));
  }
  return result;
}

auto AttributedString::replacing(std::uint32_t start, std::uint32_t end,
                                 const AttributedString& inserted) const
    -> AttributedString {
  const auto size = static_cast<std::uint32_t>(text_.size());
  start = std::min(start, size);
  end = std::clamp(end, start, size);
  AttributedString result = substring(0, start);
  result.append(inserted);
  result.append(substring(end, size));
  return result;
}

auto AttributedString::markdown_source() const -> std::string {
  std::string source;
  const std::vector<std::uint32_t> cuts = boundaries();
  for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
    const Attributes held = attributes(cuts[index]);
    std::string part;
    for (const char byte : std::string_view{text_}.substr(
             cuts[index], cuts[index + 1] - cuts[index])) {
      if (byte == '*' || byte == '_' || byte == '~' || byte == '[' ||
          byte == ']' || byte == '`' || byte == '\\') {
        part.push_back('\\');
      }
      part.push_back(byte);
    }
    const bool bold = held.weight.has_value() &&
                      *held.weight >= theme::Font::Weight::semibold;
    std::string open;
    std::string close;
    if (held.strikethrough) {
      open += "~~";
      close.insert(0, "~~");
    }
    if (bold) {
      open += "**";
      close.insert(0, "**");
    }
    if (held.italic) {
      open += "*";
      close.insert(0, "*");
    }
    part = open + part + close;
    if (held.link.has_value()) {
      part = "[" + part + "](" + *held.link + ")";
    }
    source += part;
  }
  return source;
}

auto AttributedString::transforming(
    std::uint32_t start, std::uint32_t end,
    const std::function<void(Attributes&)>& change) const -> AttributedString {
  return transformed(start, end, change);
}

void AttributedString::replace_selection(AttributedTextSelection& selection,
                                         const AttributedString& inserted) {
  const AttributedTextSelection::Range range = selection.indices(*this).range();
  *this = replacing(static_cast<std::uint32_t>(range.lower),
                    static_cast<std::uint32_t>(range.upper), inserted);
  selection = AttributedTextSelection{
      AttributedTextSelection::InsertionPoint{.index = range.lower +
                                                       inserted.text().size()},
      std::nullopt};
}

void AttributedString::replace_selection(AttributedTextSelection& selection,
                                         std::string_view characters) {
  const Attributes typing = selection.typing_attributes(*this);
  replace_selection(selection,
                    AttributedString{std::string{characters}, typing});
  selection = selection.with_typing_attributes(typing);
}

void AttributedString::transform_attributes(
    AttributedTextSelection& selection,
    const std::function<void(Attributes&)>& body) {
  const AttributedTextSelection::Indices indices = selection.indices(*this);
  if (indices.is_insertion_point()) {
    Attributes typing = selection.typing_attributes(*this);
    body(typing);
    selection = selection.with_typing_attributes(std::move(typing));
    return;
  }
  AttributedString result = *this;
  for (const AttributedTextSelection::Range& range : indices.ranges()) {
    result = result.transformed(static_cast<std::uint32_t>(range.lower),
                                static_cast<std::uint32_t>(range.upper), body);
  }
  *this = std::move(result);
}

void AttributedString::transform(
    AttributedTextSelection& selection,
    const std::function<void(AttributedString&)>& body) {
  const std::string before = text_;
  const AttributedTextSelection::Indices indices = selection.indices(*this);
  body(*this);
  const std::string_view after = text_;
  std::size_t prefix = 0;
  while (prefix < before.size() && prefix < after.size() &&
         before[prefix] == after[prefix]) {
    ++prefix;
  }
  std::size_t suffix = 0;
  while (suffix < before.size() - prefix && suffix < after.size() - prefix &&
         before[before.size() - 1 - suffix] ==
             after[after.size() - 1 - suffix]) {
    ++suffix;
  }
  const auto moved = [&](std::size_t index) {
    if (index <= prefix) {
      return index;
    }
    if (index >= before.size() - suffix) {
      return index - before.size() + after.size();
    }
    return after.size() - suffix;
  };
  if (indices.is_insertion_point()) {
    selection =
        AttributedTextSelection{AttributedTextSelection::InsertionPoint{
                                    .index = moved(indices.insertion())},
                                std::nullopt};
    return;
  }
  std::vector<AttributedTextSelection::Range> ranges;
  for (const AttributedTextSelection::Range& range : indices.ranges()) {
    ranges.push_back(AttributedTextSelection::Range{
        .lower = moved(range.lower), .upper = moved(range.upper)});
  }
  selection = AttributedTextSelection{std::move(ranges)};
}

auto AttributedString::boundaries() const -> std::vector<std::uint32_t> {
  std::vector<std::uint32_t> cuts{0, static_cast<std::uint32_t>(text_.size())};
  for (const text::TextRun& run : pieces_) {
    cuts.push_back(run.start);
    cuts.push_back(run.end);
  }
  std::ranges::sort(cuts);
  const auto [first, last] = std::ranges::unique(cuts);
  cuts.erase(first, last);
  return cuts;
}

auto AttributedString::attributes_of(const text::TextRun& run) const
    -> Attributes {
  Attributes found;
  if (run.weight.has_value()) {
    found.weight = static_cast<theme::Font::Weight>(run.weight->value());
  }
  found.italic = run.italic;
  if (run.typeface.has_value()) {
    found.design = static_cast<theme::Font::Design>(*run.typeface);
  }
  if (run.ink > 0 && run.ink <= inks_.size()) {
    found.foreground = inks_[run.ink - 1];
  }
  found.underline = run.underline;
  found.strikethrough = run.strikethrough;
  if (run.link > 0 && run.link <= destinations_.size()) {
    found.link = destinations_[run.link - 1];
  }
  if (run.font > 0 && run.font <= fonts_.size()) {
    found.font = fonts_[run.font - 1];
  }
  found.kern = run.kerning;
  found.tracking = run.tracking;
  found.baseline_offset = run.baseline_offset;
  if (run.background > 0 && run.background <= inks_.size()) {
    found.background = inks_[run.background - 1];
  }
  if (run.underline_pattern != text::LinePattern::solid ||
      run.underline_ink != 0) {
    found.underline_style = composition::TextDecoration{
        .active = run.underline,
        .colour = run.underline_ink > 0 && run.underline_ink <= inks_.size()
                      ? std::optional{inks_[run.underline_ink - 1]}
                      : std::nullopt,
        .pattern = run.underline_pattern};
    found.underline = false;
  }
  if (run.strikethrough_pattern != text::LinePattern::solid ||
      run.strikethrough_ink != 0) {
    found.strikethrough_style = composition::TextDecoration{
        .active = run.strikethrough,
        .colour =
            run.strikethrough_ink > 0 && run.strikethrough_ink <= inks_.size()
                ? std::optional{inks_[run.strikethrough_ink - 1]}
                : std::nullopt,
        .pattern = run.strikethrough_pattern};
    found.strikethrough = false;
  }
  found.line_height = run.line_height;
  if (run.alignment.has_value()) {
    switch (*run.alignment) {
      case text::TextAlignment::center:
        found.alignment = composition::TextAlignment::center;
        break;
      case text::TextAlignment::opposite:
      case text::TextAlignment::right:
        found.alignment = composition::TextAlignment::trailing;
        break;
      case text::TextAlignment::natural:
      case text::TextAlignment::left:
        found.alignment = composition::TextAlignment::leading;
        break;
    }
  }
  if (run.extra > 0 && run.extra <= speech_.size()) {
    found.accessibility = speech_[run.extra - 1];
  }
  if (run.glyph > 0 && run.glyph <= glyphs_.size()) {
    found.adaptive_image_glyph = glyphs_[run.glyph - 1];
  }
  if (run.image > 0 && run.image <= images_.size()) {
    found.image = images_[run.image - 1];
  }
  if (run.shade > 0 && run.shade <= shadings_.size()) {
    found.shading = shadings_[run.shade - 1];
  }
  if (run.custom > 0 && run.custom <= customs_.size()) {
    found.custom = customs_[run.custom - 1];
  }
  return found;
}

auto AttributedString::attributes(std::uint32_t offset) const -> Attributes {
  const auto run = runs_.find(offset);
  return run.has_value() ? attributes_of(run->get()) : Attributes{};
}

auto AttributedString::applying(const Attributes& attributes) const
    -> AttributedString {
  if (attributes == Attributes{}) {
    return *this;
  }
  return applying(attributes, 0, static_cast<std::uint32_t>(text_.size()));
}

auto AttributedString::applying(const Attributes& attributes,
                                std::uint32_t start, std::uint32_t end) const
    -> AttributedString {
  return transformed(start, end, [&attributes](Attributes& held) {
    held = merged(attributes, held);
  });
}

auto AttributedString::transformed(
    std::uint32_t start, std::uint32_t end,
    const std::function<void(Attributes&)>& change) const -> AttributedString {
  const auto size = static_cast<std::uint32_t>(text_.size());
  start = std::min(start, size);
  end = std::clamp(end, start, size);
  std::vector<std::uint32_t> cuts = boundaries();
  cuts.push_back(start);
  cuts.push_back(end);
  std::ranges::sort(cuts);
  const auto [first, last] = std::ranges::unique(cuts);
  cuts.erase(first, last);
  AttributedString result;
  for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
    Attributes held = this->attributes(cuts[index]);
    if (cuts[index] >= start && cuts[index + 1] <= end) {
      change(held);
    }
    result.append(std::string_view{text_}.substr(cuts[index],
                                                 cuts[index + 1] - cuts[index]),
                  held);
  }
  return result;
}

auto AttributedString::cased(composition::TextCase text_case) const
    -> AttributedString {
  AttributedString result;
  const std::vector<std::uint32_t> cuts = boundaries();
  for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
    const std::string_view part = std::string_view{text_}.substr(
        cuts[index], cuts[index + 1] - cuts[index]);
    result.append(text_case == composition::TextCase::uppercase
                      ? text::CaseMapping::uppercased(part)
                      : text::CaseMapping::lowercased(part),
                  attributes(cuts[index]));
  }
  return result;
}

auto AttributedString::cased(composition::TextCase text_case,
                             const text::Locale& locale) const
    -> AttributedString {
  AttributedString result;
  const std::vector<std::uint32_t> cuts = boundaries();
  for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
    const std::string_view part = std::string_view{text_}.substr(
        cuts[index], cuts[index + 1] - cuts[index]);
    result.append(text_case == composition::TextCase::uppercase
                      ? text::CaseMapping::uppercased(part, locale)
                      : text::CaseMapping::lowercased(part, locale),
                  attributes(cuts[index]));
  }
  return result;
}

auto AttributedString::unresolved() const noexcept -> bool {
  return std::ranges::any_of(images_, [](const InlineImage& image) {
    return image.pixels == nullptr && !image.name.empty();
  });
}

auto AttributedString::resolving(
    const std::function<std::shared_ptr<const provider::Image>(
        std::string_view)>& load) const -> AttributedString {
  if (!unresolved()) {
    return *this;
  }
  return transformed(
      0, static_cast<std::uint32_t>(text_.size()), [&load](Attributes& held) {
        if (held.image.has_value() && held.image->pixels == nullptr &&
            !held.image->name.empty()) {
          held.image->pixels = load(held.image->name);
        }
      });
}

auto AttributedString::links() const -> std::vector<Link> {
  std::vector<Link> found;
  for (const text::TextRun& run : pieces_) {
    if (run.link == 0 || run.link > destinations_.size()) {
      continue;
    }
    if (!found.empty() && found.back().index == run.link - 1U &&
        found.back().end == run.start) {
      found.back().end = run.end;
      continue;
    }
    found.push_back(Link{.index = run.link - 1U,
                         .start = run.start,
                         .end = run.end,
                         .destination = destinations_[run.link - 1]});
  }
  return found;
}

auto operator+(AttributedString left, const AttributedString& right)
    -> AttributedString {
  left.append(right);
  return left;
}

auto operator==(const AttributedString& left, const AttributedString& right)
    -> bool {
  return left.text_ == right.text_ && left.pieces_ == right.pieces_ &&
         left.inks_ == right.inks_ &&
         left.destinations_ == right.destinations_ &&
         left.fonts_ == right.fonts_ && left.images_ == right.images_ &&
         left.glyphs_ == right.glyphs_ && left.speech_ == right.speech_ &&
         left.shadings_ == right.shadings_ && left.customs_ == right.customs_;
}

}
