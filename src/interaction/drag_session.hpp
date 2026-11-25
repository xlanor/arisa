#pragma once

#include <arisa/composition/drag_configuration.hpp>
#include <arisa/composition/drag_drop_previews_formation.hpp>
#include <arisa/composition/drag_session.hpp>
#include <arisa/composition/drop_operation.hpp>
#include <arisa/composition/drop_session.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/reorder_item.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/tree.hpp>

#include <interaction/invocation.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::interaction {

class DragSession {
 public:
  enum class Origin : std::uint8_t {
    touch,
    controller,
  };

  struct View {
    composition::NodeId source{0};
    composition::Point location{};
    composition::Size size{};
    std::optional<composition::NodeId> target;
    std::uint64_t revision{0};
    std::size_t count{1};
    composition::DragDropPreviewsFormation formation{
        composition::DragDropPreviewsFormation::automatic};
    composition::DropOperation operation{composition::DropOperation::copy};
  };

  static constexpr std::string_view pick_label = "Move";
  static constexpr std::string_view drop_label = "Drop";
  static constexpr std::string_view cancel_label = "Cancel";

  auto active() const noexcept -> bool { return source_.has_value(); }

  auto origin() const noexcept -> Origin { return origin_; }

  auto view() const -> std::optional<View>;

  auto begin(const composition::Tree& tree, composition::NodeId source,
             Origin origin, std::optional<composition::Point> location,
             std::vector<Invocation>& invocations) -> bool;

  void move(const composition::Tree& tree, composition::Point location,
            std::vector<Invocation>& invocations);

  void follow(const composition::Tree& tree,
              std::optional<composition::NodeId> focused,
              std::vector<Invocation>& invocations);

  void elapse(std::chrono::nanoseconds now,
              std::vector<Invocation>& invocations);

  auto drop(const composition::Tree& tree, std::vector<Invocation>& invocations)
      -> bool;

  void cancel(const composition::Tree& tree,
              std::vector<Invocation>& invocations);

  auto spring_target() const noexcept -> std::optional<composition::NodeId> {
    return spring_;
  }

 private:
  struct Placed {
    composition::NodeId node{0};
    composition::ReorderItem item;
    composition::Rect frame{};
  };

  static auto centre(const composition::Tree& tree, composition::NodeId node)
      -> std::optional<composition::Point>;

  static auto bounds(const composition::Tree& tree, composition::NodeId node)
      -> std::optional<composition::Rect>;

  static auto ancestry(const composition::Tree& tree, composition::NodeId node)
      -> std::vector<std::reference_wrapper<const composition::Node>>;

  static auto springs(const composition::Tree& tree, composition::NodeId node)
      -> bool;

  static void collect(const composition::Node& node,
                      const composition::Tree& tree,
                      std::vector<Placed>& found);

  auto accepting(const composition::Tree& tree,
                 std::span<const composition::NodeId> chain) const
      -> std::optional<composition::NodeId>;

  auto local(const composition::Tree& tree, composition::NodeId node) const
      -> composition::Point;

  auto reorder(const composition::Tree& tree, composition::NodeId container,
               std::optional<composition::NodeId> focused) const
      -> std::optional<composition::DropSession::ReorderTarget>;

  auto snapshot(const composition::Tree& tree, composition::NodeId target,
                composition::DropSession::Phase phase) const
      -> composition::DropSession;

  auto dragged(composition::DragSession::Phase phase) const
      -> composition::DragSession;

  auto resolve(const composition::Tree& tree) -> composition::DropOperation;

  void notify_drag(const composition::Tree& tree,
                   composition::DragSession::Phase phase,
                   std::vector<Invocation>& invocations) const;

  void notify_drop(const composition::Tree& tree, composition::NodeId target,
                   composition::DropSession::Phase phase,
                   std::vector<Invocation>& invocations) const;

  void retarget(const composition::Tree& tree,
                std::optional<composition::NodeId> target,
                std::vector<Invocation>& invocations);

  void respring(const composition::Tree& tree,
                std::span<const composition::NodeId> chain);

  void update(const composition::Tree& tree,
              std::span<const composition::NodeId> chain,
              std::optional<composition::NodeId> focused,
              std::vector<Invocation>& invocations);

  void end();

  std::optional<composition::NodeId> source_;
  std::optional<composition::NodeId> target_;
  std::vector<composition::TransferItem> items_;
  std::vector<composition::ElementKey> ids_;
  std::size_t index_{0};
  composition::DragConfiguration configuration_{};
  composition::DragDropPreviewsFormation formation_{
      composition::DragDropPreviewsFormation::automatic};
  std::optional<composition::DragDropPreviewsFormation> landing_;
  std::optional<composition::DropSession::ReorderTarget> reorder_;
  composition::DropOperation operation_{composition::DropOperation::copy};
  Origin origin_{Origin::touch};
  composition::Point location_{};
  composition::Size size_{};
  std::uint64_t revision_{0};
  std::uint64_t session_{0};
  std::uint64_t drop_session_{0};
  std::optional<composition::NodeId> spring_;
  std::optional<std::chrono::nanoseconds> spring_since_;
  bool sprung_{false};
};

}
