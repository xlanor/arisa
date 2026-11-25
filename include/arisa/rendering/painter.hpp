#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text_glyphs.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_effect_phase_override.hpp>
#include <arisa/composition/hover_path.hpp>
#include <arisa/composition/link_frame.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/presented_clip.hpp>
#include <arisa/composition/presented_frame.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/composition/scroll_edge_effect_style.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotation_3d.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shadow_style.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/style_hierarchy.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/unhinted_animation.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/gpu_frame.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/input/stick.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/rendering/compositor.hpp>
#include <arisa/rendering/draw_list_cache.hpp>
#include <arisa/rendering/glass_lens.hpp>
#include <arisa/rendering/paint_error.hpp>
#include <arisa/rendering/replay_scene.hpp>
#include <arisa/rendering/shape_painter.hpp>
#include <arisa/support/full_order_walk.hpp>
#include <arisa/support/strong_value.hpp>
#include <arisa/text/atlas_options.hpp>
#include <arisa/text/fitted_text.hpp>
#include <arisa/text/glyph_atlas.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/positioned_glyph.hpp>
#include <arisa/text/text_fitting.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_run.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typesetter.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::rendering {

class Painter {
 public:
  using TypesetterRef = std::optional<std::reference_wrapper<text::Typesetter>>;

  struct Target {
    provider::SurfaceSize pixels{};
    provider::SurfaceScale scale{provider::SurfaceScale::unit()};
    std::chrono::nanoseconds time{0};
    float zoom{1.0F};
    std::optional<theme::Colour> clear{};
    std::optional<std::reference_wrapper<ReplayScene>> replay{};
    std::optional<std::reference_wrapper<const provider::ClockProvider>>
        clock{};
  };

  struct Shake {
    composition::NodeId node{0};
    composition::MoveCommandDirection direction{
        composition::MoveCommandDirection::right};
    std::chrono::nanoseconds started{0};

    friend auto operator==(const Shake&, const Shake&) -> bool = default;
  };

  struct Highlights {
    std::optional<composition::NodeId> focused{};
    std::optional<composition::NodeId> pressed{};
    provider::Stick tilt{};
    bool focus_visible{true};
    std::optional<Shake> shake{};
    std::optional<provider::PixelPoint> contact{};
    std::optional<composition::NodeId> touched{};
    std::shared_ptr<const composition::HoverPath> hovering{};
  };

  struct Statistics {
    std::uint32_t quads{0};
    std::uint32_t commands{0};
    bool animating{false};
    std::optional<double> frame_rate{};
    std::uint32_t culled{0};
    std::uint32_t visited{0};
    std::uint32_t recorded{0};
    std::uint32_t spliced{0};
    std::uint32_t occluded{0};
    std::uint32_t shadow_hits{0};
    std::uint32_t shadow_misses{0};
    std::uint32_t tessellations{0};
    std::uint32_t mesh_builds{0};
    std::uint32_t backdrops{0};
    std::uint32_t blurs{0};
    std::uint32_t blur_hits{0};
    std::uint32_t shared_backdrops{0};
    std::uint32_t effect_passes{0};
    std::uint32_t layer_renders{0};
    std::uint32_t layer_hits{0};
    std::uint32_t uploaded_vertices{0};
    std::uint64_t uploaded_bytes{0};
    std::uint64_t damaged_pixels{0};
    std::optional<provider::ClipRect> damage{};
    std::optional<motion::UnhintedAnimation> unhinted{};
    std::chrono::nanoseconds walk{0};
    std::chrono::nanoseconds upload{0};
    std::chrono::nanoseconds emit{0};
    std::uint32_t pictures{0};
    std::uint64_t picture_bytes{0};
    std::uint32_t targets_created{0};
    std::uint32_t targets_reused{0};
    std::uint32_t targets_released{0};
    std::uint32_t targets_evicted{0};
    std::uint32_t targets_refused{0};
  };

  struct PictureBudgetTag;
  using PictureBudget = support::StrongValue<std::uint64_t, PictureBudgetTag>;

  struct Pictures {
    std::size_t count{0};
    std::uint64_t bytes{0};
    std::uint64_t budget{0};
    std::uint64_t uploads{0};
    std::uint64_t evictions{0};

    friend constexpr auto operator==(const Pictures&, const Pictures&)
        -> bool = default;
  };

  static constexpr PictureBudget default_picture_budget{64ULL * 1024ULL *
                                                        1024ULL};
  static constexpr std::uint32_t picture_grace_frames = 3;
  static constexpr float redaction_opacity = 0.16F;
  static constexpr float redaction_bar_height = 0.8F;
  static constexpr float redaction_radius = 4.0F;

  static constexpr float continuous_smoothing = 0.6F;
  static constexpr std::array<float, 4> whole_texture{0.0F, 0.0F, 1.0F, 1.0F};
  static constexpr float focus_ring_width = theme::metrics::focus_ring.width;
  static constexpr float regular_glass_alpha = 0.78F;
  static constexpr float clear_glass_alpha = 0.45F;
  static constexpr float see_through_clear_alpha = 0.2F;
  static constexpr float see_through_regular_alpha = 0.42F;
  static constexpr float fallback_stroke_alpha = 0.55F;
  static constexpr std::uint32_t fallback_stroke_mark = 0x666c6174U;
  static constexpr float glass_highlight = 0.15F;
  static constexpr float solid_glass_alpha = 0.94F;
  static constexpr float focus_ring_gap =
      theme::metrics::focus_ring.outset - theme::metrics::focus_ring.width;
  static constexpr float focus_ring_seal = 1.0F;
  static constexpr float focus_ring_halo = 2.0F;
  static constexpr float dark_shadow_boost = 1.6F;
  static constexpr float drop_shadow_spread = 1.94F;
  static constexpr float numeric_rise = 0.6F;
  static constexpr float symbol_rise = 0.35F;
  static constexpr float visible_threshold = 2.0e-3F;
  static constexpr float cull_margin = 0.25F;
  static constexpr float shake_amplitude = 7.0F;
  static constexpr double shake_cycles = 3.0;
  static constexpr std::chrono::milliseconds shake_duration{320};
  static constexpr float indicator_thickness = 4.0F;
  static constexpr float indicator_inset = 3.0F;
  static constexpr float indicator_minimum = 28.0F;
  static constexpr float indicator_opacity = 0.55F;
  static constexpr float shadow_quantum = 4.0F;
  static constexpr float maximum_patch = 160.0F;
  static constexpr float fill_contrast_light = 0.035F;
  static constexpr float ellipse_marker = -1.0F;
  static constexpr std::size_t style_limit = 256;
  static constexpr float fill_contrast_dark = 0.035F;
  static constexpr float border_contrast_light = 0.035F;
  static constexpr float border_contrast_dark = 0.035F;
  static constexpr float text_contrast = 0.19F;
  static constexpr float same_surface = 2.0e-3F;
  static constexpr float same_extent = 0.5F;
  static constexpr std::size_t reported_limit = 4096;
  static constexpr float highlight_rise = 0.4F;
  static constexpr float union_margin = 2.0F;
  static constexpr float transform_quantum = 256.0F;
  static constexpr float replay_margin = 1.5F;
  static constexpr float maximum_gaussian = 4.0F;
  static constexpr std::uint32_t maximum_reductions = 3;
  static constexpr std::uint32_t projection_cells = 16;
  static constexpr float extension_blur = 24.0F;
  static constexpr std::uint8_t maximum_reduction = 6;
  static constexpr std::uint64_t effect_mask_salt = 0x6d61736bULL;
  static constexpr std::uint64_t effect_inverse_mask_salt = 0x696d61736bULL;
  static constexpr std::uint64_t shape_pass_salt = 0x7061737365ULL << 24U;
  static constexpr std::uint64_t clip_mask_salt = 0x4000000000000000ULL;
  static constexpr std::uint64_t clip_source_frames = 600;
  static constexpr std::uint64_t effect_reduce_salt = 0x72656475ULL;
  static constexpr std::uint64_t effect_gaussian_salt = 0x67617573ULL;
  static constexpr std::uint64_t effect_matrix_salt = 0x6d617472ULL;
  static constexpr std::uint64_t effect_table_salt = 0x7461626cULL;
  static constexpr std::uint64_t effect_blend_salt = 0x626c656eULL;
  static constexpr std::uint64_t effect_shader_salt = 0x73686472ULL;
  static constexpr std::size_t shader_arguments = 9;
#ifdef NDEBUG
  static constexpr bool checks_contrast = false;
#else
  static constexpr bool checks_contrast = true;
#endif

  enum class Contrast : std::uint8_t { fill, border, text };

  [[nodiscard]] static auto create(provider::GpuProvider& gpu,
                                   text::AtlasOptions options = {})
      -> std::expected<Painter, PaintError>;

