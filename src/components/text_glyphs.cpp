#include <arisa/components/text_glyphs.hpp>

#include <algorithm>
#include <utility>

namespace arisa::components {

Text::Glyphs::Glyphs(std::shared_ptr<const Layout::Source> source,
                     std::size_t first, std::size_t last,
                     text::DrawingOptions options)
    : Element(
          std::make_unique<Content>(std::move(source), first, last, options)) {}

auto Text::Glyphs::first_glyph(const Layout::Run& run) noexcept -> std::size_t {
  return run.source()->layout->runs()[run.run_index()].first_glyph;
}

Text::Glyphs::Glyphs(const Layout::RunSlice& slice,
                     text::DrawingOptions options)
    : Glyphs(slice.run().source(),
             first_glyph(slice.run()) + slice.indices().lower,
             first_glyph(slice.run()) +
                 std::min(slice.indices().upper, slice.run().size()),
             options) {}

Text::Glyphs::Glyphs(const Layout::Run& run, text::DrawingOptions options)
    : Glyphs(run.source(), first_glyph(run), first_glyph(run) + run.size(),
             options) {}

Text::Glyphs::Glyphs(const Layout::Line& line, text::DrawingOptions options)
    : Glyphs(line.size() == 0 ? nullptr : line[0].source(),
             line.size() == 0 ? 0 : first_glyph(line[0]),
             line.size() == 0 ? 0
                              : first_glyph(line[line.size() - 1]) +
                                    line[line.size() - 1].size(),
             options) {}

auto Text::Glyphs::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Text::Glyphs::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return first_ == theirs.first_ && last_ == theirs.last_ &&
         options_ == theirs.options_ &&
         Layout{source_} == Layout{theirs.source_};
}

}
