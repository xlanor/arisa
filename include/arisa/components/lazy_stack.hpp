#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::components {

template <class T>
concept LazyChild = composition::ViewContent<T> ||
                    std::same_as<std::remove_cvref_t<T>, GridSection>;

template <class... Children>
concept MixedLazyChildren =
    sizeof...(Children) > 0 && (LazyChild<Children> && ...) &&
    !(std::same_as<std::remove_cvref_t<Children>, ForEach> && ...) &&
    !(std::same_as<std::remove_cvref_t<Children>, GridSection> && ...);

class LazyStack {
 public:
  struct Arrangement {
    composition::Axis axis{composition::Axis::vertical};
    HorizontalAlignment horizontal{HorizontalAlignment::center};
    VerticalAlignment vertical{VerticalAlignment::center};
    theme::Space spacing{theme::space::sm};
    float overscan{240.0F};
    std::size_t retention{4};
    float estimated_extent{44.0F};
  };

  using Sources = std::vector<std::shared_ptr<const ForEach::Source>>;

  struct Run {
    std::size_t lo{0};
    std::size_t hi{0};
  };

  using Part =
      std::variant<composition::Element, std::shared_ptr<const ForEach::Source>,
                   GridSection>;

  using Parts = std::vector<Part>;

  using Build = std::function<composition::Element(Sources)>;

  class Content;

  static constexpr std::size_t initial_count = 16;
  static constexpr std::size_t located_span = 16;

  static auto arrangement(const LazyVStackOptions& options) noexcept
      -> Arrangement;

  static auto arrangement(const LazyHStackOptions& options) noexcept
      -> Arrangement;

  static auto source_of(const composition::Element& each)
      -> std::shared_ptr<const ForEach::Source>;

  static auto repeated(const Sources& now, const Sources& before,
                       std::size_t from, std::size_t to) -> std::vector<Run>;

  static auto sections(
      std::vector<GridSection>& sections, PinnedScrollableViews pinned,
      composition::Axis axis, theme::Space spacing,
      const std::function<composition::Element(Sources)>& build)
      -> std::vector<composition::Element>;

  static auto composed(Parts& parts, PinnedScrollableViews pinned,
                       composition::Axis axis, theme::Space spacing,
                       const Build& build) -> std::vector<composition::Element>;

  template <class... Children>
    requires(LazyChild<Children> && ...)
  static auto parts_of(Children&&... children) -> Parts {
    Parts parts;
    parts.reserve(sizeof...(Children));
    const auto add = [&parts]<class Child>(Child&& child) {
      if constexpr (std::same_as<std::remove_cvref_t<Child>, GridSection>) {
        parts.emplace_back(std::in_place_type<GridSection>,
                           std::forward<Child>(child));
      } else if constexpr (std::same_as<std::remove_cvref_t<Child>, ForEach>) {
        parts.emplace_back(source_of(child));
      } else if constexpr (std::convertible_to<Child, composition::Element>) {
        parts.emplace_back(std::in_place_type<composition::Element>,
                           std::forward<Child>(child));
      } else if (child.has_value()) {
        parts.emplace_back(std::in_place_type<composition::Element>,
                           *std::forward<Child>(child));
      }
    };
    (add(std::forward<Children>(children)), ...);
    static_cast<void>(add);
    return parts;
  }

 private:
  static void place(GridSection& section, std::size_t index,
                    PinnedScrollableViews pinned, composition::Axis axis,
                    theme::Space spacing, const Build& build,
                    std::vector<composition::Element>& column);
};

class LazyStack::Content final : public composition::ElementContent {
 public:
  Content(Arrangement arrangement, Sources sources) noexcept
      : arrangement_(arrangement), sources_(std::move(sources)) {}

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

  auto count() const noexcept -> std::size_t;

  auto arrangement() const noexcept -> const Arrangement& {
    return arrangement_;
  }

 private:
  struct Window {
    std::size_t lo{0};
    std::size_t hi{0};
    std::size_t built_lo{0};
    std::size_t built_hi{0};
    Sources built_from;
    bool ready{false};
    float spacing{0.0F};
    std::vector<float> extents;
    float known_total{0.0F};
    std::size_t known{0};
    std::optional<std::size_t> kept;
  };

  struct Range {
    std::size_t lo{0};
    std::size_t hi{0};
  };

  static const composition::StateKey<Window> window_key;

  auto key(std::size_t index) const -> composition::ElementKey;

  auto element(std::size_t index) const -> composition::Element;

  auto retained(std::size_t from, std::size_t to) const -> composition::Element;

  auto estimate(const Window& window) const noexcept -> float;

  auto extent(const Window& window, std::size_t index) const noexcept -> float;

  auto position(const Window& window, std::size_t index) const noexcept
      -> float;

  auto span(const Window& window, std::size_t from,
            std::size_t to) const noexcept -> float;

  auto placeholder(float length) const -> composition::Element;

  auto vertical() const noexcept -> bool {
    return arrangement_.axis == composition::Axis::vertical;
  }

  static auto stack_of(const composition::Node& node)
      -> std::optional<std::reference_wrapper<const composition::Node>>;

  void measure(Window& window, const composition::Node& node) const;

  auto needed(const Window& window, float from, float to) const -> Range;

  Arrangement arrangement_;
  Sources sources_;
};

}