  [[nodiscard]] static auto create(provider::GpuProvider& gpu,
                                   text::AtlasOptions options,
                                   provider::DiagnosticsProvider& diagnostics)
      -> std::expected<Painter, PaintError>;

  void debug_glass_coverage(bool enabled) noexcept { glass_probe_ = enabled; }

  void debug_full_walk(bool enabled) noexcept { full_walk_ = enabled; }

  void see_through_backdrops(bool enabled) noexcept { see_through_ = enabled; }

  void debug_separate_backdrops(bool enabled) noexcept {
    compositor_.separate(enabled);
  }

  void repaint_whole() noexcept { whole_next_ = true; }

  void set_picture_budget(PictureBudget budget) noexcept {
    picture_budget_ = budget;
  }

  auto pictures() const noexcept -> Pictures;

  static auto contrast_threshold(Contrast kind,
                                 theme::ColorScheme scheme) noexcept -> float;

  static auto composite(theme::Colour colour, float opacity,
                        theme::Colour ground) noexcept -> theme::Colour;

  [[nodiscard]] auto paint(composition::Tree& tree, provider::GpuFrame& frame,
                           Target target, TypesetterRef typesetter)
      -> std::expected<Statistics, PaintError>;

  [[nodiscard]] auto paint(composition::Tree& tree, provider::GpuFrame& frame,
                           Target target, TypesetterRef typesetter,
                           Highlights highlights)
      -> std::expected<Statistics, PaintError>;

  auto atlas() const noexcept -> const text::GlyphAtlas& { return atlas_; }

  auto promoted(composition::NodeId node) const -> bool {
    return compositor_.layered(node.value());
  }

  auto targets() const noexcept -> RenderTargetPool::Statistics {
    return compositor_.pool().statistics();
  }

 private:
  struct Box {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    auto width() const noexcept -> float { return right - left; }

    auto height() const noexcept -> float { return bottom - top; }

    friend auto operator==(const Box&, const Box&) -> bool = default;
  };

  struct Affine {
    float a{1.0F};
    float b{0.0F};
    float c{0.0F};
    float d{1.0F};
    float tx{0.0F};
    float ty{0.0F};

    static auto translation(float x, float y) noexcept -> Affine;

    static auto scaling(float x, float y) noexcept -> Affine;

    static auto rotation(double radians) noexcept -> Affine;

    auto around(float x, float y) const noexcept -> Affine;

    auto then(const Affine& inner) const noexcept -> Affine;

    auto apply(std::array<float, 2> point) const noexcept
        -> std::array<float, 2>;

    auto identity() const noexcept -> bool;

    auto translating() const noexcept -> bool;

    friend auto operator==(const Affine&, const Affine&) -> bool = default;
  };

  struct Quad {
    provider::ShapeKind kind{provider::ShapeKind::fill};
    Box bounds;
    std::array<float, 4> uv{};
    std::array<float, 2> centre{};
    std::array<float, 2> half_size{};
    std::array<float, 4> shape{};
    std::uint32_t colour{0};
    std::optional<provider::TextureId> texture;
    provider::ClipRect clip{};
    std::uint32_t transform{0};
    std::uint32_t mesh{0};
    std::array<float, 4> effect{};
    std::array<float, 4> detail{};
    std::optional<provider::TextureId> detail_texture{};
    bool backdrop{false};
    std::uint64_t owner{0};
    bool fresh{false};
    bool external{false};
    std::array<float, 2> trim{provider::DrawCommand::untrimmed};
    std::uint32_t binding{0};
    std::uint32_t fade{0};
    float reach{std::numeric_limits<float>::infinity()};
  };

  struct Replay {
    Affine baked;
    std::array<float, 2> translation{};
    float opacity{1.0F};
  };

  struct Fade {
    std::uint32_t parent{0};
    float local{1.0F};
    float value{1.0F};
  };

  struct RunSpan {
    std::size_t begin{0};
    std::size_t end{0};
    std::size_t placed{0};
    bool samples{false};
  };

  struct LayerPass {
    provider::TextureId target{0};
    provider::PassRegion region{};
    std::array<float, 2> origin{};
    std::vector<Quad> quads;
    std::uint64_t owner{0};
    std::uint64_t spoils{0};
    std::string_view effect;
  };

  struct Corners {
    float radius{0.0F};
    float smoothing{0.0F};
    std::array<float, 4> radii{};
    bool ellipse{false};

    friend auto operator==(const Corners&, const Corners&) -> bool = default;
  };

  struct Hierarchy {
    const Hierarchy* parent{nullptr};
    graphics::StyleHierarchy source;
    std::vector<graphics::ShapeStyle> levels;
  };

  struct Styled {
    const graphics::ShapeStyle* style{nullptr};
    float opacity{1.0F};
  };

  struct Shade {
    float mode{0.0F};
    std::array<float, 4> points{};
    std::array<float, 2> extent{};
    std::array<float, 2> centre{};
    provider::TextureId ramp{0};
  };

  struct Outline {
    Box box;
    Corners corners;

    friend auto operator==(const Outline&, const Outline&) -> bool = default;
  };

  struct Enclosure {
    Outline outline;
    std::uint32_t transform{0};
  };

  struct Viewport {
    Box screen;
    composition::Axes axes;

    friend auto operator==(const Viewport&, const Viewport&) -> bool = default;
  };

  struct Shading {
    Affine transform;
    float opacity{1.0F};
    graphics::ColourMatrix colour{};
    float blur{0.0F};
    bool opaque{false};
    graphics::BlendMode blend{graphics::BlendMode::normal};
    graphics::ProjectionTransform projection{};
    std::optional<graphics::Shader> shader{};
    composition::Size sample_offset{};
  };

  struct EffectPlan {
    graphics::ColourMatrix matrix{};
    bool premultiplied{false};
    bool linear{false};
    float blur{0.0F};
    bool opaque{false};
    graphics::BlendMode blend{graphics::BlendMode::normal};
    bool group{false};
    bool masks{false};
    bool inverse_mask{false};
    bool extends{false};
    std::optional<graphics::Shader> shader{};
    composition::Size sample_offset{};
    std::optional<graphics::ProjectionTransform> projection{};
    std::optional<graphics::AffineTransform> affine{};

    auto layered() const noexcept -> bool {
      return group || masks || extends || shader.has_value() || blur > 0.0F ||
             !matrix.is_identity() || blend != graphics::BlendMode::normal ||
             projection.has_value();
    }
  };

  struct EffectSource {
    provider::TextureId texture{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::array<float, 2> extent{1.0F, 1.0F};
    std::array<float, 2> origin{};
    std::uint64_t hash{0};
    bool rendered{false};
  };

  struct LayerContent {
    std::uint64_t hash{0};
    std::array<float, 2> anchor{};
    provider::ClipRect clip{};
  };

  struct LayerArea {
    Box region;
    bool clamped{false};
  };

  struct Arrow {
    float x{0.0F};
    float y{0.0F};

    friend auto operator==(const Arrow&, const Arrow&) -> bool = default;
  };

  struct Popover {
    float dx{0.0F};
    float dy{0.0F};
    std::optional<Arrow> arrow;
  };

  struct Scope {
    std::reference_wrapper<const theme::Theme> theme;
    std::optional<theme::ColorScheme> scheme;
    std::reference_wrapper<const text::Locale> locale;
    const Hierarchy* foreground{nullptr};
    float opacity{1.0F};
    provider::ClipRect clip{};
    bool reduce_motion{false};
    std::uint32_t transform{0};
    theme::DynamicTypeSize dynamic_type_size{theme::DynamicTypeSize::large};
    theme::DynamicTypeRange dynamic_type_range{theme::DynamicTypeRange::all()};
    std::optional<theme::Font> font{};
    composition::TextAttributes text{};
    composition::LayoutDirection direction{
        composition::LayoutDirection::left_to_right};
    bool focused{false};
    theme::ColorSchemeContrast contrast{theme::ColorSchemeContrast::standard};
    bool reduce_transparency{false};
    std::optional<motion::ContentTransition> content_transition{};
    std::optional<Viewport> viewport{};
    std::optional<Arrow> arrow{};
    bool focus_effect_disabled{false};
    bool differentiate{false};
    bool invert_colors{false};
    bool ignores_invert{false};
    bool inverts_here{false};
    bool reduce_highlighting{false};
    bool show_borders{false};
    bool cross_fade{false};
    std::optional<Enclosure> enclosure{};
    bool enabled{true};
    std::uint32_t fade{0};
    composition::RedactionReasons redaction{};
    bool sensitive{false};
    theme::Theme::ColourRoles* roles{nullptr};
    const graphics::ShapeStyle* background_style{nullptr};
    const composition::ScrollEdgeEffects* edge_effects{nullptr};
    std::optional<composition::Point> group_origin{};
    bool hover_disabled{false};
    const composition::CustomHoverEffect* default_hover{nullptr};
    std::optional<composition::NodeId> hover_container{};
    const composition::HoverEffectGroup* hover_group{nullptr};

