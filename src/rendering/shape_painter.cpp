#include <arisa/rendering/shape_painter.hpp>

#include <bridge/nanovg/path_tessellator.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::rendering {

ShapePainter::ShapePainter(provider::GpuProvider& gpu)
    : gpu_(gpu),
      tessellator_([]() -> std::unique_ptr<bridge::nanovg::PathTessellator> {
        auto made = bridge::nanovg::PathTessellator::create();
        if (!made.has_value()) {
          return nullptr;
        }
        return std::make_unique<bridge::nanovg::PathTessellator>(
            std::move(*made));
      }()) {}

ShapePainter::ShapePainter(ShapePainter&&) noexcept = default;

auto ShapePainter::operator=(ShapePainter&&) noexcept
    -> ShapePainter& = default;

ShapePainter::~ShapePainter() = default;

void ShapePainter::advance_frame(
    const std::function<bool(composition::NodeId)>& animating) {
  ++epoch_;
  counters_ = Counters{};
  const std::uint64_t limit = epoch_ > grace_frames ? epoch_ - grace_frames : 0;
  const std::uint64_t kept =
      epoch_ > retention_frames ? epoch_ - retention_frames : 0;
  std::erase_if(cached_,
                [kept](const auto& entry) { return entry.second.used < kept; });
  std::erase_if(memories_, [limit, &animating](auto& entry) {
    if (entry.second.used >= limit) {
      return false;
    }
    if (animating(entry.first)) {
      entry.second.used = limit;
      return false;
    }
    return true;
  });
  std::erase_if(ramps_,
                [limit](const Ramp& ramp) { return ramp.used < limit; });
  std::erase_if(owned_ramps_, [limit](const auto& entry) {
    return entry.second.used < limit;
  });
}

void ShapePainter::retain(provider::TextureId texture) noexcept {
  for (Ramp& ramp : ramps_) {
    if (ramp.texture.id() == texture) {
      ramp.used = epoch_;
    }
  }
  for (auto& [owner, ramp] : owned_ramps_) {
    if (ramp.texture.id() == texture) {
      ramp.used = epoch_;
    }
  }
}

void ShapePainter::retain(
    std::span<const std::uint64_t> sorted_textures) noexcept {
  for (Ramp& ramp : ramps_) {
    if (std::ranges::binary_search(sorted_textures,
                                   ramp.texture.id().value())) {
      ramp.used = epoch_;
    }
  }
  for (auto& [owner, ramp] : owned_ramps_) {
    if (std::ranges::binary_search(sorted_textures,
                                   ramp.texture.id().value())) {
      ramp.used = epoch_;
    }
  }
}

void ShapePainter::retain(composition::NodeId node) {
  if (const auto found = memories_.find(node); found != memories_.end()) {
    found->second.used = epoch_;
  }
}

auto ShapePainter::pack(theme::Colour colour, float opacity) noexcept
    -> std::uint32_t {
  const auto byte = [](float value) {
    return static_cast<std::uint32_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  return byte(colour.red) | (byte(colour.green) << 8U) |
         (byte(colour.blue) << 16U) | (byte(colour.alpha * opacity) << 24U);
}

auto ShapePainter::trims_on_gpu(const Request& request) -> bool {
  const bool stroked =
      request.stroke.has_value() && request.stroke->dash.empty();
  const bool guided = !request.stroke.has_value() && request.guide != nullptr;
  return (stroked || guided) && (request.paint.kind == PaintKind::solid ||
                                 request.paint.kind == PaintKind::linear);
}

auto ShapePainter::same(const Request& left, const Request& right) -> bool {
  const bool sources = left.source == right.source ||
                       (left.source != nullptr && right.source != nullptr &&
                        *left.source == *right.source);
  const bool gpu = trims_on_gpu(left) && trims_on_gpu(right);
  const bool trims = gpu || (left.trim_from == right.trim_from &&
                             left.trim_to == right.trim_to);
  const bool guides = left.guide == right.guide ||
                      (left.guide != nullptr && right.guide != nullptr &&
                       *left.guide == *right.guide);
  const Area& was = left.area;
  const Area& now = right.area;
  return sources && was.right - was.left == now.right - now.left &&
         was.bottom - was.top == now.bottom - now.top &&
         was.scale == now.scale && left.corner_radius == right.corner_radius &&
         left.corners == right.corners && left.inset == right.inset &&
         left.mirrored == right.mirrored && left.stroke == right.stroke &&
         left.fill == right.fill && trims && guides &&
         left.reach == right.reach && left.paint.kind == right.paint.kind &&
         left.paint.colour == right.paint.colour;
}

auto ShapePainter::moved(const Request& left, const Request& right) -> bool {
  return left.area.left != right.area.left || left.area.top != right.area.top ||
         left.paint.start != right.paint.start ||
         left.paint.end != right.paint.end ||
         left.paint.first != right.paint.first ||
         left.paint.second != right.paint.second;
}

auto ShapePainter::colour_of(const Request& request, float alpha) const
    -> std::uint32_t {
  const float opacity = request.opacity * alpha;
  return request.paint.kind == PaintKind::solid
             ? pack(request.paint.colour, opacity)
             : pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, opacity);
}

