#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/container_corners.hpp>
#include <arisa/composition/layout_slot.hpp>
#include <arisa/composition/node_cost.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/layout/layout_error.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/support/unique_handle.hpp>
#include <arisa/text/text_fitting.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <yoga/Yoga.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_map>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::bridge::yoga {

class CustomArrangement;
class GridArrangement;
class YogaLayout;

class YogaSlot final : public composition::LayoutSlot {
  struct Token {
    explicit Token() = default;
  };

  struct ConfigRelease {
    using handle_type = YGConfigRef;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type config) noexcept -> bool {
      return config != nullptr;
    }
    static void release(handle_type& config) noexcept { YGConfigFree(config); }
  };

  struct NodeRelease {
    using handle_type = YGNodeRef;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type node) noexcept -> bool {
      return node != nullptr;
    }
    static void release(handle_type& node) noexcept;
  };

 public:
  using ConfigHandle = support::UniqueHandle<ConfigRelease>;
  using NodeHandle = support::UniqueHandle<NodeRelease>;

  struct Length {
    enum class Unit : std::uint8_t {
      automatic,
      points,
      percent,
    };

    Unit unit{Unit::automatic};
    float value{0.0F};

    friend auto operator==(const Length&, const Length&) -> bool = default;
  };

  struct Style {
    YGDirection direction{YGDirectionInherit};
    YGFlexDirection flex_direction{YGFlexDirectionColumn};
    YGJustify justify{YGJustifyFlexStart};
    YGAlign align_items{YGAlignStretch};
    YGAlign align_self{YGAlignAuto};
    YGAlign align_content{YGAlignFlexStart};
    YGWrap wrap{YGWrapNoWrap};
    YGPositionType position{YGPositionTypeRelative};
    YGOverflow overflow{YGOverflowVisible};
    float grow{0.0F};
    float shrink{0.0F};
    std::optional<float> basis;
    theme::Edges padding;
    theme::Edges margin;
    float gap_row{0.0F};
    float gap_column{0.0F};
    Length width;
    Length height;
    std::optional<float> min_width;
    std::optional<float> max_width;
    std::optional<float> min_height;
    std::optional<float> max_height;
    std::optional<float> aspect_ratio;
    bool within_parent{false};

    friend auto operator==(const Style&, const Style&) -> bool = default;
  };

  struct Measured {
    std::string text;
    text::TextStyle style;
    bool wraps{true};
    provider::SurfaceScale scale{provider::SurfaceScale::unit()};
    std::uintptr_t typesetter{0};
    text::TextFitting fitting{};
    std::optional<std::size_t> reserved{};
    std::vector<std::string> variants{};

    friend auto operator==(const Measured&, const Measured&) -> bool = default;
  };

  struct Measurement {
    std::string_view text;
    text::TextStyle style;
    bool wraps{true};
    provider::SurfaceScale scale{provider::SurfaceScale::unit()};
    std::uintptr_t typesetter{0};
    text::TextFitting fitting{};
    std::optional<std::size_t> reserved{};
    std::vector<std::string> variants{};
  };

  struct Visit {
    std::uint64_t stamp{0};
    std::uint8_t role{0};
    std::uint64_t geometry{0};
    theme::DynamicTypeSize dynamic_type_size{theme::DynamicTypeSize::large};
    theme::DynamicTypeRange dynamic_type_range{theme::DynamicTypeRange::all()};
    std::uintptr_t locale{0};
    std::uint8_t direction{0};
    std::optional<theme::Font> font;
    composition::TextAttributes text;
    theme::Edges insets;
    theme::Edges safe;
    bool open_width{false};
    bool open_height{false};
    bool fixed_width{false};
    bool fixed_height{false};
    bool scrolled_width{false};
    bool scrolled_height{false};
    float shrink_weight{1.0F};
    bool grow_blocked{false};
    bool ranked{false};
    std::uint64_t cell{0};
    std::optional<composition::ContainerCorners::Enclosure> enclosure;
    bool unoriented{false};

    friend auto operator==(const Visit&, const Visit&) -> bool = default;
  };

  struct Bound {
    bool vertical{true};
    YGAlign align{YGAlignStretch};
    float gap{0.0F};
    YGDirection direction{YGDirectionLTR};

    friend auto operator==(const Bound&, const Bound&) -> bool = default;
  };

  [[nodiscard]] static auto create(std::shared_ptr<ConfigHandle> config,
                                   std::uint64_t engine)
      -> std::expected<std::unique_ptr<YogaSlot>, layout::LayoutError>;

  YogaSlot(Token, std::shared_ptr<ConfigHandle> config, NodeHandle node,
           std::uint64_t engine) noexcept;

  YogaSlot(const YogaSlot&) = delete;
  auto operator=(const YogaSlot&) -> YogaSlot& = delete;
  YogaSlot(YogaSlot&&) = delete;
  auto operator=(YogaSlot&&) -> YogaSlot& = delete;
  ~YogaSlot() override;

  auto node() const noexcept -> YGNodeRef { return node_.get(); }

  auto layout_direction() const noexcept
      -> composition::LayoutDirection override {
    return style_.has_value() && style_->direction == YGDirectionRTL
               ? composition::LayoutDirection::right_to_left
               : composition::LayoutDirection::left_to_right;
  }

  auto content() const noexcept -> YGNodeRef {
    return inner_.valid() ? inner_.get() : node_.get();
  }

  auto aligns() const noexcept -> bool { return inner_.valid(); }

  void align(const std::optional<Style>& inner);

  void aspect(const std::optional<composition::AspectRatio>& wanted);

  auto fits() const noexcept -> bool {
    return aspect_.has_value() && inner_.valid();
  }

  auto fit() -> bool;

  auto engine() const noexcept -> std::uint64_t { return engine_id_; }

  void apply(const Style& style);

  void measure(const std::optional<Measurement>& measurement,
               YogaLayout& engine, const composition::NodeCost& cost);

  void set_gap(theme::Edges gap);

  void set_children(std::span<const YGNodeRef> children);

  void bound(std::span<const YGNodeRef> children, const Bound& arrangement);

  auto bounds() const noexcept -> bool {
    return host_.valid() || grid_ != nullptr || custom_ != nullptr;
  }

  auto grid(YGDirection direction) -> GridArrangement&;

  auto custom(YGDirection direction) -> CustomArrangement&;

  void relay_grid();

  auto arrangement() const noexcept
      -> std::optional<std::reference_wrapper<const CustomArrangement>>;

  auto arrangement() noexcept
      -> std::optional<std::reference_wrapper<CustomArrangement>>;

  void set_guide(theme::Edges guide);

  void set_inset(std::optional<theme::Edges> inset);

  auto inset() const noexcept -> const std::optional<theme::Edges>& {
    return inset_;
  }

  auto set_cap(std::optional<float> width) -> bool;

  auto guide() const noexcept -> const theme::Edges& { return guide_; }

  auto set_shift(theme::Edges shift) -> bool;

  auto shift() const noexcept -> const theme::Edges& { return shift_; }

  auto set_extent(std::optional<std::array<float, 2>> extent) -> bool;

  auto set_share(std::optional<float> height) -> bool;

  auto distributes() const noexcept -> bool { return distributes_; }

  void set_distributes(bool distributes) noexcept {
    distributes_ = distributes;
  }

  auto set_corner(theme::Edges corner) -> bool;

  auto corner() const noexcept -> const theme::Edges& { return corner_; }

  auto text() const noexcept
      -> std::optional<std::reference_wrapper<const Measured>> {
    if (!measured_.has_value()) {
      return std::nullopt;
    }
    return std::cref(*measured_);
  }

  auto settle() -> bool;

  struct Publication {
    std::shared_ptr<const components::AttributedString> text;
    components::AttributedString::Custom custom;
    std::optional<theme::ColourValue> foreground;

    friend auto operator==(const Publication& left, const Publication& right)
        -> bool {
      return left.custom == right.custom &&
             left.foreground == right.foreground &&
             (left.text == right.text ||
              (left.text != nullptr && right.text != nullptr &&
               *left.text == *right.text));
    }
  };

  struct Published {
    std::shared_ptr<const text::TextLayout> layout;
    composition::Rect content{};
    bool truncated{false};
    std::optional<Publication> publication;
  };

  void set_publication(std::optional<Publication> publication) {
    if (publication_ != publication) {
      publication_ = std::move(publication);
    }
  }

  auto publication() const noexcept -> const std::optional<Publication>& {
    return publication_;
  }

  auto published() noexcept -> std::optional<Published>& { return published_; }

  static auto anchor(YGNodeRef root) noexcept -> YGNodeRef;

  auto current(const Visit& visit, std::uint64_t settled) const noexcept
      -> bool {
    if (hosts_ || !visit_.has_value()) {
      return false;
    }
    Visit expected = visit;
    if (expected.stamp == settled) {
      expected.stamp = visit_->stamp;
    }
    return *visit_ == expected;
  }

  void relate(std::optional<float> spacing) noexcept {
    relative_spacing_ = spacing;
  }

  auto relative_spacing() const noexcept -> std::optional<float> {
    return relative_spacing_;
  }

  void idealize(std::optional<float> width,
                std::optional<float> height) noexcept {
    ideal_width_ = width;
    ideal_height_ = height;
  }

  auto ideal_width() const noexcept -> std::optional<float> {
    return ideal_width_;
  }

  auto ideal_height() const noexcept -> std::optional<float> {
    return ideal_height_;
  }

  void remember(const Visit& visit, bool hosts) {
    visit_ = visit;
    hosts_ = hosts;
  }

 private:
  static auto measure_text(YGNodeConstRef node, float width,
                           YGMeasureMode width_mode, float height,
                           YGMeasureMode height_mode) -> YGSize;

  static auto baseline(YGNodeConstRef node, float width, float height) -> float;

  static void host_dirtied(YGNodeConstRef host);

  static auto measure_bound(YGNodeConstRef node, float width,
                            YGMeasureMode width_mode, float height,
                            YGMeasureMode height_mode) -> YGSize;

  static auto measure_grid(YGNodeConstRef node, float width,
                           YGMeasureMode width_mode, float height,
                           YGMeasureMode height_mode) -> YGSize;

  static auto measure_custom(YGNodeConstRef node, float width,
                             YGMeasureMode width_mode, float height,
                             YGMeasureMode height_mode) -> YGSize;

  void leave_custom();

  void write_margins(YGNodeRef node) const;

  void write_inset(YGNodeRef node) const;

  void write_bounds() const;

  void unbind();

  void leave_grid();

  auto inner_length(bool vertical) const -> float;

  void apply_fit();

  void lay_host(float cross);

  void measure_fresh(std::span<const YGNodeRef> children);

  auto natural_of(YGNodeRef child) const -> std::array<float, 2>;

  static void adopt(YGNodeRef parent, std::span<const YGNodeRef> children);

  static void set_length(YGNodeRef node, Length length, bool horizontal);

  static void write(YGNodeRef node, const Style& style, theme::Edges gap);

  std::shared_ptr<ConfigHandle> config_;
  NodeHandle node_;
  std::uint64_t engine_id_;
  std::optional<Style> style_;
  theme::Edges gap_;
  std::optional<Measured> measured_;
  std::optional<Publication> publication_;
  std::optional<Published> published_;
  std::optional<std::reference_wrapper<YogaLayout>> engine_;
  std::optional<std::reference_wrapper<const composition::NodeCost>> cost_;
  std::unique_ptr<GridArrangement> grid_;
  std::unique_ptr<CustomArrangement> custom_;
  theme::Edges guide_;
  theme::Edges corner_;
  std::optional<theme::Edges> inset_;
  std::optional<float> cap_;
  theme::Edges shift_;
  std::optional<std::array<float, 2>> extent_;
  std::optional<float> share_;
  bool distributes_{false};
  NodeHandle host_;
  NodeHandle scratch_;
  NodeHandle inner_;
  std::optional<Style> inner_style_;
  std::optional<composition::AspectRatio> aspect_;
  std::optional<std::array<float, 2>> fitted_;
  std::optional<float> natural_ratio_;
  std::flat_map<YGNodeRef, std::array<float, 2>> naturals_;
  std::optional<Bound> arrangement_;
  float cross_{0.0F};
  float main_{0.0F};
  float natural_cross_{0.0F};
  bool laid_{false};
  std::optional<Visit> visit_;
  bool hosts_{false};
  std::optional<float> relative_spacing_;
  std::optional<float> ideal_width_;
  std::optional<float> ideal_height_;
};

}