    auto redacts() const noexcept -> bool {
      return redaction.redacts(sensitive);
    }

    auto resolved_scheme() const noexcept -> theme::ColorScheme {
      return scheme.value_or(theme.get().scheme());
    }

    auto colour(theme::ColourValue value) const noexcept -> theme::Colour {
      if (roles != nullptr) {
        if (const theme::ColourRole* role = value.role()) {
          roles->set(std::to_underlying(*role));
        }
        for (const theme::ColourValue::Step& step : value.steps()) {
          if (const auto* role = std::get_if<theme::ColourRole>(&step.colour)) {
            roles->set(std::to_underlying(*role));
          }
        }
      }
      return theme.get().colour(value, resolved_scheme(), contrast);
    }
  };

  struct Span {
    std::uint32_t quads{0};
    std::uint32_t rings{0};
    std::uint32_t transforms{0};
    std::uint32_t meshes{0};
    std::uint32_t backdrops{0};
    std::uint32_t deferrals{0};
    std::uint32_t presences{0};
    std::uint32_t fades{0};
    std::uint32_t fields{0};

    friend auto operator+(Span left, Span right) noexcept -> Span {
      return Span{.quads = left.quads + right.quads,
                  .rings = left.rings + right.rings,
                  .transforms = left.transforms + right.transforms,
                  .meshes = left.meshes + right.meshes,
                  .backdrops = left.backdrops + right.backdrops,
                  .deferrals = left.deferrals + right.deferrals,
                  .presences = left.presences + right.presences,
                  .fades = left.fades + right.fades,
                  .fields = left.fields + right.fields};
    }

    friend auto operator-(Span left, Span right) noexcept -> Span {
      return Span{.quads = left.quads - right.quads,
                  .rings = left.rings - right.rings,
                  .transforms = left.transforms - right.transforms,
                  .meshes = left.meshes - right.meshes,
                  .backdrops = left.backdrops - right.backdrops,
                  .deferrals = left.deferrals - right.deferrals,
                  .presences = left.presences - right.presences,
                  .fades = left.fades - right.fades,
                  .fields = left.fields - right.fields};
    }
  };

  struct Presence {
    composition::NodeId node{0};
    Box frame{};
    std::uint32_t transform{0};
    provider::ClipRect clip{};
    float hit_radius{0.0F};
    float opacity{1.0F};
    std::uint32_t fade{0};
    std::optional<Corners> shaped{};
    std::optional<graphics::AnyShape> hit_path{};
    graphics::FillStyle hit_fill{};
    std::optional<Box> drawn{};
    bool hollow{false};
  };

  struct HitRegion {
    std::optional<Box> drawn{};
    bool hollow{false};
  };

  struct ClipSource {
    std::shared_ptr<const components::ShapeView::Source> source;
    std::uint64_t used{0};
  };

  struct BackdropCall {
    std::uint64_t key{0};
    provider::PassRegion wanted{};
    provider::PassRegion core{};
    float radius{0.0F};
    std::uint32_t behind{0};
    Compositor::Surface surface{};
  };

  struct FieldCall {
    std::uint64_t key{0};
    provider::TextureId texture{0};
  };

  struct Lineage {
    std::optional<Span> previous{};
    std::uint64_t walked{0};
    Span begin{};
  };

  struct Signature {
    const theme::Theme* theme{nullptr};
    std::optional<theme::ColorScheme> scheme{};
    const text::Locale* locale{nullptr};
    const Hierarchy* foreground{nullptr};
    const graphics::ShapeStyle* background_style{nullptr};
    float opacity{1.0F};
    provider::ClipRect clip{};
    bool reduce_motion{false};
    theme::DynamicTypeSize dynamic_type_size{theme::DynamicTypeSize::large};
    theme::DynamicTypeRange dynamic_type_range{theme::DynamicTypeRange::all()};
    std::optional<theme::Font> font{};
    composition::TextAttributes text{};
    composition::LayoutDirection direction{
        composition::LayoutDirection::left_to_right};
    bool focused{false};
    theme::ColorSchemeContrast contrast{theme::ColorSchemeContrast::standard};
    bool reduce_transparency{false};
    std::optional<motion::ContentTransition> content_transition{};
    std::optional<Viewport> viewport{};
    std::optional<Arrow> arrow{};
    bool focus_effect_disabled{false};
    std::optional<Outline> enclosure{};
    bool redacted{false};
    bool hover_disabled{false};
    const composition::CustomHoverEffect* default_hover{nullptr};
    std::optional<composition::NodeId> hover_container{};
    const composition::HoverEffectGroup* hover_group{nullptr};

    friend auto operator==(const Signature&, const Signature&)
        -> bool = default;
  };

  static constexpr Box unreached{
      .left = std::numeric_limits<float>::max(),
      .top = std::numeric_limits<float>::max(),
      .right = std::numeric_limits<float>::lowest(),
      .bottom = std::numeric_limits<float>::lowest()};

  struct Gathered {
    bool unsettled{false};
    bool positional{false};
    Box reach{};
    std::uint64_t pages{0};
  };

  struct Own {
    Span head{};
    Span tail{};
    Box reach{};
    bool translatable{true};
    std::uint64_t pages{0};
    Scope inner;
    std::optional<std::uint32_t> inner_transform{};
    std::optional<std::uint32_t> inner_fade{};
    bool encloses{false};
  };

  struct Record {
    composition::NodeId node{0};
    std::uint64_t stamp{0};
    std::uint64_t own_stamp{0};
    std::optional<Own> own{};
    std::uint64_t generation{0};
    std::uint64_t walked{0};
    std::uint64_t reached{0};
    bool reusable{false};
    bool translatable{false};
    bool united{false};
    Signature signature{};
    theme::Theme::ColourRoles roles{};
    Affine transform{};
    std::uint32_t parent_transform{0};
    float faded{1.0F};
    Box reach{};
    Span offset{};
    Span extent{};
    std::uint64_t pages{0};
  };

  struct Reprisal {
    Own own;
    std::uint32_t parent_transform{0};
    Span extent{};
    theme::Theme::ColourRoles roles{};
  };

  struct Adoption {
    std::uint32_t parent{0};
    Affine local{};
  };

  struct Splice {
    std::uint32_t from{0};
    std::uint32_t to{0};
    std::uint32_t count{0};
  };

  struct Sized {
    const theme::Theme* base{nullptr};
    theme::DynamicTypeSize size{theme::DynamicTypeSize::large};
    theme::Theme theme;
  };

  static constexpr composition::NodeId vacant_record{
      std::numeric_limits<std::uint64_t>::max()};
  static constexpr std::size_t canonical_limit = 32;

  struct Hover {
    float scale{1.0F};
    double rotation{0.0};
    float offset_x{0.0F};
    float offset_y{0.0F};
    float lifted{0.0F};
    bool present{false};
    float lift{0.0F};
    float highlight{0.0F};
    float content_scale_x{1.0F};
    float content_scale_y{1.0F};
    float opacity{1.0F};
  };

  struct PhaseMemory {
    std::optional<composition::ElementKey> trigger{};
    bool flipped{false};
    std::optional<std::chrono::nanoseconds> changed{};
  };

  struct Arranged {
    std::shared_ptr<const text::TextLayout> layout;
    float origin_x{0.0F};
    float origin_y{0.0F};
  };

  struct Ink {
    float dx{0.0F};
    float dy{0.0F};
    float alpha{1.0F};
  };

  struct TextInks {
    std::span<const std::uint32_t> runs{};
    std::optional<std::uint32_t> link{};
    std::optional<std::uint32_t> underline{};
    std::optional<std::uint32_t> strikethrough{};
    std::span<const components::AttributedString::InlineImage> images{};
    std::span<const components::AttributedString::AdaptiveImageGlyph> glyphs{};
  };

  struct Decoration {
    bool active{false};
    std::uint32_t colour{0};
    float left{0.0F};
    float right{0.0F};
    float top{0.0F};
    float thickness{0.0F};
    text::LinePattern pattern{text::LinePattern::solid};
    float size{0.0F};
  };

  struct Underlay {
    std::uint32_t colour{0};
    float left{0.0F};
    float right{0.0F};
  };

  struct ShadedGlyph {
    std::size_t quad{0};
    std::uint16_t shade{0};
  };

  struct Picture {
    std::weak_ptr<const provider::Image> source;
    provider::Texture texture;
    std::uint32_t idle{0};
    std::uint64_t bytes{0};
    std::uint64_t painted{0};
  };

  struct PictureKey {
    const provider::Image* image{nullptr};
    bool mask{false};
    provider::SamplerFilter filter{provider::SamplerFilter::linear};
    std::uint8_t level{0};

