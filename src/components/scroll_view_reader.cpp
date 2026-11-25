#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/composition/context.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

ScrollViewReader::ScrollViewReader(Builder content)
    : Element(std::make_unique<Content>(std::move(content))) {}

auto ScrollViewReader::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ScrollViewReader::Content::composes() const noexcept -> bool {
  return true;
}

auto ScrollViewReader::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (builder_) {
    produced.push_back(builder_(context.scroll_view_proxy()));
  }
  return produced;
}

}
