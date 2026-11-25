#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/gpu_frame.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/rendering/draw_list_cache.hpp>
#include <arisa/rendering/paint_error.hpp>
#include <arisa/rendering/render_target_pool.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::rendering {

class Compositor {
 public:
  struct Surface {
    provider::TextureId texture{0};
    provider::PassRegion region{};
    std::uint64_t key{0};
  };

  struct FieldShape {
    std::array<float, 2> centre{};
    std::array<float, 2> half_size{};
    float radius{0.0F};
    float smoothing{0.0F};

    friend constexpr auto operator==(const FieldShape&, const FieldShape&)
        -> bool = default;
  };

  struct BackdropRefusal {
    enum class Cause : std::uint8_t {
      empty_region,
      render_target,
      rejected_pass
    };

    Cause cause{Cause::empty_region};
    std::uint32_t width{0};
    std::uint32_t height{0};
    provider::ProviderError error{
        provider::ProviderErrorCode::invalid_argument};
  };

  struct QuadSpan {
    std::uint32_t vertex_begin{0};
    std::uint32_t vertex_end{0};
    std::uint32_t index_begin{0};
    std::array<float, 2> translation{};
    std::uint64_t content{0};
    bool damaged{false};
    DrawListCache::Bounds reach{};
  };

  struct LayerRequest {
    provider::PassRegion wanted{};
    std::uint64_t hash{0};
    std::array<float, 2> anchor{};
    std::optional<std::uint64_t> matching{};
  };

  struct LayerPlan {
    Surface surface;
    std::array<float, 2> extent{};
    std::uint64_t generation{0};
    bool render{false};
    bool inexact{false};
  };

  struct Statistics {
    std::uint32_t blurs{0};
    std::uint32_t blur_hits{0};
    std::uint32_t passes{0};
    std::uint32_t fields{0};
    std::uint32_t layer_renders{0};
    std::uint32_t layer_hits{0};
    std::uint32_t shared{0};

    friend constexpr auto operator==(const Statistics&, const Statistics&)
        -> bool = default;
  };

  static constexpr float backdrop_scale = 0.25F;
  static constexpr float light_scale = 0.5F;
  static constexpr float sharp_radius = 1.0F;
  static constexpr float level_radius = 4.0F;
  static constexpr float glass_level_radius = 6.0F;
  static constexpr float frost_radius = 7.0F;
  static constexpr float minimum_capture_scale = 0.125F;
  static constexpr float maximum_capture_pixels = 262144.0F;
  static constexpr std::uint32_t maximum_levels = 4;
  static constexpr std::uint64_t grace_frames = 3;
  static constexpr std::uint64_t retention_frames = 600;
  static constexpr std::uint32_t volatile_streak = 2;
  static constexpr float placement_tolerance = 1.0F / 256.0F;
  static constexpr std::size_t field_members = 8;
  static constexpr std::uint32_t field_header = 2;
  static constexpr std::uint32_t field_stride = 6;
  static constexpr std::uint32_t field_width =
      field_header + (field_stride * field_members);
  static constexpr float group_gap = 16.0F;
  static constexpr float group_slack = 1.5F;
  static constexpr std::size_t group_members = 16;
  static constexpr std::uint32_t table_width = 32;
  static constexpr std::uint32_t table_rows = 8;
  static constexpr std::uint32_t table_capacity = table_width * table_rows;
  static constexpr std::size_t rejected_limit = 256;
  static constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
  static constexpr std::uint64_t fnv_prime = 1099511628211ULL;
  static constexpr std::uint64_t duplicate_salt = 0x9E3779B97F4A7C15ULL;

  explicit Compositor(provider::GpuProvider& gpu);

  void report_to(provider::DiagnosticsProvider& diagnostics) noexcept {
    diagnostics_ = std::ref(diagnostics);
  }

  void advance_frame();

  void separate(bool enabled) noexcept;

  auto unsettled() const noexcept -> bool { return unsettled_; }

  auto statistics() const noexcept -> Statistics { return statistics_; }

  auto pool() const noexcept -> const RenderTargetPool& { return pool_; }

  void withdraw(provider::TextureId texture);

  auto withdrawn() const noexcept -> std::span<const provider::TextureId> {
    return withdrawn_;
  }

  static auto levels_for(float radius_pixels) noexcept -> std::uint32_t;