    friend auto operator<=>(const PictureKey&, const PictureKey&) = default;
  };

  Painter(provider::GpuProvider& gpu, text::GlyphAtlas atlas) noexcept;

  auto scoped(const composition::Node& node, Scope scope) -> Scope;

  static auto glass_of(const composition::Modifiers& modifiers,
                       const theme::Theme& theme) noexcept
      -> std::optional<theme::Glass>;

  static auto hit_region(const composition::Node& node,
                         const theme::Theme& theme, const Box& frame,
                         float scale) -> HitRegion;

  static auto glass_fill(const Scope& scope, const theme::Glass& glass,
                         bool lit) noexcept -> theme::Colour;

  auto lens_of(const Scope& scope, const theme::Glass& glass,
               Box area) const noexcept -> GlassLens;

  struct Backing {
    std::optional<std::size_t> behind{};
    std::optional<theme::Material> frost{};
    float shadow{1.0F};
  };

  void glaze(composition::NodeId key, const Scope& scope,
             const theme::Glass& glass, Box area, Corners corners, bool lit,
             Backing backing);

  void flat_glass(const Scope& scope, const theme::Glass& glass, Box area,
                  Corners corners, bool lit);
  void see_through_glass(const Scope& scope, const theme::Glass& glass,
                         Box area, Corners corners, bool lit);

  void glass_shadow(const Scope& scope, Box area, Corners corners,
                    float strength);

  auto highlight_spot(composition::NodeId node, Box area) const noexcept
      -> std::array<float, 2>;

  auto backdrop_region(Box area, float reach) const noexcept
      -> provider::PassRegion;

  static auto region_effect(const provider::PassRegion& region) noexcept
      -> std::array<float, 4>;

  void frost(composition::NodeId key, const Scope& scope,
             theme::Material material, Box area, Corners corners,
             std::optional<std::size_t> behind = std::nullopt);

  void glass_stroke(const composition::Node& node, const Scope& scope,
                    const theme::Glass& glass, Box frame,
                    const ShapePainter::Geometry& geometry);

  static auto glassy(const composition::Node& node) noexcept -> bool;

  auto promotable(const composition::Node& node,
                  const motion::Presentation& presentation,
                  const std::optional<motion::Transition::Effect>& effect,
                  const Scope& parent, bool carries) const -> bool;

  void promote(const composition::Node& node, const Scope& scope,
               const Scope& parent, const Affine& local, float factor,
               std::size_t first, std::size_t backdrops_before);

  void fade_range(std::size_t first, float factor);

  static auto recoloured(const motion::Transition::Effect& effect) noexcept
      -> graphics::ColourMatrix;

  static auto smart_invert() noexcept -> graphics::ColourMatrix;

  auto effect_plan(const composition::Node& node, const Scope& scope,
                   const composition::Rect& laid,
                   const std::optional<Shading>& shaded,
                   const std::optional<motion::Transition::Effect>& appearing,
                   motion::Presentation& presentation,
                   const motion::Presentation::Moment& moment) const
      -> EffectPlan;

  static auto projected(
      const std::vector<composition::GraphicEffects::Projection>& projections,
      composition::Size size, motion::Presentation& presentation,
      const motion::Presentation::Moment& moment)
      -> graphics::ProjectionTransform;

  static auto clip_box(const provider::ClipRect& clip) noexcept -> Box;

  static auto released(const provider::ClipRect& clip,
                       const provider::ClipRect& parent) noexcept -> Box;

  auto visible(const Quad& quad, Box clip) const -> std::optional<Box>;

  auto covering(std::size_t first, std::size_t last,
                std::optional<provider::ClipRect> parent) const
      -> std::optional<Box>;

  auto layer_area(std::size_t first, std::size_t last,
                  const provider::ClipRect& parent, float pad) const
      -> std::optional<LayerArea>;

  void rebase(LayerPass& pass, const provider::ClipRect& parent) const;

  auto effect_stage(std::uint64_t key, std::uint32_t stage,
                    const EffectSource& source, std::uint32_t width,
                    std::uint32_t height, std::uint64_t salt, Quad quad)
      -> std::optional<EffectSource>;

  auto effect_layer(std::uint64_t key, std::uint32_t stage, Box region,
                    std::size_t first, std::size_t last,
                    const LayerContent& content) -> std::optional<EffectSource>;

  void composite(const composition::Node& node, const Scope& scope,
                 const Scope& parent, const EffectPlan& plan, float factor,
                 std::size_t first, std::optional<std::size_t> split,
                 std::size_t backdrops_before, Box frame);

  void project(const graphics::ProjectionTransform& projection,
               const Affine& world, Box frame, Box region,
               const EffectSource& source, Quad quad, float pad);

  auto shaped_clip(const composition::Modifiers& modifiers, const Scope& scope,
                   Box area) const -> std::optional<Corners>;

  void round_images(std::size_t first, Box area, const Corners& corners,
                    std::uint32_t transform);
  auto intrudes(std::size_t first, Box area, const Corners& corners,
                std::uint32_t transform) const -> bool;

  static auto encloses(std::array<float, 2> point, Box clip,
                       const std::array<float, 4>& reach, bool ellipse) noexcept
      -> bool;

  auto inside_clip(const Quad& quad, Box placed, Box clip,
                   const std::array<float, 4>& reach,
                   bool ellipse) const noexcept -> bool;

  static auto inscribed(const Quad& quad, Box area, const Corners& corners,
                        std::uint32_t transform) noexcept -> bool;

  void clip_mask(const composition::Node& node, const Scope& scope, Box area,
                 const Corners& corners);

  auto presented_clip(const composition::Tree& tree, const Presence& presence,
                      const composition::PresentedFrame& presented) const
      -> composition::PresentedClip;

  auto anchor_of(std::size_t first) const -> std::array<float, 2>;

  auto layer_hash(std::size_t first, std::size_t last,
                  std::array<float, 2> anchor, const provider::ClipRect& parent,
                  bool anchored) const -> std::uint64_t;

  auto write(const Quad& quad, const Replay& replayed,
             const provider::ShapeGeometry& geometry, std::uint32_t& base,
             std::uint32_t& first) const -> std::uint32_t;

  void write_vertices(const Quad& quad, const Affine& matrix,
                      std::span<provider::ShapeVertex> out) const;

  auto replay(const Quad& quad) const noexcept -> Replay;

  auto write_indices(const Quad& quad, std::span<provider::Index> out,
                     std::uint32_t base) const -> std::uint32_t;

  static auto quad_hash(std::uint64_t state, const Quad& quad,
                        const Affine& matrix,
                        const ShapePainter::Geometry* mesh) noexcept
      -> std::uint64_t;

  static auto samples_backdrop(provider::ShapeKind kind) noexcept -> bool;

  void own(std::size_t from, composition::NodeId node, std::uint32_t fade);

  auto fade(std::uint32_t parent, float local) -> std::uint32_t;

  auto shown(const Scope& scope) const noexcept -> float;

  void gather_runs();

  auto whole_damage() noexcept -> bool;

  auto damage_rect(const DrawListCache::Bounds& bounds) const noexcept
      -> provider::ClipRect;

  static auto vertex_box(
      std::span<const provider::ShapeVertex> vertices) noexcept
      -> DrawListCache::Bounds;

  static auto drawn_bounds(const DrawListCache::Bounds& box,
                           const provider::ClipRect& clip,
                           std::array<float, 2> shift) noexcept
      -> DrawListCache::Bounds;

  static auto clip_bounds(const provider::ClipRect& clip) noexcept
      -> DrawListCache::Bounds;

  static auto stamp(const Target& target) noexcept -> std::chrono::nanoseconds;

  auto cost(const Quad& quad) const noexcept -> std::array<std::size_t, 2>;

  static void batch(std::vector<provider::DrawCommand>& commands,
                    std::vector<std::uint32_t>& owners, const Quad& quad,
                    provider::TextureId& current, std::uint32_t first,
                    std::uint32_t used, const Replay& replayed);

  auto bind(const motion::Presentation& presentation,
            std::uint32_t parent_transform) -> std::uint32_t;

  void claim(std::size_t from, std::uint32_t binding);

  void capture(const provider::ShapeGeometry& geometry,
               std::uint32_t vertex_count, std::uint64_t generation,
               bool replayable);

  template <class T>
  static void exchange(std::vector<T>& current, std::vector<T>& previous) {
    std::swap(current, previous);
    current.reserve(previous.capacity());
  }

  template <class T>
  static void grow(std::vector<T>& values, std::size_t wanted) {
    if (values.capacity() < wanted) {
      values.reserve(wanted * 2);
    }
  }

  void emit_layers(provider::GpuFrame& frame,
                   const provider::ShapeGeometry& geometry, std::uint32_t& base,
                   std::uint32_t& first);

