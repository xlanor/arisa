#pragma once

#include <bridge/yoga/yoga_slot.hpp>

#include <arisa/components/grid_item.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/motion/unit_point.hpp>

#include <yoga/Yoga.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace arisa::bridge::yoga {

class GridArrangement {
 public:
  struct Bounds {
    std::optional<float> width{};
    std::optional<float> height{};
    std::optional<float> min_width{};
    std::optional<float> min_height{};
    std::optional<float> max_width{};
    std::optional<float> max_height{};

    auto apply(float natural, bool horizontal) const noexcept -> float;

    friend auto operator==(const Bounds&, const Bounds&) -> bool = default;
  };

  struct Cell {
    YGNodeRef node{nullptr};
    std::uint32_t span{1};
    std::optional<composition::HorizontalAlignment> column_alignment{};
    std::optional<motion::UnitPoint> anchor{};
    bool flexible{false};
    bool fills{false};
    composition::Axes unsized{};
    bool text{false};
    std::optional<Bounds> bounds{};

    friend auto operator==(const Cell&, const Cell&) -> bool = default;
  };

  struct Row {
    YGNodeRef node{nullptr};
    std::optional<composition::VerticalAlignment> alignment{};
    bool spans{false};
    std::size_t begin{0};
    std::size_t end{0};

    friend auto operator==(const Row&, const Row&) -> bool = default;
  };

  struct Table {
    composition::Alignment alignment{};
    composition::Alignment placement{};
    float column_spacing{0.0F};
    std::optional<float> row_spacing{};
    float spacing{0.0F};
    float text_spacing{0.0F};

    friend auto operator==(const Table&, const Table&) -> bool = default;
  };

  struct Flow {
    std::vector<components::GridItem> items;
    std::vector<float> spacings;
    composition::Alignment alignment{};
    float spacing{0.0F};
    float leading{0.0F};
    float trailing{0.0F};
    bool vertical{true};

    friend auto operator==(const Flow&, const Flow&) -> bool = default;
  };

  using Layout = std::variant<Table, Flow>;

  GridArrangement(std::shared_ptr<YogaSlot::ConfigHandle> config,
                  YogaSlot& owner, YGDirection direction);

  GridArrangement(const GridArrangement&) = delete;
  auto operator=(const GridArrangement&) -> GridArrangement& = delete;
  GridArrangement(GridArrangement&&) = delete;
  auto operator=(GridArrangement&&) -> GridArrangement& = delete;
  ~GridArrangement();

  auto host() const noexcept -> YGNodeRef { return host_.get(); }

  auto update(Layout layout, std::span<const Cell> cells,
              std::span<const Row> rows, YGDirection direction) -> bool;

  auto settle(float cross, float along) -> bool;

  auto measure(float width, YGMeasureMode width_mode, float height,
               YGMeasureMode height_mode) const -> YGSize;

  auto vertical() const noexcept -> bool;

  void invalidate() noexcept { laid_ = false; }

 private:
  struct Line {
    float offset{0.0F};
    float extent{0.0F};
  };

  struct Placed {
    float offset{0.0F};
    float length{0.0F};
    float anchor_x{0.5F};
    float anchor_y{0.5F};
  };

  static auto fraction(composition::HorizontalAlignment alignment) noexcept
      -> float;

  static auto fraction(composition::VerticalAlignment alignment) noexcept
      -> float;

  void adopt();

  void measure_naturals();

  auto natural(YGNodeRef cell) const -> std::array<float, 2>;

  auto bounded(YGNodeRef cell, float length, bool horizontal) const -> float;

  auto definite(float cross) const noexcept -> bool;

  auto natural_cross() const -> float;

  auto flexible() const -> bool;

  void columns();

  void place_table(float cross);

  void place_flow(float cross);

  auto row_gap(std::size_t row) const -> float;

  auto lead() const -> float;

  void lay(float cross);

  void position(YGNodeRef node, float cross_offset, float cross_length,
                std::optional<float> main_offset,
                std::optional<float> main_length) const;

  std::shared_ptr<YogaSlot::ConfigHandle> config_;
  YogaSlot::NodeHandle host_;
  YGDirection direction_;
  Layout layout_{Table{}};
  std::vector<Cell> cells_;
  std::vector<Row> rows_;
  std::vector<YogaSlot::NodeHandle> wrappers_;
  std::flat_map<YGNodeRef, std::array<float, 2>> naturals_;
  std::flat_map<YGNodeRef, Bounds> bounds_;
  std::vector<Placed> placed_;
  std::vector<std::size_t> line_of_;
  std::vector<Line> lines_;
  std::vector<std::size_t> first_;
  std::vector<float> widths_;
  std::vector<bool> flexes_;
  std::vector<float> aligns_;
  std::vector<float> scratch_;
  float natural_total_{0.0F};
  float natural_{0.0F};
  bool crossed_{false};
  float cross_{0.0F};
  float along_{0.0F};
  float main_{0.0F};
  float extent_{0.0F};
  bool laid_{false};
};

}
