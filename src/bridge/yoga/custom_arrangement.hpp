#pragma once

#include <bridge/yoga/yoga_slot.hpp>

#include <arisa/components/any_layout.hpp>
#include <arisa/components/layout_subview.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/components/view_spacing.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/keyed_values.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <yoga/Yoga.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace arisa::bridge::yoga {

class CustomArrangement final : public components::LayoutSubview::Engine {
 public:
  struct Member {
    YGNodeRef node{nullptr};
    std::reference_wrapper<const composition::Node> source;
    std::reference_wrapper<const theme::Theme> theme;
    std::optional<std::reference_wrapper<const composition::Modifiers>>
        carried{};
    std::optional<std::reference_wrapper<const composition::KeyedValues>>
        carried_values{};
    components::ViewSpacing spacing{};
    std::optional<float> ideal_width{};
    std::optional<float> ideal_height{};
  };

  using Reader =
      std::function<composition::TextBaselines(const composition::Node&)>;

  CustomArrangement(std::shared_ptr<YogaSlot::ConfigHandle> config,
                    YogaSlot& owner, YGDirection direction);

  CustomArrangement(const CustomArrangement&) = delete;
  auto operator=(const CustomArrangement&) -> CustomArrangement& = delete;
  CustomArrangement(CustomArrangement&&) = delete;
  auto operator=(CustomArrangement&&) -> CustomArrangement& = delete;
  ~CustomArrangement() override;

  auto host() const noexcept -> YGNodeRef { return host_.get(); }

  auto update(std::shared_ptr<const components::AnyLayout::Box> box,
              std::span<const Member> members, YGDirection direction,
              Reader reader) -> bool;

  auto measure(float width, YGMeasureMode width_mode, float height,
               YGMeasureMode height_mode) -> YGSize;

  void settle(float width, float height);

  auto members() const noexcept -> std::span<const Member> { return members_; }

  auto hidden(std::size_t index) const -> bool;

  auto layout_spacing() -> components::ViewSpacing;

  auto explicit_alignment(const composition::AlignmentKey& key, float width,
                          float height) -> std::optional<float>;

  auto size_that_fits(std::size_t index, components::ProposedViewSize proposal)
      -> composition::Size override;

  auto dimensions(std::size_t index, components::ProposedViewSize proposal)
      -> composition::ViewDimensions override;

  auto priority(std::size_t index) const -> double override;

  auto spacing(std::size_t index) const -> components::ViewSpacing override;

  auto values(std::size_t index) const
      -> const composition::KeyedValues& override;

  auto container_values(std::size_t index) const
      -> const composition::KeyedValues& override;

  auto points(std::size_t index, theme::Space space) const -> float override;

  void place(std::size_t index, composition::Point position,
             motion::UnitPoint anchor,
             components::ProposedViewSize proposal) override;

  void hide(std::size_t index) override;

 private:
  struct Measured {
    components::ProposedViewSize proposal;
    composition::Size size;
    std::optional<composition::TextBaselines> baselines{};
  };

  struct Valued {
    composition::KeyedValues values;
    double priority{0.0};
  };

  struct Placement {
    composition::Point position{};
    motion::UnitPoint anchor{motion::UnitPoint::top_leading};
    components::ProposedViewSize proposal{};
    bool placed{false};
    bool hidden{false};
  };

  static constexpr float huge = 1.0e7F;
  static constexpr std::size_t remembered = 8;

  static auto proposed(float length, YGMeasureMode mode) noexcept
      -> std::optional<float>;

  static auto offered(std::optional<float> length) noexcept -> float;

  static auto reported(float length) noexcept -> float;

  static void restore(YGNodeRef node, YGValue value, bool across) noexcept;

  static auto yielded(float length, float offered, YGMeasureMode mode) noexcept
      -> float;

  static auto modifiers_of(const Member& member)
      -> const composition::Modifiers&;

  auto modifiers(std::size_t index) const -> const composition::Modifiers&;

  auto revalued(std::span<const Member> members) const -> bool;

  void remember_values();

  auto subviews() const -> components::LayoutSubviews;

  void adopt();

  auto measured(std::size_t index, components::ProposedViewSize proposal,
                bool read) -> const Measured&;

  void orient();

  std::shared_ptr<YogaSlot::ConfigHandle> config_;
  YogaSlot::NodeHandle host_;
  YogaSlot::NodeHandle scratch_;
  YGDirection direction_;
  std::shared_ptr<const components::AnyLayout::Box> box_;
  std::unique_ptr<components::AnyLayout::State> state_;
  std::vector<Member> members_;
  std::vector<YogaSlot::NodeHandle> wrappers_;
  std::vector<components::LayoutSubview> subviews_;
  std::vector<std::vector<Measured>> measured_;
  std::vector<Valued> valued_;
  std::vector<Placement> placements_;
  Reader reader_;
  YGFlexDirection flow_{YGFlexDirectionColumn};
  float width_{0.0F};
  float height_{0.0F};
  bool laid_{false};
};

}
