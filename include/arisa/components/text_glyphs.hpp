#pragma once

#include <arisa/components/text.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/text/drawing_options.hpp>

#include <cstddef>
#include <memory>

namespace arisa::components {

class Text::Glyphs : public composition::Element {
 public:
  class Content;

  Glyphs(const Layout::RunSlice& slice, text::DrawingOptions options = {});

  Glyphs(const Layout::Run& run, text::DrawingOptions options = {});

  Glyphs(const Layout::Line& line, text::DrawingOptions options = {});

 private:
  Glyphs(std::shared_ptr<const Layout::Source> source, std::size_t first,
         std::size_t last, text::DrawingOptions options);

  static auto first_glyph(const Layout::Run& run) noexcept -> std::size_t;
};

class Text::Glyphs::Content final : public composition::ElementContent {
 public:
  Content(std::shared_ptr<const Layout::Source> source, std::size_t first,
          std::size_t last, text::DrawingOptions options) noexcept
      : source_(std::move(source)),
        first_(first),
        last_(last),
        options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto source() const noexcept -> const std::shared_ptr<const Layout::Source>& {
    return source_;
  }

  auto first() const noexcept -> std::size_t { return first_; }

  auto last() const noexcept -> std::size_t { return last_; }

  auto options() const noexcept -> text::DrawingOptions { return options_; }

 private:
  std::shared_ptr<const Layout::Source> source_;
  std::size_t first_{0};
  std::size_t last_{0};
  text::DrawingOptions options_{};
};

}