void ShapePainter::patch(const Request& request, Geometry& geometry) {
  const std::uint32_t colour = colour_of(request, geometry.alpha);
  std::array<float, 2> trim = provider::DrawCommand::untrimmed;
  if (geometry.trims) {
    if (request.trim_from > 0.0) {
      trim[0] = static_cast<float>(std::clamp(request.trim_from, 0.0, 1.0)) *
                geometry.length;
    }
    if (request.trim_to < 1.0) {
      trim[1] = static_cast<float>(std::clamp(request.trim_to, 0.0, 1.0)) *
                geometry.length;
    }
  }
  geometry.retrimmed = geometry.trim != trim;
  geometry.trim = trim;
  if (!geometry.vertices.empty() &&
      geometry.vertices.front().colour == colour) {
    return;
  }
  for (provider::ShapeVertex& vertex : geometry.vertices) {
    vertex.colour = colour;
  }
  geometry.revision = ++revisions_;
}

auto ShapePainter::ramp(const Paint& paint)
    -> std::optional<provider::TextureId> {
  return paint.owner.has_value() ? owned_ramp(*paint.owner, paint)
                                 : shared_ramp(paint);
}

auto ShapePainter::ramp_revision(provider::TextureId texture) const noexcept
    -> std::uint64_t {
  for (const auto& [owner, ramp] : owned_ramps_) {
    if (ramp.texture.id() == texture) {
      return ramp.revision;
    }
  }
  return 0;
}

auto ShapePainter::shared_ramp(const Paint& paint)
    -> std::optional<provider::TextureId> {
  const auto found = std::ranges::find_if(ramps_, [&paint](const Ramp& each) {
    return each.stops == paint.stops && each.space == paint.space;
  });
  if (found != ramps_.end()) {
    found->used = epoch_;
    return found->texture.id();
  }
  std::optional<Ramp> made = made_ramp(paint);
  if (!made.has_value()) {
    return std::nullopt;
  }
  ramps_.push_back(std::move(*made));
  return ramps_.back().texture.id();
}

auto ShapePainter::owned_ramp(composition::NodeId owner, const Paint& paint)
    -> std::optional<provider::TextureId> {
  const auto found = owned_ramps_.find(owner);
  if (found == owned_ramps_.end()) {
    std::optional<Ramp> made = made_ramp(paint);
    if (!made.has_value()) {
      return std::nullopt;
    }
    made->revision = ++revisions_;
    return owned_ramps_.insert_or_assign(owner, std::move(*made))
        .first->second.texture.id();
  }
  Ramp& held = found->second;
  held.used = epoch_;
  if (held.stops == paint.stops && held.space == paint.space) {
    return held.texture.id();
  }
  if (paint.stops.empty() || !paint_ramp(paint, held.texture).has_value()) {
    owned_ramps_.erase(found);
    return std::nullopt;
  }
  held.stops.assign(paint.stops.begin(), paint.stops.end());
  held.space = paint.space;
  held.revision = ++revisions_;
  return held.texture.id();
}

