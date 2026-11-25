#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_role.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/shape.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::graphics {

class AnyShape;

template <class S>
concept RoundedRectangularShape = requires(
    const S& shape, std::optional<composition::Size> size) {
  {
    shape.corners(size)
  }
  -> std::convertible_to<std::optional<theme::RoundedRectangularShapeCorners>>;
};

template <class S>
concept PathShape = !std::derived_from<std::remove_cvref_t<S>, theme::Shape> &&
                    !std::same_as<std::remove_cvref_t<S>, Path> &&
                    !std::same_as<std::remove_cvref_t<S>, AnyShape> &&
                    (requires(const S& shape, composition::Rect rect) {
                      { shape.path(rect) } -> std::convertible_to<Path>;
                    });

class AnyShape {
 public:
  AnyShape(theme::Shape shape) : value_(shape) {}

  AnyShape(Path path) : value_(std::move(path)) {}

  template <PathShape S>
  AnyShape(S shape)
      : value_(std::static_pointer_cast<const Model>(
            std::make_shared<const Holder<std::remove_cvref_t<S>>>(
                std::move(shape)))) {}

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto inset(float amount) const -> AnyShape;

  auto scale(float x, float y,
             motion::UnitPoint anchor = motion::UnitPoint::center) const
      -> AnyShape;

  auto scale(float amount,
             motion::UnitPoint anchor = motion::UnitPoint::center) const
      -> AnyShape;

  auto rotation(motion::Angle angle,
                motion::UnitPoint anchor = motion::UnitPoint::center) const
      -> AnyShape;

  auto offset(float x, float y) const -> AnyShape;

  auto transform(const AffineTransform& transform) const -> AnyShape;

  auto union_with(AnyShape other, bool eo_fill = false) const -> AnyShape;

  auto intersection(AnyShape other, bool eo_fill = false) const -> AnyShape;

  auto subtracting(AnyShape other, bool eo_fill = false) const -> AnyShape;

  auto symmetric_difference(AnyShape other, bool eo_fill = false) const
      -> AnyShape;

  auto line_intersection(AnyShape other, bool eo_fill = false) const
      -> AnyShape;

  auto line_subtraction(AnyShape other, bool eo_fill = false) const -> AnyShape;

  auto size(composition::Size size) const -> AnyShape;

  auto size(composition::Size size, motion::UnitPoint anchor) const -> AnyShape;

  auto size(float width, float height) const -> AnyShape;

  auto size(float width, float height, motion::UnitPoint anchor) const
      -> AnyShape;

  auto role() const -> ShapeRole;

  auto corners(std::optional<composition::Size> size) const
      -> std::optional<theme::RoundedRectangularShapeCorners>;

  auto inset_amount() const noexcept -> float { return inset_; }

  auto leaf() const -> std::optional<theme::Shape>;

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior;

  auto path_mirrors_in(composition::LayoutDirection direction) const -> bool;

  friend auto operator==(const AnyShape& left, const AnyShape& right) -> bool;

 private:
  class Model {
   public:
    Model() = default;
    Model(const Model&) = delete;
    auto operator=(const Model&) -> Model& = delete;
    Model(Model&&) = delete;
    auto operator=(Model&&) -> Model& = delete;
    virtual ~Model() = default;

    virtual auto path(composition::Rect rect,
                      const theme::RectangleCornerRadii& radii) const
        -> Path = 0;

    virtual auto inset(float amount) const -> std::shared_ptr<const Model> = 0;

    virtual auto kind() const noexcept -> const void* = 0;

    virtual auto leaf() const -> std::optional<theme::Shape> = 0;

    virtual auto layout_direction_behavior() const
        -> composition::LayoutDirectionBehavior = 0;

    virtual auto equals(const Model& other) const -> bool = 0;

    virtual auto role() const -> ShapeRole = 0;

    virtual auto corners(std::optional<composition::Size> size) const
        -> std::optional<theme::RoundedRectangularShapeCorners> = 0;
  };

  template <class S>
  class Holder final : public Model {
   public:
    explicit Holder(S shape) : shape_(std::move(shape)) {}

    auto path(composition::Rect rect,
              const theme::RectangleCornerRadii& radii) const -> Path override {
      if constexpr (requires { shape_.path(rect, radii); }) {
        return shape_.path(rect, radii);
      } else {
        return shape_.path(rect);
      }
    }

    auto inset(float amount) const -> std::shared_ptr<const Model> override {
      if constexpr (requires {
                      { shape_.inset(amount) } -> PathShape;
                    }) {
        using Inset = std::remove_cvref_t<decltype(shape_.inset(amount))>;
        return std::make_shared<const Holder<Inset>>(shape_.inset(amount));
      } else {
        return nullptr;
      }
    }

    auto kind() const noexcept -> const void* override { return &tag; }

    auto leaf() const -> std::optional<theme::Shape> override {
      if constexpr (requires {
                      {
                        shape_.shape()
                      } -> std::convertible_to<const AnyShape&>;
                    }) {
        return shape_.shape().leaf();
      } else {
        return std::nullopt;
      }
    }

    auto layout_direction_behavior() const
        -> composition::LayoutDirectionBehavior override {
      if constexpr (requires {
                      {
                        shape_.layout_direction_behavior()
                      } -> std::convertible_to<
                          composition::LayoutDirectionBehavior>;
                    }) {
        return shape_.layout_direction_behavior();
      } else {
        return composition::LayoutDirectionBehavior::mirrors();
      }
    }

    auto equals(const Model& other) const -> bool override {
      if (other.kind() != kind()) {
        return false;
      }
      if constexpr (std::equality_comparable<S>) {
        return static_cast<const Holder&>(other).shape_ == shape_;
      } else {
        return &other == this;
      }
    }

    auto role() const -> ShapeRole override {
      if constexpr (requires {
                      { shape_.role() } -> std::convertible_to<ShapeRole>;
                    }) {
        return shape_.role();
      } else if constexpr (requires {
                             { S::role } -> std::convertible_to<ShapeRole>;
                           }) {
        return S::role;
      } else {
        return ShapeRole::fill;
      }
    }

    auto corners(std::optional<composition::Size> size) const
        -> std::optional<theme::RoundedRectangularShapeCorners> override {
      if constexpr (RoundedRectangularShape<S>) {
        return shape_.corners(size);
      } else {
        return std::nullopt;
      }
    }

   private:
    static constexpr char tag{};

    S shape_;
  };

  AnyShape(std::shared_ptr<const Model> model, float inset)
      : value_(std::move(model)), inset_(inset) {}

  static auto inset_rect(composition::Rect rect, float amount) noexcept
      -> composition::Rect;

  std::variant<theme::Shape, Path, std::shared_ptr<const Model>> value_;
  float inset_{0.0F};
};

}