  static auto scale_for(float radius_pixels) noexcept -> float;

  static auto mix(std::uint64_t hash, std::span<const std::byte> bytes) noexcept
      -> std::uint64_t;

  [[nodiscard]] auto backdrop(std::uint64_t key, provider::PassRegion wanted,
                              provider::PassRegion core, float radius_pixels,
                              std::uint32_t first_quad)
      -> std::expected<Surface, BackdropRefusal>;

  auto predicts(std::uint64_t key, provider::PassRegion wanted,
                float radius_pixels, const Surface& surface) const -> bool;

  static auto capture_scale(provider::PassRegion wanted,
                            float radius_pixels) noexcept -> float;

  auto field(std::uint64_t key, std::uint64_t owner,
             std::span<const FieldShape> shapes, float blend)
      -> std::optional<provider::TextureId>;

  auto holds_field(std::uint64_t key, provider::TextureId texture) const
      -> bool;

  void retain_field(std::uint64_t key);

  auto layerable(std::uint64_t key) const -> bool;

  auto generation_of(provider::TextureId texture) const -> std::uint64_t;

  auto layered(std::uint64_t key) const -> bool;

  auto repaints(std::uint64_t key) const -> bool;

  auto layer(std::uint64_t key, const LayerRequest& request)
      -> std::optional<LayerPlan>;

  auto effect_layer(std::uint64_t key, const LayerRequest& request)
      -> std::optional<LayerPlan>;

  void redraw_layer(std::uint64_t key);

  auto table(std::uint64_t key, std::span<const float> values)
      -> std::optional<provider::TextureId>;

  void forget_layer(std::uint64_t key);

  void spoil_layer(std::uint64_t key);

  void count_layer(bool rendered) noexcept;

  auto pending_backdrops() const noexcept -> std::size_t {
    return backdrops_.size();
  }

  auto extra_quads() const noexcept -> std::uint32_t;

  void zoom(float factor, float centre_x, float centre_y);

  void emit(provider::GpuFrame& frame, const provider::ShapeGeometry& geometry,
            std::uint32_t base_vertex, std::uint32_t first_index,
            std::span<const provider::DrawCommand> commands,
            std::span<const QuadSpan> spans,
            std::optional<DrawListCache::Changes> damage,
            provider::Colour clear);

  void refuse(std::string_view what, std::uint32_t width, std::uint32_t height,
              const provider::ProviderError& error, std::string_view fallback,
              std::optional<std::uint64_t> node = std::nullopt);

  void refuse(std::string_view what, std::uint32_t width, std::uint32_t height,
              const PaintError& error, std::string_view fallback,
              std::optional<std::uint64_t> node);

 private:
  using Extent = std::optional<std::array<float, 4>>;

  struct Blur {
    RenderTargetPool::Lease result;
    std::array<RenderTargetPool::Lease, maximum_levels + 1> outputs;
    provider::PassRegion region{};
    provider::Colour clear{};
    std::uint32_t levels{1};
    std::uint32_t boundary{0};
    std::uint32_t hashed_boundary{0};
    std::uint8_t planned{0};
    std::uint8_t present{0};
    std::uint8_t computed{0};
    float scale{backdrop_scale};
    float ceiling{1.0F};
    std::uint64_t hash{0};
    std::uint64_t used{0};
    std::uint64_t generation{0};
    std::uint64_t refreshed{0};
    std::uint64_t checked{0};
    std::uint64_t handled{0};
    std::uint64_t zoomed{0};
    bool rendered{false};
    bool explained{false};
  };

  struct Group {
    provider::PassRegion region{};
    float scale{backdrop_scale};
    std::uint32_t levels{0};
    std::uint8_t planned{0};

    friend constexpr auto operator==(const Group&, const Group&)
        -> bool = default;
  };

  struct Consumer {
    std::uint64_t key{0};
    provider::PassRegion wanted{};
    provider::PassRegion core{};
    std::uint32_t levels{0};
    float scale{0.0F};

    friend constexpr auto operator==(const Consumer&, const Consumer&)
        -> bool = default;
  };

  struct Forming {
    std::uint64_t id{0};
    provider::PassRegion region{};
    float scale{0.0F};
    float area{0.0F};
    std::uint32_t boundary{0};
    std::uint32_t levels{0};
    std::uint8_t planned{0};
    std::size_t members{0};
  };

