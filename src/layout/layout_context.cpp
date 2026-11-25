#include <arisa/layout/layout_context.hpp>

#include <bridge/yoga/yoga_layout.hpp>

#include <utility>

namespace arisa::layout {

LayoutContext::LayoutContext(
    std::unique_ptr<bridge::yoga::YogaLayout> engine) noexcept
    : engine_(std::move(engine)) {}

LayoutContext::LayoutContext(LayoutContext&& other) noexcept = default;

auto LayoutContext::operator=(LayoutContext&& other) noexcept
    -> LayoutContext& = default;

LayoutContext::~LayoutContext() = default;

auto LayoutContext::create() -> std::expected<LayoutContext, LayoutError> {
  auto engine = bridge::yoga::YogaLayout::create();
  if (!engine.has_value()) {
    return std::unexpected(engine.error());
  }
  return LayoutContext{std::move(*engine)};
}

auto LayoutContext::update(composition::Tree& tree, Viewport viewport,
                           TypesetterRef typesetter)
    -> std::expected<bool, LayoutError> {
  const text::Typesetter* current =
      typesetter.has_value() ? &typesetter->get() : nullptr;
  if (!stale_ && revision_ == tree.revision() && viewport_ == viewport &&
      typesetter_ == current) {
    return false;
  }
  if (viewport.pixels.empty()) {
    return false;
  }
  auto ran = engine_->run(tree, viewport, typesetter, stale_);
  if (!ran.has_value()) {
    stale_ = true;
    return std::unexpected(ran.error());
  }
  revision_ = tree.revision();
  viewport_ = viewport;
  typesetter_ = current;
  stale_ = false;
  return *ran;
}

auto LayoutContext::passes() const noexcept -> std::uint64_t {
  return engine_->calculations();
}

auto LayoutContext::measurements() const noexcept -> std::uint64_t {
  return engine_->measurements();
}

auto LayoutContext::computations() const noexcept -> std::uint64_t {
  return engine_->computations();
}

auto LayoutContext::visits() const noexcept -> std::uint64_t {
  return engine_->visits();
}

auto LayoutContext::placements() const noexcept -> std::uint64_t {
  return engine_->placements();
}

}
