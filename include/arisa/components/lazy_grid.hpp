#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace arisa::components {

class LazyGrid {
 public:
  struct Arrangement {
    composition::Axis axis{composition::Axis::vertical};
    std::vector<GridItem> items{GridItem{}};
    Alignment alignment{};
    std::optional<theme::Space> spacing{};
    float overscan{240.0F};
    std::size_t retention{2};
    float estimated_extent{120.0F};

    friend auto operator==(const Arrangement&, const Arrangement&)
        -> bool = default;
  };

  using Sources = LazyStack::Sources;

  class Content;
  class Flow;
  class Cells;

  static constexpr std::size_t initial_lines = 4;
  static constexpr std::size_t located_lines = 4;

  static auto arrangement(const LazyVGridOptions& options) -> Arrangement;

  static auto arrangement(const LazyHGridOptions& options) -> Arrangement;

  static auto spacing(const Arrangement& arrangement,
                      const theme::Theme& theme) noexcept -> float;
};

class LazyGrid::Cells : public composition::Element {
 public:
  Cells(Arrangement arrangement, std::vector<composition::Element> cells,
        float leading = 0.0F, float trailing = 0.0F);
};

class LazyGrid::Flow final : public composition::ElementContent {
 public:
  Flow(Arrangement arrangement, float leading, float trailing) noexcept
      : arrangement_(std::move(arrangement)),
        leading_(leading),
        trailing_(trailing) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto arrangement() const noexcept -> const Arrangement& {
    return arrangement_;
  }

  auto leading() const noexcept -> float { return leading_; }

  auto trailing() const noexcept -> float { return trailing_; }

 private:
  Arrangement arrangement_;
  float leading_;
  float trailing_;
};

class LazyGrid::Content final : public composition::ElementContent {
 public:
  Content(Arrangement arrangement, Sources sources) noexcept
      : arrangement_(std::move(arrangement)), sources_(std::move(sources)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto materialize(composition::Context& context, const composition::Node& node,
                   composition::MoveCommandDirection direction)
      -> bool override;

  auto reveals() const noexcept -> bool override;

  auto reveal(composition::Context& context, const composition::Node& node,
              const composition::Rect& visible) -> bool override;

  auto locate(composition::Context& context, const composition::Node& node,
              const composition::ElementKey& key)
      -> std::optional<composition::Rect> override;

  auto page_stop(composition::Context& context, const composition::Node& node,
                 float position) -> std::optional<float> override;

  auto count() const noexcept -> std::size_t;

  auto arrangement() const noexcept -> const Arrangement& {
    return arrangement_;
  }

 private:
  struct Window {
    std::size_t lo{0};
    std::size_t hi{0};
    std::size_t lanes{0};
    float across{-1.0F};
    std::size_t built_lo{0};
    std::size_t built_hi{0};
    std::size_t built_lanes{0};
    Sources built_from;
    bool ready{false};
    float spacing{0.0F};
    std::vector<float> extents;
    float known_total{0.0F};
    std::size_t known{0};
    std::optional<std::size_t> kept;
    float seed{-1.0F};
  };

  struct Range {
    std::size_t lo{0};
    std::size_t hi{0};
  };

  static const composition::StateKey<Window> window_key;

  auto key(std::size_t index) const -> composition::ElementKey;

  auto element(std::size_t index) const -> composition::Element;

  auto retained(std::size_t from, std::size_t to) const -> composition::Element;

  auto vertical() const noexcept -> bool {
    return arrangement_.axis == composition::Axis::vertical;
  }

  auto guess() const noexcept -> std::size_t;

  auto lines(const Window& window) const noexcept -> std::size_t;

  auto estimate(const Window& window) const noexcept -> float;

  auto extent(const Window& window, std::size_t line) const noexcept -> float;

  auto span(const Window& window, std::size_t from,
            std::size_t to) const noexcept -> float;

  static void reset(Window& window, std::size_t lines);

  static auto flow_of(const composition::Node& node)
      -> std::optional<std::reference_wrapper<const composition::Node>>;

  void measure(Window& window, const composition::Node& node) const;

  auto relane(composition::Context& context, Window& window,
              const composition::Node& node) const -> bool;

  auto needed(const Window& window, float from, float to) const -> Range;

  Arrangement arrangement_;
  Sources sources_;
};

}