auto ShapePainter::made_ramp(const Paint& paint) -> std::optional<Ramp> {
  if (paint.stops.empty()) {
    return std::nullopt;
  }
  auto texture = gpu_.get().create_texture(
      provider::TextureDescription{.width = ramp_width,
                                   .height = 1,
                                   .format = provider::TextureFormat::rgba8});
  if (!texture.has_value() || !paint_ramp(paint, *texture).has_value()) {
    return std::nullopt;
  }
  return Ramp{.stops = paint.stops,
              .space = paint.space,
              .texture = std::move(*texture),
              .used = epoch_};
}

auto ShapePainter::paint_ramp(const Paint& paint, provider::Texture& texture)
    -> std::expected<void, provider::ProviderError> {
  ramp_colours_.clear();
  ramp_locations_.clear();
  for (const Stop& stop : paint.stops) {
    ramp_colours_.push_back(stop.colour);
    ramp_locations_.push_back(stop.location);
  }
  ramp_pixels_.resize(static_cast<std::size_t>(ramp_width) * 4);
  for (std::uint32_t index = 0; index < ramp_width; ++index) {
    const float position =
        static_cast<float>(index) / static_cast<float>(ramp_width - 1);
    const theme::Colour colour = graphics::Gradient::sample(
        ramp_colours_, ramp_locations_, paint.space, position);
    const std::uint32_t packed = pack(colour, 1.0F);
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
      ramp_pixels_[(static_cast<std::size_t>(index) * 4) + channel] =
          static_cast<std::byte>((packed >> (channel * 8U)) & 0xffU);
    }
  }
  return gpu_.get().update_texture(
      texture,
      provider::TextureRegion{.x = 0, .y = 0, .width = ramp_width, .height = 1},
      provider::PixelRows{ramp_pixels_});
}

auto ShapePainter::shape(const Request& request) -> const Geometry* {
  if (tessellator_ == nullptr || request.source == nullptr) {
    return nullptr;
  }
  const bool hidden =
      trims_on_gpu(request) && request.trim_to <= request.trim_from;
  Cached& entry = cached_[request.node];
  entry.used = epoch_;
  if (entry.valid && same(entry.request, request)) {
    const bool shifted = moved(entry.request, request);
    if (shifted) {
      place(request, entry);
      entry.geometry.revision = ++revisions_;
    }
    if (request.paint.kind != PaintKind::solid) {
      const std::optional<provider::TextureId> texture = ramp(request.paint);
      if (texture != entry.geometry.texture) {
        entry.geometry.texture = texture;
        entry.geometry.revision = ++revisions_;
      }
    }
    if (entry.request.trim_from != request.trim_from ||
        entry.request.trim_to != request.trim_to ||
        entry.request.opacity != request.opacity) {
      entry.request = request;
      patch(request, entry.geometry);
    } else {
      if (shifted) {
        entry.request = request;
      }
      entry.geometry.retrimmed = false;
    }
    return hidden ? nullptr : &entry.geometry;
  }
  entry.request = request;
  build(request, entry);
  entry.geometry.revision = ++revisions_;
  ++counters_.tessellations;
  patch(request, entry.geometry);
  entry.geometry.retrimmed = true;
  entry.valid = true;
  return hidden ? nullptr : &entry.geometry;
}

auto ShapePainter::outline(const Request& request) -> graphics::Path {
  const Area& area = request.area;
  const composition::Rect local{
      .x = 0.0F,
      .y = 0.0F,
      .width = (area.right - area.left) / area.scale,
      .height = (area.bottom - area.top) / area.scale};
  graphics::Path drawn = std::visit(
      [&](const auto& each) -> graphics::Path {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, graphics::Path>) {
          return each;
        } else if constexpr (std::same_as<T, theme::Shape>) {
          if (request.inset == 0.0F && each.form == theme::ShapeForm::rounded &&
              !each.elliptical()) {
            return graphics::Path::shaped(each, local, request.corner_radius);
          }
          return graphics::AnyShape{each}
              .inset(request.inset)
              .path(local, request.corners);
        } else {
          const graphics::AnyShape& shape = each;
          return request.inset == 0.0F
                     ? shape.path(local, request.corners)
                     : shape.inset(request.inset).path(local, request.corners);
        }
      },
      *request.source);
  if (!request.mirrored) {
    return drawn;
  }
  return drawn.applying(
      graphics::AffineTransform{-1.0F, 0.0F, 0.0F, 1.0F, local.width, 0.0F});
}

