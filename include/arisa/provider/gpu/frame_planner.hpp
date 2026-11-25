#pragma once

#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/surface_underlay.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider::gpu {

class FramePlanner {
 public:
  using Hole = std::pair<std::size_t, SurfaceUnderlay>;

  struct Batch {
    std::size_t commands{0};
    std::span<const Hole> holes;
  };

  struct RecordedPass {
    RenderPass pass;
    std::optional<SurfaceSize> target;
    Batch batch;
  };

  struct RecordedScreen {
    std::size_t image{0};
    std::size_t images{1};
    SurfaceSize size;
    std::optional<ClipRect> damage;
    bool whole{false};
    std::span<const Batch> batches;
  };

  struct RecordedFrame {
    std::span<const RecordedPass> passes;
    std::optional<RecordedScreen> screen;
  };

  struct DrawRange {
    std::size_t batch{0};
    std::size_t first{0};
    std::size_t count{0};

    friend constexpr auto operator==(const DrawRange&, const DrawRange&)
        -> bool = default;
  };

  struct UnderlayFill {
    SurfaceUnderlay underlay;
    ClipRect scissor;

    friend constexpr auto operator==(const UnderlayFill&, const UnderlayFill&)
        -> bool = default;
  };

  using Segment = std::variant<DrawRange, UnderlayFill>;

  struct ClearTexture {
    TextureId texture{0};

    friend constexpr auto operator==(const ClearTexture&, const ClearTexture&)
        -> bool = default;
  };

  struct RenderSurfaces {};

  struct OffscreenPass {
    std::size_t pass{0};
    TextureId target{0};
    SurfaceSize size;
    PassRegion region;
    std::optional<Colour> clear;
    std::span<const Segment> segments;
  };

  struct SkippedPass {
    std::size_t pass{0};
  };

  struct ScreenPass {
    ClipRect region;
    bool whole{true};
    std::optional<ClipRect> scissor;
    std::span<const Segment> segments;

    auto painted() const noexcept -> bool {
      return region.width > 0 && region.height > 0;
    }
  };

  using Step = std::variant<ClearTexture, RenderSurfaces, OffscreenPass,
                            SkippedPass, ScreenPass>;

  void created(TextureId texture, const TextureDescription& description);

  auto upload(TextureId texture, TextureRegion region)
      -> std::optional<ClearTexture>;

  void seal();

  auto plan(const RecordedFrame& frame) -> std::span<const Step>;

  auto owed() -> std::span<const Step>;

  void invalidate() noexcept;

  auto unwritten() const noexcept -> std::size_t {
    return fresh_.size() + sealed_.size();
  }

 private:
  struct Extent {
    std::uint32_t width{0};
    std::uint32_t height{0};
  };

  struct Pending {
    bool whole{true};
    ClipRect region{};
  };

  auto settle(TextureId texture) -> std::optional<Extent>;

  static auto writes(const RecordedPass& pass) noexcept -> bool;

  static auto unite(ClipRect a, ClipRect b) noexcept -> ClipRect;

  auto covered(TextureId texture,
               std::span<const RecordedPass> passes) const noexcept -> bool;

  void plan_pass(std::size_t index, const RecordedPass& pass);

  void plan_screen(const RecordedScreen& screen);

  void plan_clears(std::span<const RecordedPass> passes);

  auto damaged(const RecordedScreen& screen) -> std::pair<ClipRect, bool>;

  static auto bound(const RecordedFrame& frame) noexcept -> std::size_t;

  auto segments(const RecordedPass& pass) -> std::span<const Segment>;

  auto segments(std::span<const Batch> batches, ClipRect region)
      -> std::span<const Segment>;

  std::flat_map<TextureId, Extent> fresh_;
  std::flat_map<TextureId, Extent> sealed_;
  std::vector<Pending> pending_;
  std::vector<Step> steps_;
  std::vector<Segment> segments_;
};

}