  auto add_layer_pass(const Compositor::LayerPlan& plan, std::uint64_t owner,
                      std::uint64_t spoils, std::string_view effect)
      -> LayerPass&;

  void fall_back(std::size_t first, float factor);

  static auto effect_key(std::uint64_t key) noexcept -> std::uint64_t;

  static auto effect_name(provider::ShapeKind kind) noexcept
      -> std::string_view;

  auto inverse(const Affine& matrix) const noexcept -> Affine;

  void unite(composition::Tree& tree, const composition::Node& container,
             const Scope& scope);

  auto retinted(const theme::Theme& base, theme::Colour accent)
      -> const theme::Theme&;

  static auto tint_colour(const Scope& scope, const graphics::ShapeStyle& style)
      -> std::optional<theme::Colour>;

  void refuse_picture(std::string_view stage, const provider::Image& source,
                      std::uint8_t level, const provider::ProviderError& error);

  void missing_symbol(const composition::Node& node);

  void forget_departed(const composition::Tree& tree);

  void inspect(const composition::Node& node, const Scope& scope,
               theme::Colour colour, Contrast kind, Box area);

  auto ground(Box area) const noexcept -> theme::Colour;

  static auto unpack(std::uint32_t colour) noexcept -> theme::Colour;

  auto backdrop(const composition::Node& root, Scope scope) -> Scope;

  static auto pack(theme::Colour colour, float opacity) noexcept
      -> std::uint32_t;

  static auto snap(Box box) noexcept -> Box;

  static auto intersect(provider::ClipRect clip, Box box) noexcept
      -> provider::ClipRect;

  static auto focus_outline(const composition::Node& node)
      -> std::optional<theme::Shape>;

  static auto rounding(const theme::Theme& theme, theme::Shape outline,
                       Box area, float scale) noexcept -> Corners;

  auto resolve(const Scope& scope, const theme::Shape& outline, Box area) const
      -> Corners;

  static auto corner_of(const Corners& corners, std::size_t corner) noexcept
      -> float;

  static auto uneven(const Corners& corners) noexcept -> bool;

  static auto fitted(const theme::Shape& outline, Box area) noexcept -> Box;

  auto bounded(const graphics::ShapeStyle& style, Box frame) const noexcept
      -> Box;

  static void even_out(std::uint8_t edges,
                       composition::LayoutDirection direction,
                       std::array<float, 4>& radii) noexcept;

  static auto outer_radius(const Outline& outline, std::size_t corner) noexcept
      -> float;

  static auto plain_corners(const Corners& corners, Box area) noexcept
      -> Corners;

  static auto grown(const Corners& corners, float amount) noexcept -> Corners;

  static auto detail_of(const Corners& corners) noexcept
      -> std::array<float, 4>;

  static auto corners_of(std::array<float, 4> radii, float smoothing,
                         float limit) noexcept -> Corners;

  static auto screen_corners(const theme::RectangleCornerRadii& radii,
                             composition::LayoutDirection direction) noexcept
      -> std::array<float, 4>;

  auto hierarchy(const Hierarchy* parent,
                 const graphics::StyleHierarchy& source) -> const Hierarchy*;

  auto canonical_background(const graphics::ShapeStyle& style)
      -> const graphics::ShapeStyle*;

  auto style_of(const Scope& scope, const graphics::ShapeStyle& style) const
      -> Styled;

  auto ink(const Scope& scope, theme::ColourValue fallback) const
      -> theme::ColourValue;

  auto material_colour(const Scope& scope, theme::Material material) const
      -> theme::Colour;

  auto animated_shade(const Scope& scope, const graphics::ShapeStyle& style,
                      Box frame, composition::NodeId owner,
                      motion::Presentation& presentation,
                      const motion::Presentation::Moment& moment)
      -> std::optional<Shade>;

  void animate_paint(ShapePainter::Paint& paint, composition::NodeId owner,
                     motion::Presentation& presentation,
                     const motion::Presentation::Moment& moment,
                     motion::Presentation::Mix mix);

  auto shade(const Scope& scope, const graphics::ShapeStyle& style, Box frame)
      -> std::optional<Shade>;

  void stylise(std::size_t from, const Shade& shade);

  static void stylise(Quad& quad, const Shade& shade);

  struct Motion {
    std::reference_wrapper<motion::Presentation> presentation;
    std::reference_wrapper<const motion::Presentation::Moment> moment;
  };

  void styled_fill(composition::NodeId key, const Scope& scope,
                   const graphics::ShapeStyle& style, Box area, Corners corners,
                   std::optional<Motion> animated = std::nullopt);

  auto shaded_paint(const ShapePainter::Paint& paint, Box frame)
      -> std::optional<Shade>;

  auto blends(const Scope& scope, const graphics::ShapeStyle& style, Box area,
              Corners corners, float opacity,
              const std::optional<Motion>& animated) -> bool;

  auto gradient_blend(const Scope& scope, Box area, Corners corners,
                      float opacity, const Motion& animated) -> bool;

  static auto encoded(const ShapePainter::Paint& paint,
                      std::span<float> into) noexcept -> std::size_t;

  static void decoded(std::span<const float> values, ShapePainter::Paint& into);

  static auto paint_shape(const ShapePainter::Paint& paint) noexcept
      -> std::uint64_t;

  void blend_corners(motion::Presentation& presentation,
                     const motion::Presentation::Moment& moment,
                     std::optional<double> previous, Corners& rounded,
                     Box area) const;

  void style_shadow(const Scope& scope, const graphics::ShapeStyle& style,
                    Box area, Corners corners,
                    graphics::ShadowStyle::Kind kind);

  static auto painted_outline(const composition::Modifiers& modifiers,
                              bool button, bool glassed) -> theme::Shape;

  struct UnionMember {
    std::optional<composition::GlassEffect::Tag> tag;
    composition::NodeId id{0};
    theme::Glass glass;
    theme::Shape shape;
    Box area;
    std::optional<float> radius;
  };

  auto member_corners(const Scope& scope, const UnionMember& member) const
      -> Corners;

  struct Fusion {
    std::uint64_t key{0};
    std::uint64_t owner{0};
    float spacing{1.0F};
    std::optional<Compositor::Surface> backdrop;
  };

  void fuse(const Fusion& fusion, std::span<const UnionMember> members,
            const Scope& scope);

  auto held(composition::NodeId node) const noexcept -> bool;

  auto lit(const UnionMember& member) const noexcept -> bool;

  struct Deferred {
    std::reference_wrapper<const composition::Node> node;
    Scope scope;
    std::int32_t z{0};
    std::size_t order{0};
  };

  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  enum class Stage : std::uint8_t { spliced, painted, reprised };

  enum class Order : std::uint8_t { plain, arranged, masked, pinned, done };

  enum class ShadowMotion : std::uint8_t { settled, moving };

  struct Cursor {
    Order order{Order::done};
    std::optional<Popover> anchored{};
    std::size_t position{0};
    std::size_t hidden{0};
    std::size_t index{0};
    std::size_t base{0};
    std::size_t end{0};
    bool lifting{false};
    bool masks{false};
    bool offset{false};
  };

  struct Retention {
    bool active{false};
    Lineage outer{};
    Span begin{};
    Signature wanted{};
    theme::Theme::ColourRoles outer_roles{};
    bool outer_unsettled{false};
    bool outer_positional{false};
    Box outer_reach{};
    std::uint64_t outer_pages{0};
    std::size_t outer_mark{0};
    std::uint64_t stamp{0};
    std::uint64_t own_stamp{0};
    std::uint32_t parent_transform{0};
    std::uint32_t parent_fade{0};
  };

  struct Reprise {
    Own own;
    Span from{};
    Span extent{};
    Span at{};
    std::uint32_t parent_transform{0};
    std::uint32_t outer_transform{0};
    std::uint32_t outer_fade{0};
  };

  struct Painting {
    Scope parent;
    Scope scope;
    motion::Presentation* presentation{nullptr};
    EffectPlan plan{};
    Gathered head{};
    Affine local{};
    Box frame{};
    Box snapped{};
    Corners rounded{};
    Corners hover_corners{};
    Hover hover{};
    std::optional<Corners> clip_corners{};
    std::optional<Box> link_ring{};
    Span entry{};
    Span head_end{};
    float effect_factor{1.0F};
    float factor{1.0F};
    std::size_t own_from{0};
    std::size_t own_backdrops{0};
    std::size_t promoted_from{0};
    std::size_t backdrops_before{0};
    std::uint32_t enclosing{0};
    bool effecting{false};
    bool promoting{false};
    bool anchored{false};
    bool carries{false};
    bool binds{false};
    bool encloses{false};
  };

  struct Visit {
    Scope inner;
    Stage stage{Stage::painted};
    bool descended{false};
    Retention retention{};
    Cursor cursor{};
    std::optional<std::size_t> split{};
  };