void ShapePainter::build(const Request& request, Cached& entry) {
  using Tessellator = bridge::nanovg::PathTessellator;
  Geometry& geometry = entry.geometry;
  geometry = Geometry{};
  entry.origin.clear();
  const float scale = request.area.scale;
  const graphics::Path path = outline(request);
  std::vector<Tessellator::Contour> contours =
      tessellator_->flatten(path, scale, 0.0F, 0.0F);
  Tessellator::Mesh mesh;
  const bool gpu = trims_on_gpu(request);
  if (request.stroke.has_value()) {
    if (!gpu && (request.trim_from > 0.0 || request.trim_to < 1.0)) {
      contours =
          Tessellator::trim(contours, request.trim_from, request.trim_to);
    }
    if (!request.stroke->dash.empty()) {
      std::vector<float> pattern;
      for (const float each : request.stroke->dash) {
        pattern.push_back(each * scale);
      }
      contours = Tessellator::dash(contours, pattern,
                                   request.stroke->dash_phase * scale);
    }
    mesh = tessellator_->stroke(contours, *request.stroke, scale,
                                request.fill.antialiased);
  } else if (gpu) {
    mesh = tessellator_->fill(contours, request.fill);
    const std::vector<Tessellator::Contour> guide =
        tessellator_->flatten(*request.guide, scale, 0.0F, 0.0F);
    Tessellator::follow(mesh, guide, guide_edge * scale);
    mesh.width = request.reach * scale;
  } else {
    if (request.trim_from > 0.0 || request.trim_to < 1.0) {
      contours =
          Tessellator::trim(contours, request.trim_from, request.trim_to);
      for (Tessellator::Contour& contour : contours) {
        contour.closed = true;
      }
    }
    mesh = tessellator_->fill(contours, request.fill);
  }
  if (mesh.indices.empty()) {
    return;
  }
  const Paint& paint = request.paint;
  geometry.alpha = mesh.alpha;
  geometry.length = mesh.length;
  geometry.trims = gpu;
  provider::ShapeKind kind = provider::ShapeKind::path;
  const std::uint32_t colour = colour_of(request, mesh.alpha);
  float mode = 0.0F;
  if (paint.kind != PaintKind::solid) {
    geometry.texture = ramp(paint);
    if (!geometry.texture.has_value()) {
      return;
    }
    kind = provider::ShapeKind::gradient;
    mode = paint.kind == PaintKind::linear    ? 0.0F
           : paint.kind == PaintKind::angular ? 2.0F
                                              : 1.0F;
  }
  geometry.vertices.reserve(mesh.vertices.size());
  entry.origin.reserve(mesh.vertices.size());
  for (const Tessellator::Vertex& vertex : mesh.vertices) {
    entry.origin.push_back({vertex.x, vertex.y, vertex.s});
    geometry.vertices.push_back(provider::ShapeVertex{
        .position = {},
        .uv = {vertex.u, vertex.v},
        .local = {},
        .half_size =
            gpu ? std::array<float, 2>{mesh.width,
                                       request.guide != nullptr ? 1.0F : 0.0F}
                : std::array<float, 2>{},
        .shape = {mesh.multiplier, mode, provider::DrawCommand::untrimmed[0],
                  provider::DrawCommand::untrimmed[1]},
        .colour = colour,
        .kind = kind});
  }
  geometry.indices.assign(mesh.indices.begin(), mesh.indices.end());
  place(request, entry);
}

