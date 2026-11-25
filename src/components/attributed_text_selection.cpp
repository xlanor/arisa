#include <arisa/components/attributed_text_selection.hpp>

#include <arisa/text/utf8.hpp>

#include <algorithm>

namespace arisa::components {

auto AttributedTextSelection::Indices::ranges(std::vector<Range> ranges)
    -> Indices {
  std::ranges::sort(ranges, {}, &Range::lower);
  std::vector<Range> merged;
  for (const Range& range : ranges) {
    if (!merged.empty() && range.lower <= merged.back().upper) {
      merged.back().upper = std::max(merged.back().upper, range.upper);
      continue;
    }
    merged.push_back(range);
  }
  if (merged.empty()) {
    merged.push_back(Range{});
  }
  const bool insertion = merged.size() == 1 && merged.front().empty();
  return Indices{std::move(merged), insertion};
}

auto AttributedTextSelection::Indices::range() const noexcept -> Range {
  if (ranges_.empty()) {
    return Range{};
  }
  return Range{.lower = ranges_.front().lower, .upper = ranges_.back().upper};
}

auto AttributedTextSelection::of(
    const composition::TextSelection& selection,
    std::optional<AttributedString::Attributes> typing)
    -> AttributedTextSelection {
  AttributedTextSelection made =
      selection.is_insertion()
          ? AttributedTextSelection{InsertionPoint{.index =
                                                       selection.range().lower},
                                    std::move(typing)}
          : AttributedTextSelection{
                std::vector<Range>{selection.indices().ranges().begin(),
                                   selection.indices().ranges().end()}};
  made.affinity_ = selection.affinity();
  return made;
}

auto AttributedTextSelection::indices(const AttributedString& text) const
    -> Indices {
  const std::string_view characters = text.text();
  if (indices_.is_insertion_point()) {
    return Indices::insertion_point(
        text::Utf8::boundary(characters, indices_.insertion()));
  }
  std::vector<Range> clamped;
  for (const Range& range : indices_.ranges()) {
    const Range bounded = range.clamped(characters.size());
    clamped.push_back(
        Range{.lower = text::Utf8::boundary(characters, bounded.lower),
              .upper = text::Utf8::boundary(characters, bounded.upper)});
  }
  return Indices::ranges(std::move(clamped));
}

auto AttributedTextSelection::affinity(const AttributedString& text) const
    -> composition::TextSelectionAffinity {
  if (affinity_ != composition::TextSelectionAffinity::automatic) {
    return affinity_;
  }
  const Indices bounded = indices(text);
  return bounded.is_insertion_point() && bounded.insertion() == 0
             ? composition::TextSelectionAffinity::downstream
             : composition::TextSelectionAffinity::upstream;
}

auto AttributedTextSelection::typing_attributes(
    const AttributedString& text) const -> AttributedString::Attributes {
  if (typing_.has_value()) {
    return *typing_;
  }
  const Indices bounded = indices(text);
  const std::size_t at = bounded.range().lower;
  if (text.text().empty()) {
    return AttributedString::Attributes{};
  }
  const std::size_t offset =
      at == 0
          ? 0
          : text::Utf8::previous(text.text(), std::min(at, text.text().size()));
  AttributedString::Attributes found =
      text.attributes(static_cast<std::uint32_t>(offset));
  found.link.reset();
  found.image.reset();
  found.adaptive_image_glyph.reset();
  return found;
}

auto AttributedTextSelection::attributes(const AttributedString& text) const
    -> Attributes {
  const Indices bounded = indices(text);
  if (bounded.is_insertion_point()) {
    return Attributes{
        std::vector<AttributedString::Attributes>{typing_attributes(text)}};
  }
  std::vector<AttributedString::Attributes> found;
  for (const Range& range : bounded.ranges()) {
    std::size_t at = range.lower;
    while (at < range.upper) {
      AttributedString::Attributes held =
          text.attributes(static_cast<std::uint32_t>(at));
      if (found.empty() || found.back() != held) {
        found.push_back(std::move(held));
      }
      at = text::Utf8::next(text.text(), at);
    }
  }
  return Attributes{std::move(found)};
}

auto AttributedTextSelection::text_selection(const AttributedString& text) const
    -> composition::TextSelection {
  const Indices bounded = indices(text);
  composition::TextSelection made =
      bounded.is_insertion_point()
          ? composition::
                TextSelection{composition::TextSelection::InsertionPoint{
                    .index = bounded.insertion()}}
          : (bounded.ranges().size() == 1
                 ? composition::TextSelection{bounded.ranges().front()}
                 : composition::TextSelection{std::vector<Range>{
                       bounded.ranges().begin(), bounded.ranges().end()}});
  return made.with_affinity(affinity_);
}

auto AttributedTextSelection::with_typing_attributes(
    AttributedString::Attributes attributes) const -> AttributedTextSelection {
  AttributedTextSelection changed = *this;
  changed.typing_ = std::move(attributes);
  return changed;
}

}