  struct Field {
    std::optional<provider::Texture> table;
    std::vector<FieldShape> shapes;
    std::uint64_t owner{0};
    float blend{1.0F};
    std::uint64_t used{0};
    std::uint64_t generation{0};
    std::uint64_t refreshed{0};
    bool uploaded{false};
  };

  struct Layer {
    RenderTargetPool::Lease target;
    provider::PassRegion region{};
    provider::PassRegion drawn{};
    std::array<float, 2> anchor{};
    std::array<float, 2> seen{};
    std::uint64_t hash{0};
    std::uint64_t used{0};
    std::uint64_t generation{0};
    std::uint32_t streak{0};
    bool spoiled{false};
    bool rendered{false};
  };

  struct Table {
    std::optional<provider::Texture> texture;
    std::array<float, table_capacity> values{};
    std::uint64_t used{0};
    bool uploaded{false};
  };

  struct PendingBackdrop {
    std::uint64_t key{0};
    std::uint32_t first_quad{0};
    provider::PassRegion wanted{};
    provider::PassRegion core{};
    std::uint32_t levels{0};
    float scale{0.0F};
    std::optional<std::uint64_t> group;
  };

  struct Writer {
    std::span<provider::ShapeVertex> vertices;
    std::span<provider::Index> indices;
    std::uint32_t base{0};
    std::uint32_t first{0};
  };

  static auto holds(const RenderTargetPool::Lease& lease, std::uint32_t width,
                    std::uint32_t height) noexcept -> bool;

  [[nodiscard]] auto sized(
      RenderTargetPool::Lease& lease, std::uint32_t width, std::uint32_t height,
      RenderTargetPool::Fit fit = RenderTargetPool::Fit::exact)
      -> std::expected<void, provider::ProviderError>;

  struct Refusal {
    std::string_view what;
    std::uint64_t node{0};
    std::optional<PaintErrorCode> paint;
    std::optional<provider::ProviderErrorCode> cause;

    friend constexpr auto operator==(const Refusal&, const Refusal&)
        -> bool = default;
  };

  void report(Refusal refusal, std::uint32_t width, std::uint32_t height,
              std::string_view reason, std::string_view fallback);

  static void bound(std::flat_set<std::uint64_t>& keys, std::uint64_t key);

  static auto bit(std::uint32_t levels) noexcept -> std::uint8_t;

  static auto united(const provider::PassRegion& first,
                     const provider::PassRegion& second) noexcept
      -> provider::PassRegion;

  static auto contains(const provider::PassRegion& outer,
                       const provider::PassRegion& inner) noexcept -> bool;

  static auto meets(const provider::PassRegion& first,
                    const provider::PassRegion& second) noexcept -> bool;

  static auto meets(std::span<const DrawListCache::Bounds> damage,
                    const provider::PassRegion& region) noexcept -> bool;

  static auto area(const provider::PassRegion& region) noexcept -> float;

  static auto spread(const Field& entry) noexcept -> provider::PassRegion;

  auto joined(std::uint64_t key, provider::PassRegion wanted,
              std::uint32_t levels, float scale) const
      -> std::optional<std::uint64_t>;

  auto share(std::uint64_t group, std::uint32_t levels,
             std::uint32_t first_quad) -> std::optional<Surface>;

  auto source_of(const PendingBackdrop& pending)
      -> std::optional<std::reference_wrapper<Blur>>;

  auto source_of(const PendingBackdrop& pending) const
      -> std::optional<std::reference_wrapper<const Blur>>;

  static auto output_of(const Blur& entry, std::uint32_t levels) noexcept
      -> std::optional<provider::TextureId>;

  void withdraw(const Blur& entry);

  void recycle_chain() noexcept;

  static auto same_source(const PendingBackdrop& first,
                          const PendingBackdrop& second) noexcept -> bool;

  auto reaches(std::span<const QuadSpan> spans,
               std::span<const provider::ShapeVertex> vertices,
               std::size_t index, const provider::PassRegion& region) -> bool;

  auto clear_between(std::span<const QuadSpan> spans,
                     std::span<const provider::ShapeVertex> vertices,
                     std::uint32_t from, std::uint32_t to,
                     const provider::PassRegion& core) -> bool;