void ShapePainter::place(const Request& request, Cached& entry) noexcept {
  const Paint& paint = request.paint;
  const Area& area = request.area;
  Geometry& geometry = entry.geometry;
  const float dx = paint.end[0] - paint.start[0];
  const float dy = paint.end[1] - paint.start[1];
  const float span = (dx * dx) + (dy * dy);
  const bool half = !geometry.trims && paint.kind != PaintKind::solid;
  for (std::size_t index = 0; index < geometry.vertices.size(); ++index) {
    const auto [left, top, along] = entry.origin[index];
    const float x = area.left + left;
    const float y = area.top + top;
    provider::ShapeVertex& vertex = geometry.vertices[index];
    vertex.position = {x, y};
    if (half) {
      vertex.half_size = {paint.first, paint.second};
    }
    if (paint.kind == PaintKind::linear) {
      vertex.local = {span > 0.0F ? (((x - paint.start[0]) * dx) +
                                     ((y - paint.start[1]) * dy)) /
                                        span
                                  : 0.0F,
                      along};
    } else if (paint.kind == PaintKind::solid) {
      vertex.local = {0.0F, along};
    } else if (paint.kind == PaintKind::elliptical) {
      vertex.local = {(x - paint.start[0]) / std::max(paint.end[0], 1.0e-4F),
                      (y - paint.start[1]) / std::max(paint.end[1], 1.0e-4F)};
    } else {
      vertex.local = {(x - paint.start[0]) / area.scale,
                      (y - paint.start[1]) / area.scale};
    }
  }
  measure(geometry);
}

void ShapePainter::measure(Geometry& geometry) noexcept {
  if (geometry.vertices.empty()) {
    geometry.extent = {};
    return;
  }
  const std::array<float, 2>& first = geometry.vertices.front().position;
  std::array<float, 4> box{first[0], first[1], first[0], first[1]};
  for (const provider::ShapeVertex& vertex : geometry.vertices) {
    box[0] = std::min(box[0], vertex.position[0]);
    box[1] = std::min(box[1], vertex.position[1]);
    box[2] = std::max(box[2], vertex.position[0]);
    box[3] = std::max(box[3], vertex.position[1]);
  }
  geometry.extent = box;
}

auto ShapePainter::remember(composition::NodeId node, const MeshState& state)
    -> Memory& {
  Memory& memory = memories_[node];
  memory.used = epoch_;
  if (memory.generation == 0) {
    memory.generation = 1;
    memory.from = state;
    memory.to = state;
    memory.shown = state;
  } else if (memory.to != state) {
    memory.from = memory.shown;
    memory.to = state;
    ++memory.generation;
  }
  return memory;
}

void ShapePainter::blend(Memory& memory, double amount) {
  const MeshState& from = memory.from;
  const MeshState& to = memory.to;
  MeshState& mixed = memory.shown;
  mixed.width = to.width;
  mixed.height = to.height;
  mixed.points.assign(to.points.begin(), to.points.end());
  mixed.colours.assign(to.colours.begin(), to.colours.end());
  if (amount >= 1.0 || from.width != to.width || from.height != to.height ||
      from.points.size() != to.points.size() ||
      from.colours.size() != to.colours.size()) {
    return;
  }
  const auto share = static_cast<float>(std::clamp(amount, 0.0, 1.0));
  for (std::size_t index = 0; index < mixed.points.size(); ++index) {
    mixed.points[index] = composition::Point{
        .x = from.points[index].x +
             ((to.points[index].x - from.points[index].x) * share),
        .y = from.points[index].y +
             ((to.points[index].y - from.points[index].y) * share)};
  }
  for (std::size_t index = 0; index < mixed.colours.size(); ++index) {
    mixed.colours[index] =
        graphics::Gradient::mix(from.colours[index], to.colours[index], share,
                                graphics::Gradient::ColorSpace::device);
  }
}

