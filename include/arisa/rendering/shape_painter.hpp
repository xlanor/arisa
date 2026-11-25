#pragma once

#include <arisa/components/shape_view.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/theme/colour.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::bridge::nanovg {
class PathTessellator;
}

namespace arisa::rendering {

class ShapePainter {
 public:
  struct Geometry {
    std::vector<provider::ShapeVertex> vertices;
    std::vector<provider::Index> indices;
    std::optional<provider::TextureId> texture;
    float length{0.0F};
    float alpha{1.0F};
    bool trims{false};
    std::uint64_t revision{0};
    std::array<float, 2> trim{provider::DrawCommand::untrimmed};
    bool retrimmed{false};
    bool restaged{false};
    std::array<float, 4> extent{};
  };

  struct Counters {
    std::uint32_t tessellations{0};
    std::uint32_t mesh_builds{0};
  };

  struct Area {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};
    float scale{1.0F};

    friend constexpr auto operator==(const Area&, const Area&)
        -> bool = default;
  };

  struct Stop {
    theme::Colour colour;
    float location{0.0F};

    friend constexpr auto operator==(const Stop&, const Stop&)
        -> bool = default;
  };

  enum class PaintKind : std::uint8_t {
    solid,
    linear,
    radial,
    angular,
    elliptical
  };

  struct Paint {
    PaintKind kind{PaintKind::solid};
    theme::Colour colour{};
    std::vector<Stop> stops{};
    graphics::Gradient::ColorSpace space{
        graphics::Gradient::ColorSpace::device};
    std::array<float, 2> start{};
    std::array<float, 2> end{};
    float first{0.0F};
    float second{0.0F};
    std::optional<composition::NodeId> owner{};
  };

  struct Request {
    composition::NodeId node{0};
    std::shared_ptr<const components::ShapeView::Source> source;
    Area area;
    float corner_radius{0.0F};
    theme::RectangleCornerRadii corners{};
    float inset{0.0F};
    bool mirrored{false};
    std::optional<graphics::StrokeStyle> stroke;
    graphics::FillStyle fill{};
    double trim_from{0.0};
    double trim_to{1.0};
    Paint paint;
    float opacity{1.0F};
    std::shared_ptr<const graphics::Path> guide;
    float reach{0.0F};
  };

  struct MeshState {
    int width{0};
    int height{0};
    std::vector<composition::Point> points;
    std::vector<theme::Colour> colours;

    friend auto operator==(const MeshState&, const MeshState&)
        -> bool = default;
  };

  struct MeshRequest {
    MeshState state;
    bool smooth{true};
    graphics::Gradient::ColorSpace space{
        graphics::Gradient::ColorSpace::device};
    float grain{0.0F};
    Area area;
    float opacity{1.0F};

    friend auto operator==(const MeshRequest&, const MeshRequest&)
        -> bool = default;
  };

  struct Memory {
    std::uint64_t generation{0};
    MeshState from;
    MeshState to;
    MeshState shown;
    std::uint64_t used{0};
    std::optional<MeshRequest> built;
    Geometry geometry;
  };

  static constexpr std::uint32_t ramp_width = 256;
  static constexpr std::uint64_t grace_frames = 3;
  static constexpr std::uint64_t retention_frames = 600;
  static constexpr float mesh_cell = 16.0F;
  static constexpr int minimum_cells = 4;
  static constexpr int maximum_cells = 32;

  explicit ShapePainter(provider::GpuProvider& gpu);

  ShapePainter(const ShapePainter&) = delete;
  auto operator=(const ShapePainter&) -> ShapePainter& = delete;
  ShapePainter(ShapePainter&&) noexcept;
  auto operator=(ShapePainter&&) noexcept -> ShapePainter&;
  ~ShapePainter();

  void advance_frame(const std::function<bool(composition::NodeId)>& animating);

  void retain(provider::TextureId texture) noexcept;

  void retain(std::span<const std::uint64_t> sorted_textures) noexcept;

  void retain(composition::NodeId node);

  auto shape(const Request& request) -> const Geometry*;

  auto remember(composition::NodeId node, const MeshState& state) -> Memory&;

  auto mesh(const MeshRequest& request) -> Geometry;

  auto mesh(composition::NodeId node, const MeshRequest& request)
      -> const Geometry&;

  static void blend(Memory& memory, double amount);

  auto counters() const noexcept -> Counters { return counters_; }

  static auto trims_on_gpu(const Request& request) -> bool;

  auto ramp(const Paint& paint) -> std::optional<provider::TextureId>;

  auto ramp_revision(provider::TextureId texture) const noexcept
      -> std::uint64_t;

  static constexpr float guide_edge = 4.0F;

 private:
  static void tessellate(const MeshRequest& request, Geometry& geometry);

  static void reposition(const MeshRequest& request, Geometry& geometry);

  static auto same_palette(const MeshRequest& built, const MeshRequest& request)
      -> bool;

  static auto grid(const MeshRequest& request) -> std::array<int, 3>;

  struct Flattened {
    std::shared_ptr<const components::ShapeView::Source> source;
    Area area;
    float corner_radius{0.0F};
    std::vector<std::vector<std::array<float, 2>>> points;
    std::vector<bool> closed;
  };

  struct Cached {
    Request request;
    Geometry geometry;
    std::vector<std::array<float, 3>> origin;
    std::uint64_t used{0};
    bool valid{false};
  };

  struct Ramp {
    std::vector<Stop> stops;
    graphics::Gradient::ColorSpace space;
    provider::Texture texture;
    std::uint64_t used{0};
    std::uint64_t revision{0};
  };

  auto shared_ramp(const Paint& paint) -> std::optional<provider::TextureId>;

  auto owned_ramp(composition::NodeId owner, const Paint& paint)
      -> std::optional<provider::TextureId>;

  [[nodiscard]] auto made_ramp(const Paint& paint) -> std::optional<Ramp>;

  [[nodiscard]] auto paint_ramp(const Paint& paint, provider::Texture& texture)
      -> std::expected<void, provider::ProviderError>;

  static auto same(const Request& left, const Request& right) -> bool;

  static auto moved(const Request& left, const Request& right) -> bool;

  static void place(const Request& request, Cached& entry) noexcept;

  void patch(const Request& request, Geometry& geometry);

  auto colour_of(const Request& request, float alpha) const -> std::uint32_t;

  void build(const Request& request, Cached& entry);

  static auto outline(const Request& request) -> graphics::Path;

  static auto mesh_position(const MeshState& state, int column, int row,
                            float across, float down) -> composition::Point;

  static auto mesh_colour(const MeshRequest& request, int column, int row,
                          float across, float down) -> theme::Colour;

  static auto pack(theme::Colour colour, float opacity) noexcept
      -> std::uint32_t;

  static void measure(Geometry& geometry) noexcept;

  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::unique_ptr<bridge::nanovg::PathTessellator> tessellator_;
  std::map<composition::NodeId, Cached> cached_;
  std::map<composition::NodeId, Memory> memories_;
  std::vector<Ramp> ramps_;
  std::map<composition::NodeId, Ramp> owned_ramps_;
  std::vector<theme::Colour> ramp_colours_;
  std::vector<float> ramp_locations_;
  std::vector<std::byte> ramp_pixels_;
  std::uint64_t epoch_{1};
  std::uint64_t revisions_{0};
  Counters counters_;
};

}
