#include <arisa/components/text_layout.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace arisa::components {

auto Text::Layout::is_truncated() const noexcept -> bool {
  return source_ != nullptr && source_->truncated;
}

auto Text::Layout::end_index() const noexcept -> std::size_t {
  if (source_ == nullptr || source_->layout == nullptr) {
    return 0;
  }
  return source_->layout->lines().size();
}

auto Text::Layout::operator[](std::size_t index) const -> Line {
  return Line{source_, index};
}

auto Text::Layout::begin() const -> Cursor<Layout, Line> {
  return Cursor<Layout, Line>{this, 0};
}

auto Text::Layout::end() const -> Cursor<Layout, Line> {
  return Cursor<Layout, Line>{this, end_index()};
}

auto Text::Layout::text() const noexcept -> std::string_view {
  return source_ == nullptr ? std::string_view{}
                            : std::string_view{source_->text};
}

auto Text::Layout::extent() const noexcept -> composition::Size {
  if (source_ == nullptr || source_->layout == nullptr) {
    return composition::Size{};
  }
  const text::TextSize size = source_->layout->size();
  return composition::Size{.width = size.width, .height = size.height};
}

auto Text::Layout::line_of(CharacterIndex index) const -> std::size_t {
  if (empty()) {
    return 0;
  }
  const std::span<const text::TextLine> lines = source_->layout->lines();
  for (std::size_t at = 0; at < lines.size(); ++at) {
    const text::TextLine& line = lines[at];
    if (index.value() < line.text_end ||
        (at + 1 < lines.size() && lines[at + 1].text_start > index.value())) {
      return at;
    }
  }
  return lines.size() - 1;
}

auto Text::Layout::boundary(const text::TextLine& line,
                            std::size_t offset) const -> float {
  const text::TextLayout& laid = *source_->layout;
  float best = line.x + line.width;
  bool found = false;
  for (const text::GlyphRun& run :
       laid.runs().subspan(line.first_run, line.run_count)) {
    for (const text::PositionedGlyph& glyph : laid.glyphs(run)) {
      if (run.right_to_left) {
        if (glyph.cluster < offset && (!found || glyph.x < best)) {
          best = glyph.x;
          found = true;
        }
        continue;
      }
      if (glyph.cluster >= offset && (!found || glyph.x < best)) {
        best = glyph.x;
        found = true;
      }
    }
  }
  return best;
}

auto Text::Layout::character_at(composition::Point point) const
    -> CharacterIndex {
  if (empty()) {
    return CharacterIndex{};
  }
  const text::TextLayout& laid = *source_->layout;
  const std::span<const text::TextLine> lines = laid.lines();
  std::size_t chosen = lines.size() - 1;
  for (std::size_t at = 0; at < lines.size(); ++at) {
    if (point.y < lines[at].top + lines[at].height) {
      chosen = at;
      break;
    }
  }
  const text::TextLine& line = lines[chosen];
  std::size_t end = line.text_end;
  if (end > line.text_start && end <= source_->text.size() &&
      source_->text[end - 1] == '\n') {
    --end;
  }
  std::size_t nearest = end;
  float distance = std::numeric_limits<float>::max();
  const auto consider = [&](float x, std::size_t offset) {
    const float gap = std::abs(point.x - x);
    if (gap < distance) {
      distance = gap;
      nearest = offset;
    }
  };
  consider(line.x + line.width, end);
  for (const text::GlyphRun& run :
       laid.runs().subspan(line.first_run, line.run_count)) {
    for (const text::PositionedGlyph& glyph : laid.glyphs(run)) {
      if (glyph.cluster >= end) {
        continue;
      }
      consider(run.right_to_left ? glyph.x + glyph.advance : glyph.x,
               glyph.cluster);
    }
  }
  return CharacterIndex{std::min(nearest, end)};
}

auto Text::Layout::caret(CharacterIndex index) const -> composition::Rect {
  static constexpr float caret_width = 2.0F;
  if (empty()) {
    const float height = extent().height;
    return composition::Rect{
        .x = 0.0F, .y = 0.0F, .width = caret_width, .height = height};
  }
  const text::TextLine& line = source_->layout->lines()[line_of(index)];
  const float x = boundary(line, index.value());
  return composition::Rect{.x = x - (caret_width * 0.5F),
                           .y = line.top,
                           .width = caret_width,
                           .height = line.height};
}

