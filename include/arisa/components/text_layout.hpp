#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/theme/colour_value.hpp>

#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class Text::Layout {
 public:
  class Line;
  class Run;
  class RunSlice;
  class CharacterIndex;
  struct TypographicBounds;

  struct IndexRange {
    std::size_t lower{0};
    std::size_t upper{0};

    friend constexpr auto operator==(const IndexRange&, const IndexRange&)
        -> bool = default;
  };

  struct Source {
    std::shared_ptr<const text::TextLayout> layout;
    std::string text;
    AttributedString attributed;
    AttributedString::Custom custom;
    std::optional<theme::ColourValue> foreground;
    bool truncated{false};
  };

  template <class Owner, class Value>
  class Cursor {
   public:
    using value_type = Value;
    using difference_type = std::ptrdiff_t;

    Cursor() = default;

    Cursor(const Owner* owner, std::size_t index) noexcept
        : owner_(owner), index_(index) {}

    auto operator*() const -> Value { return (*owner_)[index_]; }

    auto operator++() -> Cursor& {
      ++index_;
      return *this;
    }

    auto operator++(int) -> Cursor {
      Cursor previous = *this;
      ++index_;
      return previous;
    }

    friend auto operator==(const Cursor& left, const Cursor& right) noexcept
        -> bool {
      return left.index_ == right.index_;
    }

   private:
    const Owner* owner_{nullptr};
    std::size_t index_{0};
  };

  Layout() = default;

  explicit Layout(std::shared_ptr<const Source> source) noexcept
      : source_(std::move(source)) {}

  auto is_truncated() const noexcept -> bool;

  auto start_index() const noexcept -> std::size_t { return 0; }

  auto end_index() const noexcept -> std::size_t;

  auto size() const noexcept -> std::size_t { return end_index(); }

  auto empty() const noexcept -> bool { return end_index() == 0; }

  auto operator[](std::size_t index) const -> Line;

  auto begin() const -> Cursor<Layout, Line>;

  auto end() const -> Cursor<Layout, Line>;

  auto text() const noexcept -> std::string_view;

  auto extent() const noexcept -> composition::Size;

  auto source() const noexcept -> const std::shared_ptr<const Source>& {
    return source_;
  }

  auto character_at(composition::Point point) const -> CharacterIndex;

  auto caret(CharacterIndex index) const -> composition::Rect;

  auto rects(composition::TextSelection::Range range) const
      -> std::vector<composition::Rect>;

  auto line_of(CharacterIndex index) const -> std::size_t;

  friend auto operator==(const Layout& left, const Layout& right) -> bool;

 private:
  auto boundary(const text::TextLine& line, std::size_t offset) const -> float;

  std::shared_ptr<const Source> source_;
};

class Text::Layout::CharacterIndex {
 public:
  constexpr CharacterIndex() noexcept = default;

  constexpr explicit CharacterIndex(std::size_t value) noexcept
      : value_(value) {}

  constexpr auto value() const noexcept -> std::size_t { return value_; }

  constexpr auto advanced(std::ptrdiff_t by) const noexcept -> CharacterIndex {
    return CharacterIndex{
        static_cast<std::size_t>(static_cast<std::ptrdiff_t>(value_) + by)};
  }

  constexpr auto distance(CharacterIndex other) const noexcept
      -> std::ptrdiff_t {
    return static_cast<std::ptrdiff_t>(other.value_) -
           static_cast<std::ptrdiff_t>(value_);
  }

  friend constexpr auto operator<=>(const CharacterIndex&,
                                    const CharacterIndex&) = default;

 private:
  std::size_t value_{0};
};

struct Text::Layout::TypographicBounds {
  composition::Point origin{};
  float width{0.0F};
  float ascent{0.0F};
  float descent{0.0F};
  float leading{0.0F};

  constexpr auto rect() const noexcept -> composition::Rect {
    return composition::Rect{.x = origin.x,
                             .y = origin.y - ascent,
                             .width = width,
                             .height = ascent + descent};
  }

  friend constexpr auto operator==(const TypographicBounds&,
                                   const TypographicBounds&) -> bool = default;
};

class Text::Layout::Line {
 public:
  auto origin() const noexcept -> composition::Point;

  auto typographic_bounds() const -> TypographicBounds;

  auto start_index() const noexcept -> std::size_t { return 0; }

  auto end_index() const noexcept -> std::size_t;