  static auto quiet(std::span<const QuadSpan> spans, std::uint32_t from,
                    std::uint32_t to) noexcept -> bool;

  auto disturbed(std::span<const QuadSpan> spans,
                 std::span<const provider::ShapeVertex> vertices,
                 std::uint32_t boundary, const provider::PassRegion& region)
      -> bool;

  auto members_clear(std::uint64_t group, std::uint32_t boundary,
                     std::span<const QuadSpan> spans,
                     std::span<const provider::ShapeVertex> vertices,
                     bool whole) -> bool;

  auto unchanged(const Blur& entry, std::uint32_t boundary,
                 provider::Colour clear, std::span<const QuadSpan> spans,
                 const std::optional<DrawListCache::Changes>& damage) const
      -> bool;

  void replan(std::span<const QuadSpan> spans,
              std::span<const provider::ShapeVertex> vertices, bool whole,
              bool misgrouped);

  void form(std::span<const QuadSpan> spans,
            std::span<const provider::ShapeVertex> vertices);

  static auto captured_extent(float logical, float scale) noexcept
      -> std::uint32_t;

  static auto fitted(provider::PassRegion wanted, float scale,
                     std::uint32_t width, std::uint32_t height) noexcept
      -> provider::PassRegion;

  static auto quad(Writer& writer, const provider::ShapeVertex& shape,
                   std::array<float, 4> box, std::array<float, 4> uv)
      -> std::optional<provider::DrawCommand>;

  static auto extent(std::span<const provider::ShapeVertex> vertices) noexcept
      -> std::optional<std::array<float, 4>>;

  static auto touches(const std::optional<std::array<float, 4>>& extent,
                      const provider::PassRegion& region) noexcept -> bool;

  static auto overlaps(const provider::ClipRect& clip,
                       const provider::PassRegion& region) noexcept -> bool;

  auto hashed(std::span<const provider::DrawCommand> prefix,
              std::span<const QuadSpan> spans,
              std::span<const provider::ShapeVertex> vertices, const Blur& blur,
              provider::Colour clear) -> std::uint64_t;

  [[nodiscard]] auto blur(provider::GpuFrame& frame,
                          const provider::ShapeGeometry& geometry,
                          Writer& writer, Blur& entry,
                          std::span<const provider::DrawCommand> prefix,
                          provider::Colour clear)
      -> std::expected<void, PaintError>;

  [[nodiscard]] auto upload(Field& entry) -> std::expected<void, PaintError>;

  auto planned_layer(std::uint64_t key, const LayerRequest& request,
                     bool spoils) -> std::optional<LayerPlan>;

  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::optional<std::reference_wrapper<provider::DiagnosticsProvider>>
      diagnostics_;
  std::vector<Refusal> refused_;
  std::flat_set<std::uint64_t> rejected_backdrops_;
  std::flat_set<std::uint64_t> rejected_groups_;
  std::flat_set<std::uint64_t> rejected_fields_;
  RenderTargetPool pool_;
  std::flat_map<std::uint64_t, Blur> blurs_;
  std::flat_map<std::uint64_t, Blur> groups_;
  std::flat_map<std::uint64_t, std::uint64_t> plan_;
  std::flat_map<std::uint64_t, Group> planned_;
  std::flat_map<std::uint64_t, std::uint64_t> next_plan_;
  std::flat_map<std::uint64_t, Group> next_planned_;
  std::vector<Consumer> consumers_;
  std::vector<Consumer> seen_;
  std::vector<Forming> forming_;
  std::vector<std::pair<std::size_t, std::uint64_t>> formed_;
  std::vector<provider::PassRegion> changed_;
  std::flat_map<std::uint64_t, Field> fields_;
  std::flat_map<std::uint64_t, Layer> layers_;
  std::flat_map<std::uint64_t, Table> tables_;
  std::vector<PendingBackdrop> backdrops_;
  std::vector<std::uint64_t> pending_fields_;
  std::vector<provider::DrawCommand> prefix_;
  std::vector<Extent> extents_;
  std::vector<std::uint8_t> measured_;
  std::vector<RenderTargetPool::Lease> chain_;
  std::vector<provider::TextureId> withdrawn_;
  std::uint64_t epoch_{1};
  std::uint64_t zoomed_{0};
  Statistics statistics_;
  bool separate_{false};
  bool unsettled_{false};
  bool restless_{false};
};

}