  using Walk = support::FullOrderWalk<NodeAccess, Visit>;

  class Walker {
   public:
    Walker(Painter& painter, composition::Tree& tree) noexcept
        : painter_(painter), tree_(tree) {}

    auto enter(const composition::Node& node, const Visit& above) -> Walk::Step;

    auto next(const composition::Node& node, Visit& visit)
        -> const composition::Node*;

    void leave(const composition::Node& node, Visit& visit);

   private:
    std::reference_wrapper<Painter> painter_;
    std::reference_wrapper<composition::Tree> tree_;
  };

  [[nodiscard]] auto walk(composition::Tree& tree,
                          const composition::Node& node, const Scope& parent)
      -> std::expected<void, PaintError>;

  auto open(composition::Tree& tree, const composition::Node& node,
            const Visit& above) -> Walk::Step;

  auto advance(composition::Tree& tree, const composition::Node& node,
               Visit& visit) -> const composition::Node*;

  void close(composition::Tree& tree, const composition::Node& node,
             Visit& visit);

  void commit_visit(composition::Tree& tree, const composition::Node& node,
                    Visit& visit);

  [[nodiscard]] auto paint_head(composition::Tree& tree,
                                const composition::Node& node,
                                const Scope& parent, Visit& visit)
      -> std::expected<bool, PaintError>;

  void paint_tail(const composition::Node& node, Visit& visit);

  void paint_unwind();

  void begin_children(composition::Tree& tree, const composition::Node& node,
                      Visit& visit, const std::optional<Popover>& anchored);

  auto admit(const composition::Node& child, Visit& visit) -> bool;

  auto repriseable(composition::Tree& tree, const composition::Node& node,
                   const Record& record, Span from, const Scope& parent) const
      -> bool;

  void reprise_head(Own own, std::uint32_t parent_transform, Span from,
                    Span extent, const Scope& parent, Visit& visit);

  void reprise_tail();

  auto reprised_transform(const Reprise& reprise,
                          std::uint32_t index) const noexcept -> std::uint32_t;

  auto reprised_fade(const Reprise& reprise, std::uint32_t index) const noexcept
      -> std::uint32_t;

  void reprise_copy(const Reprise& reprise, Span source, Span count);

  auto gather() noexcept -> Gathered;

  void fold(const Gathered& gathered) noexcept;

  auto signature(const Scope& scope) const noexcept -> Signature;

  static auto matches(const Signature& held, const Signature& wanted,
                      const theme::Theme::ColourRoles& roles) noexcept -> bool;

  auto span() const noexcept -> Span;

  auto held_record(const composition::Node& node) noexcept
      -> std::optional<std::reference_wrapper<Record>>;

  auto spliceable(const Record& record, Span from) const noexcept -> bool;

  auto slot_for(composition::Tree& tree, const composition::Node& node)
      -> Record&;

  auto vacant_slot(composition::Tree& tree) -> std::uint32_t;

  void reclaim_records(const composition::Tree& tree);

  auto splice(composition::Tree& tree, const Record& record, Span from,
              const Scope& parent) -> bool;

  auto slides(const Record& record, const Scope& parent) const noexcept -> bool;

  auto carried(Box box, const Affine& was, const Affine& now) const noexcept
      -> Box;

  void reach(Box box) noexcept;

  auto inherited(const Record& record, Span from) const noexcept
      -> std::optional<Affine>;

  void place() noexcept { positional_ = true; }

  auto backdrop_for(std::uint64_t key, provider::PassRegion wanted,
                    provider::PassRegion core, float radius,
                    std::uint32_t behind)
      -> std::expected<Compositor::Surface, Compositor::BackdropRefusal>;

  void replay_backdrop(const BackdropCall& call, std::uint32_t behind);

  void forget_since(std::size_t first);

  void rehighlight(composition::Tree& tree, const Highlights& next);

  void begin_frame(composition::Tree& tree, const Target& target,
                   const Highlights& highlights);

  void unsettle() noexcept { unsettled_ = true; }

  void use_page(std::uint32_t page) noexcept;

  auto sized(const theme::Theme& base, theme::DynamicTypeSize size)
      -> const theme::Theme&;

  auto canonical(const theme::Theme& theme) -> const theme::Theme&;

  auto canonical(const text::Locale& locale) -> const text::Locale&;

  void forget_canonical();

  static auto layered(const composition::Node& node) noexcept -> bool;

  void defer(const composition::Node& node, const Scope& scope);

  struct Hoisted {
    std::reference_wrapper<const composition::Node> node;
    std::int32_t z{0};
  };

  void hoist(const composition::Tree& tree);

  static auto shows(const composition::Tree& tree,
                    const composition::Node& node) -> bool;

  static auto nested(const composition::Tree& tree,
                     const composition::Node& node) -> bool;

  auto layer_scope(const composition::Tree& tree, const composition::Node& node,
                   Scope scope) -> Scope;

  static auto draws(const composition::ExclusiveScopes& exclusive,
                    std::int32_t z) noexcept -> bool;

  void harvest(composition::Tree& tree, const composition::Node& node,
               const Scope& scope);

  struct Gleaning {
    Scope scope;
    std::size_t position{0};
  };

  using Gleaner = support::FullOrderWalk<NodeAccess, Gleaning>;

  class Harvester {
   public:
    Harvester(Painter& painter, composition::Tree& tree) noexcept
        : painter_(painter), tree_(tree) {}

    auto enter(const composition::Node& node, const Gleaning& above)
        -> Gleaner::Step;

    auto next(const composition::Node& node, Gleaning& gleaning)
        -> const composition::Node*;

    void leave(const composition::Node& node, Gleaning& gleaning);

   private:
    std::reference_wrapper<Painter> painter_;
    std::reference_wrapper<composition::Tree> tree_;
  };

  auto popover(const composition::Tree& tree,
               const composition::Node& layer) const -> std::optional<Popover>;

  void arrow(const Scope& scope, Box frame, std::uint32_t colour);

  void settle(composition::Tree& tree, const composition::Node& node);

  static auto mirrors(const composition::Modifiers& modifiers,
                      composition::LayoutDirection outer,
                      composition::LayoutDirection inner) noexcept -> bool;

  auto transform_of(const composition::Modifiers& modifiers,
                    motion::Presentation& presentation,
                    const motion::Presentation::Moment& moment,
                    const std::optional<motion::Transition::Effect>& effect,
                    const Hover& hover, Box frame, float width,
                    composition::LayoutDirection direction) -> Affine;

  auto hover_of(const composition::Node& node, const Scope& scope, Box frame,
                motion::Presentation& presentation,
                const motion::Presentation::Moment& moment) -> Hover;

  auto hover_active(const composition::Node& node, const Scope& scope,
                    const composition::CustomHoverEffect::Layer& layer,
                    const composition::CustomHoverEffect::Layer& origin,
                    std::uint32_t index) -> bool;

  auto phased(composition::NodeId node, std::uint32_t index,
              const composition::HoverEffectPhaseOverride& phase, bool natural)
      -> bool;

  void forget_phases(const composition::Tree& tree);

  auto shaken(composition::NodeId node) -> std::array<float, 2>;

  auto highlighted(composition::NodeId node) const noexcept -> bool;

  void record(composition::Tree& tree, const Presence& presence);

  auto shading(const composition::Node& node, const Scope& scope,
               motion::Presentation& presentation,
               const motion::Presentation::Moment& moment, Box frame,
               std::optional<composition::Rect> within) const
      -> std::optional<Shading>;

  auto pinned(const composition::ScrollEffects::Pin& pin, const Scope& scope,
              Box screen, const composition::Rect& within) const -> float;

  static auto phase_of(
      Box rect, const Viewport& viewport,
      const composition::ScrollEffects::ScrollTransition& transition) noexcept
      -> double;

  auto effect_transform(const composition::VisualEffect& effect,
                        Box frame) const -> Affine;

  void edge_effects(const composition::Node& node, const Scope& scope,
                    Box viewport);

  void indicators(const composition::Node& node, const Scope& scope,
                  Box viewport);

  auto points_of(Box box) const noexcept -> composition::Rect;

  auto scroll_proxy(Box screen, const std::optional<Viewport>& viewport) const
      -> composition::GeometryProxy;

  static void conceal(composition::Tree& tree, const composition::Node& node);

  static auto follow(const composition::Tree& tree,
                     const composition::Node& node)
      -> std::optional<composition::Rect>;

  static auto carry(Box laid, Box shown, motion::UnitPoint anchor) noexcept
      -> Affine;

  static auto tinted(theme::Colour colour, theme::Colour toward,
                     float amount) noexcept -> theme::Colour;

  static auto faded(std::uint32_t colour, float alpha) noexcept
      -> std::uint32_t;