auto Text::Layout::rects(composition::TextSelection::Range range) const
    -> std::vector<composition::Rect> {
  static constexpr float newline_width = 4.0F;
  std::vector<composition::Rect> found;
  if (empty() || range.empty()) {
    return found;
  }
  for (const text::TextLine& line : source_->layout->lines()) {
    if (line.text_end <= range.lower || line.text_start >= range.upper) {
      continue;
    }
    const float start =
        boundary(line, std::max<std::size_t>(range.lower, line.text_start));
    float finish = range.upper >= line.text_end ? line.x + line.width
                                                : boundary(line, range.upper);
    if (range.upper >= line.text_end && line.text_end > line.text_start &&
        line.text_end <= source_->text.size() &&
        source_->text[line.text_end - 1] == '\n') {
      finish += newline_width;
    }
    found.push_back(composition::Rect{.x = std::min(start, finish),
                                      .y = line.top,
                                      .width = std::abs(finish - start),
                                      .height = line.height});
  }
  return found;
}

auto operator==(const Text::Layout& left, const Text::Layout& right) -> bool {
  if (left.source_ == right.source_) {
    return true;
  }
  if (left.source_ == nullptr || right.source_ == nullptr) {
    return false;
  }
  const Text::Layout::Source& one = *left.source_;
  const Text::Layout::Source& other = *right.source_;
  if (one.text != other.text || one.truncated != other.truncated ||
      one.foreground != other.foreground ||
      (one.layout == nullptr) != (other.layout == nullptr)) {
    return false;
  }
  if (one.layout == other.layout) {
    return one.attributed == other.attributed && one.custom == other.custom;
  }
  const auto same_glyph = [](const text::PositionedGlyph& first,
                             const text::PositionedGlyph& second) {
    return first.glyph == second.glyph && first.x == second.x &&
           first.y == second.y && first.cluster == second.cluster;
  };
  return one.layout->size() == other.layout->size() &&
         std::ranges::equal(one.layout->glyphs(), other.layout->glyphs(),
                            same_glyph) &&
         one.attributed == other.attributed && one.custom == other.custom;
}

auto Text::Layout::Line::line() const noexcept -> const text::TextLine& {
  return source_->layout->lines()[index_];
}

auto Text::Layout::Line::origin() const noexcept -> composition::Point {
  const text::TextLine& held = line();
  return composition::Point{.x = held.x, .y = held.baseline};
}

auto Text::Layout::Line::typographic_bounds() const -> TypographicBounds {
  const text::TextLine& held = line();
  float leading = 0.0F;
  for (const text::GlyphRun& run :
       source_->layout->runs().subspan(held.first_run, held.run_count)) {
    leading = std::max(leading, run.face->metrics(run.size).line_gap);
  }
  return TypographicBounds{
      .origin = composition::Point{.x = held.x, .y = held.baseline},
      .width = held.width,
      .ascent = held.baseline - held.top,
      .descent = held.top + held.height - held.baseline,
      .leading = leading};
}

auto Text::Layout::Line::end_index() const noexcept -> std::size_t {
  return line().run_count;
}

auto Text::Layout::Line::operator[](std::size_t index) const -> Run {
  return Run{source_, index_, line().first_run + index};
}

auto Text::Layout::Line::begin() const -> Cursor<Line, Run> {
  return Cursor<Line, Run>{this, 0};
}

auto Text::Layout::Line::end() const -> Cursor<Line, Run> {
  return Cursor<Line, Run>{this, end_index()};
}

auto Text::Layout::Line::characters() const noexcept
    -> composition::TextSelection::Range {
  const text::TextLine& held = line();
  return composition::TextSelection::Range{.lower = held.text_start,
                                           .upper = held.text_end};
}

auto operator==(const Text::Layout::Line& left, const Text::Layout::Line& right)
    -> bool {
  return left.index_ == right.index_ &&
         Text::Layout{left.source_} == Text::Layout{right.source_};
}

auto Text::Layout::Run::glyph_run() const noexcept -> const text::GlyphRun& {
  return source_->layout->runs()[run_];
}

auto Text::Layout::Run::text_line() const noexcept -> const text::TextLine& {
  return source_->layout->lines()[line_];
}

auto Text::Layout::Run::glyphs() const noexcept
    -> std::span<const text::PositionedGlyph> {
  return source_->layout->glyphs(glyph_run());
}

