#include <arisa/components/geometry_reader.hpp>
#include <arisa/composition/context.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

GeometryReader::GeometryReader(Builder content)
    : Element(std::make_unique<Content>(std::move(content))) {}

auto GeometryReader::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto GeometryReader::Content::composes() const noexcept -> bool { return true; }

auto GeometryReader::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (builder_) {
    const composition::StateHandle<composition::GeometryProxy> proxy =
        context.state(proxy_key);
    produced.push_back(builder_(proxy.get()));
  }
  return produced;
}

auto GeometryReader::Content::reads_geometry() const noexcept -> bool {
  return true;
}

void GeometryReader::Content::place(composition::Context& context,
                                    const composition::GeometryProxy& proxy) {
  context.state(proxy_key).set(proxy);
}

}
