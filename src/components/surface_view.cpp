#include <arisa/components/surface_view.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/ui_error.hpp>

#include <format>
#include <memory>
#include <utility>

namespace arisa::components {

const composition::StateKey<std::shared_ptr<SurfaceView::Owner::Attachment>>
    SurfaceView::Owner::attachment{"surface-view-attachment"};

SurfaceView::SurfaceView(const provider::GpuSurface& surface)
    : Element(std::make_unique<Content>(surface.id(), surface.presence())) {
  fill();
}

SurfaceView::SurfaceView(std::shared_ptr<Source> source)
    : Element(std::make_unique<Owner>(std::move(source))) {
  fill();
}

void SurfaceView::fill() {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto SurfaceView::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto SurfaceView::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  return paints_like(other);
}

auto SurfaceView::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  return other.type() == type() &&
         static_cast<const Content&>(other).surface_ == surface_;
}

auto SurfaceView::Owner::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Owner>();
}

auto SurfaceView::Owner::same_as(const composition::ElementContent&) const
    -> bool {
  return false;
}

auto SurfaceView::Owner::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  std::shared_ptr<Attachment>& held = context.retained(attachment);
  if (held == nullptr) {
    auto attached = source_->attach();
    if (!attached.has_value()) {
      context.fail(composition::UiError{std::format(
          "the surface was refused: {}", attached.error().description())});
      return produced;
    }
    held = std::make_shared<Attachment>(source_, std::move(*attached));
  } else {
    held->refresh(*source_);
  }
  produced.emplace_back(SurfaceView{held->surface()});
  return produced;
}

void SurfaceView::Owner::Attachment::refresh(Source& incoming) {
  source_->adopt(incoming);
  surface_.set_drive(source_->options().drive);
  surface_.request_frame();
}

}
