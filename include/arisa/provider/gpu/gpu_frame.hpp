#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_packet.hpp>
#include <arisa/provider/gpu/render_pass.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

namespace arisa::provider {

class GpuFrame {
 public:
  struct Sealed {
    std::shared_ptr<FramePacket::Submitter> submitter;
    FramePacket::Contents contents{};
  };

  class Implementation : public FramePacket::Submitter {
   public:
    [[nodiscard]] virtual auto allocate_geometry(std::uint32_t vertex_count,
                                                 std::uint32_t index_count)
        -> std::expected<FrameGeometry, ProviderError> = 0;
    [[nodiscard]] virtual auto allocate_shapes(std::uint32_t vertex_count,
                                               std::uint32_t index_count)
        -> std::expected<ShapeGeometry, ProviderError> = 0;
    virtual void clear(Colour colour) = 0;
    [[nodiscard]] virtual auto draw(const DrawData& data)
        -> std::expected<void, ProviderError> = 0;
    [[nodiscard]] virtual auto draw(const ShapeDrawData& data)
        -> std::expected<void, ProviderError> = 0;
    [[nodiscard]] virtual auto draw(const ShapeDrawData& data,
                                    const RenderPass& pass)
        -> std::expected<void, ProviderError> = 0;
    [[nodiscard]] virtual auto present()
        -> std::expected<void, ProviderError> = 0;
    virtual void finish() noexcept {}
    [[nodiscard]] virtual auto allocate_resident(std::uint32_t vertex_count,
                                                 std::uint32_t index_count)
        -> std::expected<ResidentShapes, ProviderError> {
      return allocate_shapes(vertex_count, index_count)
          .transform([](const ShapeGeometry& geometry) {
            return ResidentShapes{.geometry = geometry, .generation = 0};
          });
    }
    [[nodiscard]] virtual auto replay(const ReplayDrawData&)
        -> std::expected<void, ProviderError> {
      return std::unexpected(ProviderError::unsupported_capability());
    }
    virtual void touch(VertexRange) {}
    virtual void damage(ClipRect) {}
    virtual void plan(FramePlan) {}
    [[nodiscard]] virtual auto seal() -> std::expected<Sealed, ProviderError> {
      return Sealed{};
    }
    [[nodiscard]] auto submit(const FramePacket& packet,
                              FramePacket::Clock clock)
        -> std::expected<FrameReceipt, ProviderError> override;
    void abandon(const FramePacket& packet) noexcept override;
  };

  explicit GpuFrame(std::unique_ptr<Implementation> implementation);

  static auto reused(std::shared_ptr<Implementation> implementation) noexcept
      -> GpuFrame;

  GpuFrame(GpuFrame&& other) noexcept = default;
  auto operator=(GpuFrame&& other) noexcept -> GpuFrame&;
  GpuFrame(const GpuFrame&) = delete;
  auto operator=(const GpuFrame&) -> GpuFrame& = delete;
  ~GpuFrame();

  auto active() const noexcept -> bool;

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError>;

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError>;

  [[nodiscard]] auto allocate_resident(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<ResidentShapes, ProviderError>;

  [[nodiscard]] auto replay(const ReplayDrawData& data)
      -> std::expected<void, ProviderError>;

  void touch(VertexRange range);

  void damage(ClipRect region);

  void plan(FramePlan plan);

  void clear(Colour colour);

  [[nodiscard]] auto draw(const DrawData& data)
      -> std::expected<void, ProviderError>;

  [[nodiscard]] auto draw(const ShapeDrawData& data)
      -> std::expected<void, ProviderError>;

  [[nodiscard]] auto draw(const ShapeDrawData& data, const RenderPass& pass)
      -> std::expected<void, ProviderError>;

  [[nodiscard]] auto present() -> std::expected<void, ProviderError>;

  [[nodiscard]] auto seal() -> std::expected<FramePacket, ProviderError>;

  static auto well_formed(const DrawData& data) noexcept -> bool {
    return covers(data);
  }

  static auto well_formed(const ShapeDrawData& data) noexcept -> bool {
    return covers(data);
  }

 private:
  template <class V>
  static auto covers(const BasicDrawData<V>& data) noexcept -> bool {
    const std::size_t vertex_count = data.geometry.vertices.size();
    const std::size_t index_count = data.geometry.indices.size();
    return std::ranges::all_of(data.commands, [vertex_count, index_count](
                                                  const DrawCommand& command) {
      return command.index_offset <= index_count &&
             command.index_count <= index_count - command.index_offset &&
             (command.index_count == 0 || command.vertex_offset < vertex_count);
    });
  }

  struct Reuse {};

  static auto samples(const ShapeDrawData& data, TextureId target) noexcept
      -> bool;

  static auto refusal(bool implemented, bool formed,
                      const std::optional<RenderPass>& pass) noexcept
      -> std::optional<ProviderErrorDetail>;

  GpuFrame(std::shared_ptr<Implementation> implementation, Reuse) noexcept;

  void release() noexcept;

  std::shared_ptr<Implementation> implementation_;
};

}
