#pragma once

#include <arisa/graphics/path.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/support/unique_handle.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct NVGpathContext;
struct NVGvertex;

namespace arisa::bridge::nanovg {

class PathTessellator {
 public:
  struct Vertex {
    float x{0.0F};
    float y{0.0F};
    float u{0.5F};
    float v{1.0F};
    float s{0.0F};
  };

  struct Contour {
    std::vector<std::array<float, 2>> points;
    bool closed{false};

    friend auto operator==(const Contour&, const Contour&) -> bool = default;
  };

  struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    float multiplier{1.0F};
    float alpha{1.0F};
    float length{0.0F};
    float width{0.0F};
  };

  static constexpr float tolerance = 0.25F;
  static constexpr float distance = 0.01F;
  static constexpr float fringe = 1.0F;
  static constexpr float widest = 400.0F;
  static constexpr float aliased_multiplier = 1.0e4F;

  static auto create() -> std::optional<PathTessellator>;

  auto flatten(const graphics::Path& path, float scale, float origin_x,
               float origin_y) -> std::vector<Contour>;

  auto stroke(std::span<const Contour> contours,
              const graphics::StrokeStyle& style, float scale,
              bool antialiased = true) -> Mesh;

  auto fill(std::span<const Contour> contours, graphics::FillStyle style)
      -> Mesh;

  static auto length(std::span<const Contour> contours) -> double;

  static void follow(Mesh& mesh, std::span<const Contour> guide, float edge);

  static auto trim(std::span<const Contour> contours, double from, double to)
      -> std::vector<Contour>;

  static auto dash(std::span<const Contour> contours,
                   std::span<const float> pattern, float phase)
      -> std::vector<Contour>;

 private:
  struct ContextTraits {
    using handle_type = NVGpathContext*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != nullptr;
    }

    static void release(handle_type& handle) noexcept;
  };

  explicit PathTessellator(NVGpathContext* context) noexcept
      : context_(context) {}

  void load(std::span<const Contour> contours, bool enforce);

  static void strip(const NVGvertex* first, int count, Mesh& mesh);

  static void fan(const NVGvertex* first, int count, Mesh& mesh);

  void fringes(Mesh& mesh);

  auto tessellate(std::span<const Contour> contours, bool even_odd,
                  bool boundary) -> std::optional<std::vector<Contour>>;

  static auto contour_length(const Contour& contour) -> double;

  static void measure(const Contour& contour, double offset,
                      std::span<Vertex> vertices);

  static void split(Mesh& mesh, float edge);

  static auto area(const Contour& contour) -> double;

  static auto simple(const Contour& contour) -> bool;

  static void cut(const Contour& contour, double from, double to,
                  std::vector<Contour>& into);

  support::UniqueHandle<ContextTraits> context_;
};

}
