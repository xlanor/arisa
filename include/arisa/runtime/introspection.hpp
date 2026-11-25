#pragma once

#include <arisa/components/accessibility_tree.hpp>

#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_cost.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/gpu_inventory.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/surface_directory.hpp>
#include <arisa/provider/gpu/texture_publications.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/rendering/render_target_pool.hpp>
#include <arisa/theme/space.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::interaction {

class InputRouter;

}

namespace arisa::runtime {

class PainterRenderer;
class Root;

class Introspection {
 public:
  static constexpr std::array directions{
      composition::MoveCommandDirection::up,
      composition::MoveCommandDirection::down,
      composition::MoveCommandDirection::left,
      composition::MoveCommandDirection::right};

  struct Flags {
    bool focusable{false};
    bool clips{false};
    bool glass{false};
    bool promoted{false};
    bool layer{false};
    bool animating{false};

    friend constexpr auto operator==(const Flags&, const Flags&)
        -> bool = default;
  };

  struct NodeRecord {
    composition::NodeId id{0};
    std::optional<composition::NodeId> parent{};
    std::size_t depth{0};
    std::string key;
    std::string name;
    composition::Rect frame{};
    composition::Rect bounds{};
    std::optional<theme::Insets> padding{};
    Flags flags{};
    composition::NodeCost::Counts cost{};
    std::size_t children{0};
    std::vector<std::string> notes;
  };

  struct CostRecord {
    composition::NodeId id{0};
    composition::NodeCost::Counts cost{};
  };

  struct FocusRecord {
    std::optional<composition::NodeId> focused{};
    std::vector<composition::NodeId> chain;
    std::vector<composition::NodeId> scopes;
    std::array<std::optional<composition::NodeId>, directions.size()> next{};
  };

  struct GpuRecord {
    provider::GpuInventory inventory;
    std::vector<provider::FrameSchedule::Listing> hooks;
    std::vector<provider::TexturePublications::Listing> external;
    std::vector<provider::SurfaceDirectory::Listing> surfaces;
    std::uint64_t atlas_bytes{0};
    std::size_t atlas_pages{0};
    rendering::RenderTargetPool::Statistics targets{};
  };

  Introspection(const Root& root, const interaction::InputRouter& router,
                const PainterRenderer& renderer,
                provider::GpuProvider& gpu) noexcept;

  auto nodes() const -> std::vector<NodeRecord>;

  auto node(composition::NodeId id) const -> std::optional<NodeRecord>;

  auto costs() const -> std::vector<CostRecord>;

  auto hit(composition::Point point) const
      -> std::optional<composition::NodeId>;

  auto points(provider::PixelPoint pixel) const -> composition::Point;

  auto viewport() const -> composition::Size;

  auto focus() const -> FocusRecord;

  auto gpu() const -> GpuRecord;

  auto accessibility(composition::NodeId id) const
      -> std::optional<components::AccessibilityElement>;

  static auto readable(std::string_view spelled) -> std::string;

  auto serialized_data() const -> std::vector<std::byte>;

 private:
  auto tree() const -> const composition::Tree&;

  auto shown(const composition::Node& node) const -> bool;

  auto record(const composition::Node& node) const -> NodeRecord;

  auto walk() const
      -> std::vector<std::reference_wrapper<const composition::Node>>;

  auto deepest(const composition::Node& node, composition::Point point) const
      -> std::optional<composition::NodeId>;

  static auto bounds_of(const composition::Node& node) -> composition::Rect;

  static auto scoping(const composition::Node& node) -> bool;

  std::reference_wrapper<const Root> root_;
  std::reference_wrapper<const interaction::InputRouter> router_;
  std::reference_wrapper<const PainterRenderer> renderer_;
  std::reference_wrapper<provider::GpuProvider> gpu_;
};

}