  static auto positions(const Arranged& arranged)
      -> std::vector<std::array<float, 2>>;

  auto adopt(std::uint32_t parent, const Affine& local) -> std::uint32_t;

  auto bounds(Box box, std::uint32_t transform) const noexcept -> Box;

  auto relocate(Outline outline, std::uint32_t from,
                std::uint32_t to) const noexcept -> std::optional<Outline>;

  auto confine(Outline ring, Box frame, const Scope& scope) const noexcept
      -> Outline;

  auto enclosure_of(const composition::Modifiers& modifiers, const Scope& scope,
                    Box area, Corners painted, bool glassed, bool filled) const
      -> std::optional<Outline>;

  auto offscreen(const composition::Node& node, const Scope& parent,
                 const motion::Presentation& presentation) const noexcept
      -> bool;

  void suspend(composition::Tree& tree, const composition::Node& node);

  void place_culled(composition::Tree& tree, const composition::Node& node,
                    const Scope& parent);

  static auto covers(composition::Tree& tree, const composition::Node& cover,
                     const composition::Node& host, const Scope& scope) -> bool;

  static auto unobserved(composition::Tree& tree, const composition::Node& node)
      -> bool;

  void pace(const composition::Node& node,
            const motion::Presentation& presentation) noexcept;

  void unhint(motion::UnhintedAnimation cause) noexcept;

  static auto anchored(motion::UnitPoint anchor, Box frame) noexcept
      -> std::array<float, 2>;

  void zoom_vertices(std::span<provider::ShapeVertex> vertices) const;

  void zoom_commands();

  void place(const Quad& quad, const Affine& matrix,
             std::span<provider::ShapeVertex> vertices) const;

  void shape(provider::ShapeKind kind, Box area, Corners shape_corners,
             float stroke, float softness, std::uint32_t colour,
             const provider::ClipRect& clip);

  void shadows(const Scope& scope, theme::Elevation level, Box area,
               Corners shape_corners, float weight, ShadowMotion movement);

  void soft(Box area, Corners corners, float softness, std::uint32_t colour,
            const provider::ClipRect& clip, ShadowMotion movement);

  static auto penumbra(std::uint32_t extent, float outward, float radius,
                       float softness) -> text::GlyphBitmap;

  void patch(Box bounds, std::array<float, 4> uv, std::uint32_t colour,
             provider::TextureId texture, const provider::ClipRect& clip);

  void picture(const std::shared_ptr<const provider::Image>& pixels, Box area,
               std::uint32_t colour, const provider::ClipRect& clip,
               std::array<float, 4> uv,
               components::Image::Rendering rendering = {});

  void paint_raster(const components::Image::Raster::Content& raster,
                    const Scope& scope, Box area);

  void paint_slices(const std::shared_ptr<const provider::Image>& pixels,
                    const components::Image::Slicing& slicing, Box area,
                    std::uint32_t colour, const provider::ClipRect& clip,
                    components::Image::Rendering rendering);

  void tile_picture(const std::shared_ptr<const provider::Image>& pixels,
                    Box area, std::array<float, 4> uv, float tile_width,
                    float tile_height, std::uint32_t colour,
                    const provider::ClipRect& clip,
                    components::Image::Rendering rendering);

  void paint_image(const Scope& scope, const graphics::ImagePaint& paint,
                   Box area, float opacity);

  static auto pixel_snapped(Box area) noexcept -> Box;

  static auto masked(const provider::Image& pixels) -> std::vector<std::byte>;

  static auto reduction(const provider::Image& pixels, Box area,
                        std::array<float, 4> uv,
                        components::Image::Interpolation interpolation) noexcept
      -> std::uint8_t;

  static auto reduced(const provider::Image& pixels, std::uint8_t level)
      -> std::optional<provider::Image>;

  static auto averaged(const provider::Image& pixels,
                       std::span<const std::array<std::uint32_t, 2>> sizes,
                       std::size_t depth, std::uint32_t x,
                       std::uint32_t y) noexcept -> std::array<float, 4>;

  void note_used(std::span<const Quad> quads);

  void drop_withdrawn();

  void report_targets(Statistics& statistics);

  void mark_used();

  void evict_pictures();

  [[nodiscard]] auto redact_text(std::string_view content,
                                 const text::TextStyle& style, Box area,
                                 bool centred, const Scope& scope)
      -> std::expected<void, PaintError>;

  void redact_box(const Scope& scope, Box area);

  static auto symbolic(std::string_view content) noexcept -> bool;

  auto redaction_colour(const Scope& scope) const noexcept -> std::uint32_t;

  void sample(provider::TextureId texture, Box area, std::uint32_t colour,
              const provider::ClipRect& clip, bool fresh,
              std::array<float, 4> uv = whole_texture);

  void sample_external(provider::TextureId texture, Box area,
                       std::uint32_t colour, const provider::ClipRect& clip);

  void paint_surface(provider::SurfaceId surface, Box area, const Scope& scope);

  auto publication(provider::TextureId texture) const
      -> std::optional<provider::TexturePublications::Publication>;

  auto republished(provider::TextureId texture) -> bool;

  static auto fitted_box(const provider::Image& pixels, Box area) noexcept
      -> Box;

  static auto filled_uv(const provider::Image& pixels, Box area) noexcept
      -> std::array<float, 4>;

  void retire_pictures();

  void restyle(const Scope& scope, const graphics::ShapeStyle& style, Box frame,
               ShapePainter::Paint& paint);

  void draw_shape(const composition::Node& node, const Scope& scope,
                  motion::Presentation& presentation,
                  const motion::Presentation::Moment& moment, Box frame,
                  Corners corners);

  void draw_pass(const composition::Node& node,
                 const components::ShapeView::Content& content,
                 composition::NodeId key, const Scope& scope,
                 motion::Presentation& presentation,
                 const motion::Presentation::Moment& moment, Box frame,
                 Corners corners);

  void draw_mesh(const composition::Node& node, const Scope& scope,
                 motion::Presentation& presentation,
                 const motion::Presentation::Moment& moment, Box frame);

  void push_geometry(const ShapePainter::Geometry& geometry,
                     const provider::ClipRect& clip);

  auto stage_mesh(const ShapePainter::Geometry& geometry,
                  std::uint64_t revision) -> ShapePainter::Geometry&;

  void flat_stroke(const Scope& scope, const ShapePainter::Geometry& geometry);

  void probe_glass(const composition::Node& node);

  void report_glass(std::uint64_t node,
                    std::span<const provider::ShapeVertex> submitted,
                    const provider::ShapeGeometry& geometry,
                    const provider::DrawCommand& command);

  void push_mesh(const ShapePainter::Geometry& geometry,
                 const provider::ClipRect& clip, provider::ShapeKind kind,
                 bool backdrop);

  void drop_shadow(const Scope& scope, theme::Colour colour, float radius,
                   float x, float y, Box area, Corners shape_corners,
                   ShadowMotion movement);

  auto fitted(std::string_view content, const text::TextStyle& style, Box area,
              const text::TextFitting& fitting)
      -> std::shared_ptr<const text::FittedText>;

  auto clipped_lines(std::string_view content, const text::TextStyle& style,
                     text::LayoutWidth width, float height)
      -> std::optional<std::size_t>;

  [[nodiscard]] auto marquee(const composition::Node& node, const Scope& scope,
                             std::string_view content,
                             const text::TextStyle& style, Box area,
                             std::uint32_t colour)
      -> std::optional<std::expected<void, PaintError>>;

  [[nodiscard]] auto glyph_slice(
      const components::Text::Glyphs::Content& glyphs, Box frame,
      const Scope& scope) -> std::expected<void, PaintError>;

  [[nodiscard]] auto typeset(std::string_view content,
                             const text::TextStyle& style, Box area,
                             bool centred, std::uint32_t colour,
                             const provider::ClipRect& clip)
      -> std::expected<void, PaintError>;

  auto arrange(std::string_view content, const text::TextStyle& style, Box area,
               bool centred, bool wraps) -> std::optional<Arranged>;

  template <class Inking>
  [[nodiscard]] auto draw(const Arranged& arranged, std::uint32_t colour,
                          const provider::ClipRect& clip, Inking&& ink)
      -> std::expected<void, PaintError>;

  auto inked(const text::TextRun& run, std::uint32_t colour) const noexcept
      -> std::uint32_t;

  void extend(Decoration& decoration, bool active, std::uint32_t colour,
              float left, float advance, float top, float thickness,
              text::LinePattern pattern, float size,
              const provider::ClipRect& clip);

  void underlay(const text::TextLayout& layout, const text::TextLine& line,
                const Arranged& arranged, std::uint32_t alpha,
                const provider::ClipRect& clip);