auto ShapePainter::mesh_position(const MeshState& state, int column, int row,
                                 float across, float down)
    -> composition::Point {
  const auto at = [&state](int x, int y) {
    return state.points[static_cast<std::size_t>((y * state.width) + x)];
  };
  const auto tangent = [&](int x, int y, bool horizontal) {
    const int low = std::max((horizontal ? x : y) - 1, 0);
    const int high = std::min((horizontal ? x : y) + 1,
                              (horizontal ? state.width : state.height) - 1);
    const composition::Point a = horizontal ? at(low, y) : at(x, low);
    const composition::Point b = horizontal ? at(high, y) : at(x, high);
    const auto steps = static_cast<float>(high - low);
    return composition::Point{.x = (b.x - a.x) / steps,
                              .y = (b.y - a.y) / steps};
  };
  const auto hermite = [](composition::Point p0, composition::Point p1,
                          composition::Point m0, composition::Point m1,
                          float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float h00 = (2.0F * t3) - (3.0F * t2) + 1.0F;
    const float h10 = t3 - (2.0F * t2) + t;
    const float h01 = (-2.0F * t3) + (3.0F * t2);
    const float h11 = t3 - t2;
    return composition::Point{
        .x = (h00 * p0.x) + (h10 * m0.x) + (h01 * p1.x) + (h11 * m1.x),
        .y = (h00 * p0.y) + (h10 * m0.y) + (h01 * p1.y) + (h11 * m1.y)};
  };
  const composition::Point top =
      hermite(at(column, row), at(column + 1, row), tangent(column, row, true),
              tangent(column + 1, row, true), across);
  const composition::Point bottom =
      hermite(at(column, row + 1), at(column + 1, row + 1),
              tangent(column, row + 1, true),
              tangent(column + 1, row + 1, true), across);
  const auto mixed = [across](composition::Point a, composition::Point b) {
    return composition::Point{.x = a.x + ((b.x - a.x) * across),
                              .y = a.y + ((b.y - a.y) * across)};
  };
  return hermite(
      top, bottom,
      mixed(tangent(column, row, false), tangent(column + 1, row, false)),
      mixed(tangent(column, row + 1, false),
            tangent(column + 1, row + 1, false)),
      down);
}

auto ShapePainter::mesh_colour(const MeshRequest& request, int column, int row,
                               float across, float down) -> theme::Colour {
  const MeshState& state = request.state;
  const auto at = [&state](int x, int y) {
    return state.colours[static_cast<std::size_t>((y * state.width) + x)];
  };
  const auto ease = [&request](float t) {
    return request.smooth ? t * t * (3.0F - (2.0F * t)) : t;
  };
  const graphics::Gradient::ColorSpace space =
      request.space == graphics::Gradient::ColorSpace::device
          ? graphics::Gradient::ColorSpace::device
          : graphics::Gradient::ColorSpace::perceptual;
  const float u = ease(across);
  const float v = ease(down);
  const theme::Colour top =
      graphics::Gradient::mix(at(column, row), at(column + 1, row), u, space);
  const theme::Colour bottom = graphics::Gradient::mix(
      at(column, row + 1), at(column + 1, row + 1), u, space);
  return graphics::Gradient::mix(top, bottom, v, space);
}

auto ShapePainter::mesh(composition::NodeId node, const MeshRequest& request)
    -> const Geometry& {
  Memory& memory = memories_[node];
  memory.used = epoch_;
  if (!memory.built.has_value() || *memory.built != request) {
    if (memory.built.has_value() && same_palette(*memory.built, request)) {
      reposition(request, memory.geometry);
    } else {
      tessellate(request, memory.geometry);
    }
    memory.geometry.revision = ++revisions_;
    memory.built = request;
    ++counters_.mesh_builds;
  }
  return memory.geometry;
}

auto ShapePainter::same_palette(const MeshRequest& built,
                                const MeshRequest& request) -> bool {
  return built.state.width == request.state.width &&
         built.state.height == request.state.height &&
         built.state.points.size() == request.state.points.size() &&
         built.state.colours == request.state.colours &&
         built.smooth == request.smooth && built.space == request.space &&
         built.grain == request.grain && built.area == request.area &&
         built.opacity == request.opacity;
}