  auto size() const noexcept -> std::size_t { return end_index(); }

  auto operator[](std::size_t index) const -> Run;

  auto begin() const -> Cursor<Line, Run>;

  auto end() const -> Cursor<Line, Run>;

  auto characters() const noexcept -> composition::TextSelection::Range;

  friend auto operator==(const Line& left, const Line& right) -> bool;

 private:
  friend class Layout;

  Line(std::shared_ptr<const Source> source, std::size_t index) noexcept
      : source_(std::move(source)), index_(index) {}

  auto line() const noexcept -> const text::TextLine&;

  std::shared_ptr<const Source> source_;
  std::size_t index_{0};
};

class Text::Layout::Run {
 public:
  auto start_index() const noexcept -> std::size_t { return 0; }

  auto end_index() const noexcept -> std::size_t;

  auto size() const noexcept -> std::size_t { return end_index(); }

  auto operator[](std::size_t index) const -> RunSlice;

  auto slice(IndexRange bounds) const -> RunSlice;

  auto begin() const -> Cursor<Run, RunSlice>;

  auto end() const -> Cursor<Run, RunSlice>;

  template <TextAttribute T>
  auto get() const -> std::optional<T> {
    return attribute<T>(source_, glyph_run().text_start);
  }

  auto layout_direction() const noexcept -> composition::LayoutDirection;

  auto typographic_bounds() const -> TypographicBounds;

  auto character_indices() const -> std::vector<CharacterIndex>;

  auto source() const noexcept -> const std::shared_ptr<const Source>& {
    return source_;
  }

  auto line_index() const noexcept -> std::size_t { return line_; }

  auto run_index() const noexcept -> std::size_t { return run_; }

  auto glyphs() const noexcept -> std::span<const text::PositionedGlyph>;

  auto bounds(IndexRange glyphs) const -> TypographicBounds;

  auto characters(IndexRange glyphs) const -> std::vector<CharacterIndex>;

  friend auto operator==(const Run& left, const Run& right) -> bool;

 private:
  friend class Line;

  Run(std::shared_ptr<const Source> source, std::size_t line,
      std::size_t run) noexcept
      : source_(std::move(source)), line_(line), run_(run) {}

  template <TextAttribute T>
  static auto attribute(const std::shared_ptr<const Source>& source,
                        std::size_t offset) -> std::optional<T> {
    if (source == nullptr) {
      return std::nullopt;
    }
    if (offset < source->attributed.text().size()) {
      if (std::optional<T> found =
              source->attributed.attributes(static_cast<std::uint32_t>(offset))
                  .custom.template get<T>()) {
        return found;
      }
    }
    return source->custom.template get<T>();
  }

  auto glyph_run() const noexcept -> const text::GlyphRun&;

  auto text_line() const noexcept -> const text::TextLine&;

  std::shared_ptr<const Source> source_;
  std::size_t line_{0};
  std::size_t run_{0};

  friend class RunSlice;
};

class Text::Layout::RunSlice {
 public:
  RunSlice(Run run, IndexRange indices) noexcept
      : run_(std::move(run)), indices_(indices) {}

  auto run() const noexcept -> const Run& { return run_; }

  auto indices() const noexcept -> IndexRange { return indices_; }

  auto start_index() const noexcept -> std::size_t { return indices_.lower; }

  auto end_index() const noexcept -> std::size_t { return indices_.upper; }

  auto size() const noexcept -> std::size_t {
    return indices_.upper - indices_.lower;
  }

  auto operator[](std::size_t index) const -> RunSlice;

  auto slice(IndexRange bounds) const -> RunSlice;

  auto begin() const -> Cursor<RunSlice, RunSlice>;

  auto end() const -> Cursor<RunSlice, RunSlice>;

  template <TextAttribute T>
  auto get() const -> std::optional<T> {
    const std::span<const text::PositionedGlyph> glyphs = run_.glyphs();
    const std::size_t offset =
        indices_.lower < glyphs.size()
            ? glyphs[indices_.lower].cluster
            : static_cast<std::size_t>(run_.glyph_run().text_start);
    return Run::attribute<T>(run_.source(), offset);
  }

  auto typographic_bounds() const -> TypographicBounds;

  auto character_indices() const -> std::vector<CharacterIndex>;

  friend auto operator==(const RunSlice&, const RunSlice&) -> bool = default;

 private:
  Run run_;
  IndexRange indices_;
};

}
