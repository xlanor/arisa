#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::examples {

class GlassPreview final : public composition::Component {
 public:
  using Photo = std::shared_ptr<const provider::Image>;

  enum class Variant : std::uint8_t { regular, clear, identity };

  enum class Tint : std::uint8_t {
    none,
    accent,
    accent_soft,
    red,
    red_soft,
    green,
    green_soft,
    orange,
    orange_soft,
    purple,
    purple_soft,
  };

  enum class Form : std::uint8_t { capsule, rounded, circle, panel };

  enum class Size : std::uint8_t { small, medium, large };

  enum class Grouping : std::uint8_t { single, container, united };

  struct Look {
    Variant variant{Variant::regular};
    Tint tint{Tint::none};
    bool interactive{false};
    Form form{Form::capsule};
    Size size{Size::medium};
    Grouping grouping{Grouping::single};
    int count{3};
    double spacing{24.0};

    friend constexpr auto operator==(const Look&, const Look&)
        -> bool = default;
  };

  static constexpr std::string_view stage_view = "glass-stage";
  static constexpr std::string_view preview_view = "glass-preview";
  static constexpr std::array<std::string_view, 3> shape_views{
      "glass-shape-0", "glass-shape-1", "glass-shape-2"};
  static constexpr composition::Namespace unioned{"glass-union"};
  static constexpr std::string_view union_id = "glass-union";
  static constexpr float nudge = 8.0F;
  static constexpr float gap = 14.0F;
  static constexpr float row_width = 400.0F;
  static constexpr double soft_opacity = 0.3;
  static constexpr std::uint32_t sky_top = 0x4F9FE0;
  static constexpr std::uint32_t sky_bottom = 0xD7ECF8;
  static constexpr std::uint32_t ground = 0x2E6A3B;
  static constexpr composition::StateKey<composition::Point> placed{
      "glass-preview-placed", composition::Point{}};
  static constexpr composition::GestureState<composition::Size> dragged{
      "glass-preview-dragged", composition::Size{}};
  static constexpr composition::StateKey<bool> held{"glass-preview-held",
                                                    false};
  static constexpr composition::StateKey<composition::Size> room{
      "glass-preview-room", composition::Size{}};

  GlassPreview(Look look, Photo photo) noexcept
      : look_(look), photo_(std::move(photo)) {}

  auto operator==(const GlassPreview&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto glass(const Look& look) -> theme::Glass;

  static auto tint_colour(Tint tint) -> std::optional<theme::ColourValue>;

  static auto clamped(composition::Point point, composition::Size bounds)
      -> composition::Point;

  static auto nudged(composition::Point point,
                     composition::MoveCommandDirection direction,
                     composition::Size bounds) -> composition::Point;

  static auto extent(const Look& look) -> composition::Size;

  static auto outline(Form form, Size size) -> theme::Shape;

  static auto scenery(const Photo& photo) -> composition::Element;

 private:
  struct Handles {
    composition::StateHandle<composition::Point> placed;
    composition::GestureStateHandle<composition::Size> dragged;
    composition::Size bounds;
  };

  auto piece(std::size_t index, const Handles& handles,
             std::optional<composition::Namespace::ID> space) const
      -> composition::Element;

  auto pieces(const Handles& handles,
              std::optional<composition::Namespace::ID> space) const
      -> composition::Element;

  auto group(const Handles& handles, composition::Context& context) const
      -> composition::Element;

  Look look_;
  Photo photo_;
};

}