auto Text::Layout::Run::end_index() const noexcept -> std::size_t {
  return glyph_run().glyph_count;
}

auto Text::Layout::Run::operator[](std::size_t index) const -> RunSlice {
  return RunSlice{*this, IndexRange{.lower = index, .upper = index + 1}};
}

auto Text::Layout::Run::slice(IndexRange bounds) const -> RunSlice {
  return RunSlice{*this, bounds};
}

auto Text::Layout::Run::begin() const -> Cursor<Run, RunSlice> {
  return Cursor<Run, RunSlice>{this, 0};
}

auto Text::Layout::Run::end() const -> Cursor<Run, RunSlice> {
  return Cursor<Run, RunSlice>{this, end_index()};
}

auto Text::Layout::Run::layout_direction() const noexcept
    -> composition::LayoutDirection {
  return glyph_run().right_to_left
             ? composition::LayoutDirection::right_to_left
             : composition::LayoutDirection::left_to_right;
}

auto Text::Layout::Run::bounds(IndexRange range) const -> TypographicBounds {
  const std::span<const text::PositionedGlyph> all = glyphs();
  const std::size_t upper = std::min(range.upper, all.size());
  const std::size_t lower = std::min(range.lower, upper);
  const text::GlyphRun& run = glyph_run();
  const text::FaceMetrics metrics = run.face->metrics(run.size);
  const float baseline = text_line().baseline;
  if (lower == upper) {
    const float x = lower < all.size() ? all[lower].x
                    : all.empty()      ? text_line().x
                                       : all.back().x + all.back().advance;
    return TypographicBounds{
        .origin = composition::Point{.x = x, .y = baseline},
        .width = 0.0F,
        .ascent = metrics.ascent,
        .descent = metrics.descent,
        .leading = metrics.line_gap};
  }
  float left = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::lowest();
  for (const text::PositionedGlyph& glyph : all.subspan(lower, upper - lower)) {
    left = std::min(left, glyph.x);
    right = std::max(right, glyph.x + glyph.advance);
  }
  return TypographicBounds{
      .origin = composition::Point{.x = left, .y = baseline},
      .width = right - left,
      .ascent = metrics.ascent,
      .descent = metrics.descent,
      .leading = metrics.line_gap};
}

auto Text::Layout::Run::typographic_bounds() const -> TypographicBounds {
  return bounds(IndexRange{.lower = 0, .upper = end_index()});
}

auto Text::Layout::Run::characters(IndexRange range) const
    -> std::vector<CharacterIndex> {
  const std::span<const text::PositionedGlyph> all = glyphs();
  const std::size_t upper = std::min(range.upper, all.size());
  const std::size_t lower = std::min(range.lower, upper);
  std::vector<CharacterIndex> found;
  found.reserve(upper - lower);
  for (const text::PositionedGlyph& glyph : all.subspan(lower, upper - lower)) {
    found.emplace_back(glyph.cluster);
  }
  return found;
}

auto Text::Layout::Run::character_indices() const
    -> std::vector<CharacterIndex> {
  return characters(IndexRange{.lower = 0, .upper = end_index()});
}

auto operator==(const Text::Layout::Run& left, const Text::Layout::Run& right)
    -> bool {
  return left.line_ == right.line_ && left.run_ == right.run_ &&
         Text::Layout{left.source_} == Text::Layout{right.source_};
}

auto Text::Layout::RunSlice::operator[](std::size_t index) const -> RunSlice {
  return RunSlice{run_, IndexRange{.lower = index, .upper = index + 1}};
}

auto Text::Layout::RunSlice::slice(IndexRange bounds) const -> RunSlice {
  return RunSlice{run_, bounds};
}

auto Text::Layout::RunSlice::begin() const -> Cursor<RunSlice, RunSlice> {
  return Cursor<RunSlice, RunSlice>{this, indices_.lower};
}

auto Text::Layout::RunSlice::end() const -> Cursor<RunSlice, RunSlice> {
  return Cursor<RunSlice, RunSlice>{this, indices_.upper};
}

auto Text::Layout::RunSlice::typographic_bounds() const -> TypographicBounds {
  return run_.bounds(indices_);
}

auto Text::Layout::RunSlice::character_indices() const
    -> std::vector<CharacterIndex> {
  return run_.characters(indices_);
}

}