auto ShapePainter::grid(const MeshRequest& request) -> std::array<int, 3> {
  const MeshState& state = request.state;
  const Area& area = request.area;
  const float width = area.right - area.left;
  const float height = area.bottom - area.top;
  const float patch = std::max(width / static_cast<float>(state.width - 1),
                               height / static_cast<float>(state.height - 1));
  const int cells = std::clamp(static_cast<int>(std::ceil(patch / mesh_cell)),
                               minimum_cells, maximum_cells);
  return {cells, ((state.width - 1) * cells) + 1,
          ((state.height - 1) * cells) + 1};
}

void ShapePainter::reposition(const MeshRequest& request, Geometry& geometry) {
  const MeshState& state = request.state;
  const Area& area = request.area;
  const float width = area.right - area.left;
  const float height = area.bottom - area.top;
  const auto [cells, columns, rows] = grid(request);
  if (geometry.vertices.size() != static_cast<std::size_t>(columns * rows)) {
    tessellate(request, geometry);
    return;
  }
  for (int y = 0; y < rows; ++y) {
    const int row = std::min(y / cells, state.height - 2);
    const float down =
        static_cast<float>(y - (row * cells)) / static_cast<float>(cells);
    for (int x = 0; x < columns; ++x) {
      const int column = std::min(x / cells, state.width - 2);
      const float across =
          static_cast<float>(x - (column * cells)) / static_cast<float>(cells);
      const composition::Point unit =
          mesh_position(state, column, row, across, down);
      geometry.vertices[static_cast<std::size_t>((y * columns) + x)].position =
          {area.left + (unit.x * width), area.top + (unit.y * height)};
    }
  }
  measure(geometry);
}

auto ShapePainter::mesh(const MeshRequest& request) -> Geometry {
  Geometry geometry;
  tessellate(request, geometry);
  return geometry;
}

void ShapePainter::tessellate(const MeshRequest& request, Geometry& geometry) {
  geometry.vertices.clear();
  geometry.indices.clear();
  geometry.texture.reset();
  geometry.length = 0.0F;
  geometry.alpha = 1.0F;
  geometry.trims = false;
  const MeshState& state = request.state;
  if (state.width < 2 || state.height < 2 ||
      state.points.size() !=
          static_cast<std::size_t>(state.width * state.height) ||
      state.colours.size() != state.points.size()) {
    return;
  }
  const Area& area = request.area;
  const float width = area.right - area.left;
  const float height = area.bottom - area.top;
  const auto [cells, columns, rows] = grid(request);
  geometry.vertices.reserve(static_cast<std::size_t>(columns * rows));
  for (int y = 0; y < rows; ++y) {
    const int row = std::min(y / cells, state.height - 2);
    const float down =
        static_cast<float>(y - (row * cells)) / static_cast<float>(cells);
    for (int x = 0; x < columns; ++x) {
      const int column = std::min(x / cells, state.width - 2);
      const float across =
          static_cast<float>(x - (column * cells)) / static_cast<float>(cells);
      const composition::Point unit =
          mesh_position(state, column, row, across, down);
      geometry.vertices.push_back(provider::ShapeVertex{
          .position = {area.left + (unit.x * width),
                       area.top + (unit.y * height)},
          .uv = {0.5F, 1.0F},
          .local = {},
          .half_size = {},
          .shape = {request.grain, 0.0F, 0.0F, 0.0F},
          .colour = pack(mesh_colour(request, column, row, across, down),
                         request.opacity),
          .kind = provider::ShapeKind::mesh});
    }
  }
  geometry.indices.reserve(
      static_cast<std::size_t>((columns - 1) * (rows - 1) * 6));
  for (int y = 0; y + 1 < rows; ++y) {
    for (int x = 0; x + 1 < columns; ++x) {
      const auto a = static_cast<provider::Index>((y * columns) + x);
      const auto b = a + 1;
      const auto c = a + static_cast<provider::Index>(columns);
      const auto d = c + 1;
      geometry.indices.insert(geometry.indices.end(), {a, b, d, d, c, a});
    }
  }
  measure(geometry);
}

}
