#include <arisa/provider/gpu/gpu_frame.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::provider {

GpuFrame::GpuFrame(std::unique_ptr<Implementation> implementation)
    : implementation_(std::move(implementation)) {}

GpuFrame::GpuFrame(std::shared_ptr<Implementation> implementation,
                   Reuse) noexcept
    : implementation_(std::move(implementation)) {}

auto GpuFrame::reused(std::shared_ptr<Implementation> implementation) noexcept
    -> GpuFrame {
  return GpuFrame{std::move(implementation), Reuse{}};
}

auto GpuFrame::operator=(GpuFrame&& other) noexcept -> GpuFrame& {
  if (this != &other) {
    release();
    implementation_ = std::move(other.implementation_);
  }
  return *this;
}

GpuFrame::~GpuFrame() { release(); }

void GpuFrame::release() noexcept {
  if (implementation_ != nullptr) {
    implementation_->finish();
    implementation_.reset();
  }
}

auto GpuFrame::active() const noexcept -> bool {
  return implementation_ != nullptr;
}

auto GpuFrame::allocate_geometry(std::uint32_t vertex_count,
                                 std::uint32_t index_count)
    -> std::expected<FrameGeometry, ProviderError> {
  if (implementation_ == nullptr || vertex_count == 0 || index_count == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return implementation_->allocate_geometry(vertex_count, index_count);
}

auto GpuFrame::allocate_shapes(std::uint32_t vertex_count,
                               std::uint32_t index_count)
    -> std::expected<ShapeGeometry, ProviderError> {
  if (implementation_ == nullptr || vertex_count == 0 || index_count == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return implementation_->allocate_shapes(vertex_count, index_count);
}

auto GpuFrame::allocate_resident(std::uint32_t vertex_count,
                                 std::uint32_t index_count)
    -> std::expected<ResidentShapes, ProviderError> {
  if (implementation_ == nullptr || vertex_count == 0 || index_count == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return implementation_->allocate_resident(vertex_count, index_count);
}

auto GpuFrame::replay(const ReplayDrawData& data)
    -> std::expected<void, ProviderError> {
  const bool covered =
      std::ranges::all_of(data.commands, [&data](const DrawCommand& command) {
        return command.index_offset <= data.indices.size() &&
               command.index_count <=
                   data.indices.size() - command.index_offset &&
               (command.index_count == 0 ||
                command.vertex_offset < data.vertex_count);
      });
  if (const auto refused =
          refusal(implementation_ != nullptr, covered, std::nullopt)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::invalid_argument, *refused});
  }
  return implementation_->replay(data);
}

void GpuFrame::touch(VertexRange range) {
  if (implementation_ != nullptr && range.count > 0) {
    implementation_->touch(range);
  }
}

void GpuFrame::damage(ClipRect region) {
  if (implementation_ != nullptr) {
    implementation_->damage(region);
  }
}

void GpuFrame::plan(FramePlan plan) {
  if (implementation_ != nullptr) {
    implementation_->plan(plan);
  }
}

void GpuFrame::clear(Colour colour) {
  if (implementation_ != nullptr) {
    implementation_->clear(colour);
  }
}

auto GpuFrame::draw(const DrawData& data)
    -> std::expected<void, ProviderError> {
  if (const auto refused = refusal(implementation_ != nullptr,
                                   well_formed(data), std::nullopt)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::invalid_argument, *refused});
  }
  return implementation_->draw(data);
}

auto GpuFrame::draw(const ShapeDrawData& data)
    -> std::expected<void, ProviderError> {
  if (const auto refused = refusal(implementation_ != nullptr,
                                   well_formed(data), std::nullopt)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::invalid_argument, *refused});
  }
  return implementation_->draw(data);
}

auto GpuFrame::draw(const ShapeDrawData& data, const RenderPass& pass)
    -> std::expected<void, ProviderError> {
  if (const auto refused =
          refusal(implementation_ != nullptr, well_formed(data), pass)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::invalid_argument, *refused});
  }
  if (samples(data, pass.target)) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::samples_target});
  }
  return implementation_->draw(data, pass);
}

auto GpuFrame::samples(const ShapeDrawData& data, TextureId target) noexcept
    -> bool {
  return std::ranges::any_of(
      data.commands, [target](const DrawCommand& command) {
        return command.texture == target || command.detail == target;
      });
}

auto GpuFrame::refusal(bool implemented, bool formed,
                       const std::optional<RenderPass>& pass) noexcept
    -> std::optional<ProviderErrorDetail> {
  if (!implemented) {
    return ProviderErrorDetail::no_frame;
  }
  if (!formed) {
    return ProviderErrorDetail::malformed_draw;
  }
  if (!pass.has_value()) {
    return std::nullopt;
  }
  if (pass->target.value() == 0) {
    return ProviderErrorDetail::zero_target;
  }
  const PassRegion& region = pass->region;
  if (!std::isfinite(region.x) || !std::isfinite(region.y) ||
      !std::isfinite(region.width) || !std::isfinite(region.height)) {
    return ProviderErrorDetail::non_finite_region;
  }
  if (!(region.width > 0.0F) || !(region.height > 0.0F)) {
    return ProviderErrorDetail::empty_region;
  }
  return std::nullopt;
}

auto GpuFrame::present() -> std::expected<void, ProviderError> {
  auto sealed = seal();
  if (!sealed.has_value()) {
    return std::unexpected(sealed.error());
  }
  return sealed->submit().transform([](const FrameReceipt&) {});
}

auto GpuFrame::seal() -> std::expected<FramePacket, ProviderError> {
  if (implementation_ == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::no_frame});
  }
  std::shared_ptr<Implementation> sealing = std::move(implementation_);
  auto sealed = sealing->seal();
  if (!sealed.has_value()) {
    sealing->finish();
    return std::unexpected(sealed.error());
  }
  if (sealed->submitter == nullptr) {
    return FramePacket{std::move(sealing), sealed->contents};
  }
  return FramePacket{std::move(sealed->submitter), sealed->contents};
}

auto GpuFrame::Implementation::submit(const FramePacket&, FramePacket::Clock)
    -> std::expected<FrameReceipt, ProviderError> {
  auto presented = present();
  finish();
  return presented.transform([] { return FrameReceipt{}; });
}

void GpuFrame::Implementation::abandon(const FramePacket&) noexcept {
  finish();
}

}