  void attach(const text::TextRun& run, const text::PositionedGlyph& glyph,
              const Arranged& arranged, const text::TextLine& line, float alpha,
              const provider::ClipRect& clip);

  void rule(Decoration& decoration, const provider::ClipRect& clip);

  void link_frames(composition::Tree& tree, const composition::Node& node,
                   const Arranged& arranged, Box frame);

  [[nodiscard]] auto transition_text(
      const motion::ContentTransition& transition,
      const motion::Presentation::Swap& swap, std::string_view current,
      const text::TextStyle& style, Box area, std::uint32_t colour,
      const provider::ClipRect& clip, bool reduce)
      -> std::expected<void, PaintError>;

  [[nodiscard]] auto emit(provider::GpuFrame& frame)
      -> std::expected<Statistics, PaintError>;

  auto pixels_of(const composition::Node& node) const noexcept -> Box;

  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::optional<std::reference_wrapper<provider::DiagnosticsProvider>>
      diagnostics_;
  std::set<std::pair<composition::NodeId, Contrast>> reported_;
  std::uint64_t reported_revision_{0};
  std::set<std::string> missing_;
  TextInks inks_{};
  std::optional<std::pair<std::size_t, std::size_t>> glyph_window_;
  std::vector<std::uint32_t> run_inks_;
  std::vector<ShadedGlyph> shaded_glyphs_;
  std::vector<std::string> variants_;
  std::size_t underlaid_{0};
  std::vector<composition::LinkFrame> link_scratch_;
  theme::Colour clear_{0.0F, 0.0F, 0.0F, 1.0F};
  text::GlyphAtlas atlas_;
  std::map<PictureKey, Picture> pictures_;
  PictureBudget picture_budget_{default_picture_budget};
  std::uint64_t picture_frame_{0};
  std::uint64_t picture_bytes_{0};
  std::uint64_t picture_uploads_{0};
  std::uint64_t picture_evictions_{0};
  std::vector<std::uint64_t> painted_textures_;
  RenderTargetPool::Statistics reported_targets_;
  std::vector<std::map<PictureKey, Picture>::iterator> picture_victims_;
  ShapePainter shapes_;
  std::vector<ShapePainter::Geometry> meshes_;
  std::size_t mesh_count_{0};
  ShapePainter::Request shape_request_;
  ShapePainter::Request clip_request_;
  std::map<composition::NodeId, ClipSource> clip_sources_;
  ShapePainter::MeshRequest mesh_request_;
  graphics::StrokeStyle spare_stroke_;
  std::map<std::pair<std::uint32_t, const theme::Theme*>, theme::Theme> tints_;
  std::deque<theme::Theme> themes_;
  theme::Theme::ColourRoles roles_{};
  std::deque<text::Locale> locales_;
  std::deque<Sized> sized_;
  std::deque<Hierarchy> hierarchies_;
  std::deque<graphics::ShapeStyle> backgrounds_;
  std::vector<graphics::ShapeStyle> default_levels_{
      graphics::ShapeStyle{theme::ColourRole::label},
      graphics::ShapeStyle{theme::ColourRole::secondary_label},
      graphics::ShapeStyle{theme::ColourRole::tertiary_label},
      graphics::ShapeStyle{theme::ColourRole::quaternary_label},
      graphics::ShapeStyle{theme::ColourRole::quinary_label}};
  graphics::ShapeStyle default_background_{
      theme::ColourRole::control_background};
  graphics::StyleHierarchy control_text_{theme::ColourRole::control_text};
  graphics::ShapeStyle foreground_style_{
      graphics::HierarchicalShapeStyle::primary};
  ShapePainter::Paint shade_paint_;
  std::flat_set<composition::NodeId> united_;
  std::vector<Quad> quads_;
  std::vector<const composition::Node*> ordered_;
  Walk walk_;
  Gleaner gleaner_;
  std::vector<Painting> paintings_;
  std::vector<Reprise> reprises_;
  std::optional<PaintError> failure_;
  std::vector<Quad> rings_;
  DrawListCache draw_lists_;
  std::vector<RunSpan> runs_;
  std::vector<Replay> replays_;
  std::vector<std::uint64_t> quad_hashes_;
  std::flat_map<std::uint64_t, std::uint32_t> ordinals_;
  std::flat_map<std::uint64_t, std::chrono::nanoseconds> marquees_;
  std::flat_map<std::uint64_t, std::uint64_t> shown_publications_;
  std::flat_map<std::uint64_t, std::uint64_t> next_publications_;
  bool whole_next_{true};
  bool zoomed_{false};
  provider::SurfaceSize painted_pixels_{};
  theme::Colour painted_clear_{0.0F, 0.0F, 0.0F, 1.0F};
  Compositor compositor_;
  std::vector<LayerPass> layer_passes_;
  std::size_t layer_count_{0};
  std::uint32_t backdrops_{0};
  std::vector<provider::DrawCommand> commands_;
  std::vector<provider::DrawCommand> layer_commands_;
  std::vector<std::uint32_t> owners_;
  std::vector<std::uint32_t> layer_owners_;
  std::vector<Quad> effect_content_;
  std::vector<ReplayBinding> bindings_;
  std::uint32_t binding_{0};
  std::vector<Compositor::QuadSpan> spans_;
  std::vector<std::uint32_t> promoted_transforms_;
  ShapePainter::Geometry projected_;
  std::uint32_t effect_depth_{0};
  std::vector<UnionMember> union_members_;
  std::vector<Compositor::FieldShape> field_shapes_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> remapped_transforms_;
  std::vector<Affine> transforms_;
  std::vector<Fade> fades_;
  std::vector<Fade> previous_fades_;
  std::vector<Deferred> deferred_;
  std::vector<Hoisted> hoisted_;
  std::vector<std::reference_wrapper<const composition::Node>> ancestry_;
  std::optional<std::uint64_t> exclusive_revision_;
  std::size_t deferred_order_{0};
  provider::ClipRect screen_{};
  Target target_;
  TypesetterRef typesetter_;
  Highlights highlights_;
  std::flat_set<composition::NodeId> hover_nodes_;
  std::map<std::pair<std::uint64_t, std::uint32_t>, PhaseMemory> phases_;
  std::uint64_t phases_revision_{0};
  std::uint32_t transform_{0};
  bool animating_{false};
  bool unhinted_{false};
  std::optional<motion::UnhintedAnimation> unhinted_cause_;
  double hinted_rate_{0.0};
  std::uint32_t culled_{0};
  std::uint32_t visited_{0};
  std::uint32_t occluded_{0};
  std::uint32_t shadow_hits_{0};
  std::uint32_t shadow_misses_{0};
  bool glass_probe_{false};
  std::vector<std::uint64_t> glass_pending_;
  std::flat_set<std::uint64_t> glass_probed_;
  bool see_through_{false};
  bool full_walk_{false};
  bool retaining_{false};
  std::uint64_t frame_{0};
  std::uint64_t generation_{1};
  const composition::Tree* painted_tree_{nullptr};
  const text::Typesetter* painted_typesetter_{nullptr};
  float painted_scale_{0.0F};
  provider::SurfaceSize recorded_pixels_{};
  std::vector<Record> records_;
  std::vector<std::uint32_t> free_records_;
  std::vector<std::uint32_t> commits_;
  std::size_t commit_mark_{0};
  Lineage lineage_{};
  bool unsettled_{false};
  std::uint64_t pages_{0};
  std::vector<BackdropCall> backdrop_calls_;
  std::vector<BackdropCall> previous_backdrop_calls_;
  std::vector<FieldCall> field_calls_;
  std::vector<FieldCall> previous_field_calls_;
  std::vector<Deferred> deferrals_;
  std::vector<Deferred> previous_deferrals_;
  std::vector<ShapePainter::Geometry> previous_meshes_;
  std::size_t previous_mesh_count_{0};
  theme::Colour recorded_clear_{0.0F, 0.0F, 0.0F, 0.0F};
  std::vector<Quad> previous_quads_;
  std::vector<Quad> previous_rings_;
  std::vector<Affine> previous_transforms_;
  std::vector<Adoption> adoptions_;
  std::vector<Presence> presences_;
  std::vector<Presence> previous_presences_;
  std::vector<Adoption> previous_adoptions_;
  bool positional_{false};
  Box reach_{.left = std::numeric_limits<float>::max(),
             .top = std::numeric_limits<float>::max(),
             .right = std::numeric_limits<float>::lowest(),
             .bottom = std::numeric_limits<float>::lowest()};
  std::vector<Replay> previous_replays_;
  std::vector<std::uint64_t> previous_hashes_;
  std::size_t previous_main_{0};
  std::size_t main_count_{0};
  std::vector<Splice> splices_;
  std::vector<Splice> ring_splices_;
  std::uint32_t recorded_{0};
  std::uint32_t spliced_{0};
  std::optional<Own> staged_{};
};

}
