#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/vertical_edge.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::components {

class SafeAreaInset {
 public:
  static constexpr float inset_spacing = 8.0F;
  static constexpr float bar_spacing = 0.0F;

  static auto attach(composition::Element base, theme::Edge edge,
                     composition::Alignment alignment, float spacing,
                     composition::Element content, bool bar)
      -> composition::Element;

  static auto edge(composition::VerticalEdge edge) noexcept -> theme::Edge;

  static auto edge(composition::HorizontalEdge edge) noexcept -> theme::Edge;

  static auto placed(composition::VerticalEdge edge,
                     composition::HorizontalAlignment alignment) noexcept
      -> composition::Alignment;

  static auto placed(composition::HorizontalEdge edge,
                     composition::VerticalAlignment alignment) noexcept
      -> composition::Alignment;

  template <class Content>
  static auto made(Content content) -> composition::Element {
    if constexpr (std::convertible_to<Content, composition::Element>) {
      return composition::Element{std::move(content)};
    } else {
      return composition::Element{std::invoke(content)};
    }
  }

 private:
  static auto fade(theme::Edge edge) -> composition::Element;
};

}

namespace arisa::composition {

template <class Content>
auto Element::safe_area_inset(VerticalEdge edge,
                              Content content) && -> Element {
  return std::move(*this).safe_area_inset(edge, HorizontalAlignment::center,
                                          std::nullopt, std::move(content));
}

template <class Content>
auto Element::safe_area_inset(VerticalEdge edge, HorizontalAlignment alignment,
                              std::optional<float> spacing,
                              Content content) && -> Element {
  using components::SafeAreaInset;
  return SafeAreaInset::attach(std::move(*this), SafeAreaInset::edge(edge),
                               SafeAreaInset::placed(edge, alignment),
                               spacing.value_or(SafeAreaInset::inset_spacing),
                               SafeAreaInset::made(std::move(content)), false);
}

template <class Content>
auto Element::safe_area_inset(HorizontalEdge edge,
                              Content content) && -> Element {
  return std::move(*this).safe_area_inset(edge, VerticalAlignment::center,
                                          std::nullopt, std::move(content));
}

template <class Content>
auto Element::safe_area_inset(HorizontalEdge edge, VerticalAlignment alignment,
                              std::optional<float> spacing,
                              Content content) && -> Element {
  using components::SafeAreaInset;
  return SafeAreaInset::attach(std::move(*this), SafeAreaInset::edge(edge),
                               SafeAreaInset::placed(edge, alignment),
                               spacing.value_or(SafeAreaInset::inset_spacing),
                               SafeAreaInset::made(std::move(content)), false);
}

template <class Content>
auto Element::safe_area_bar(VerticalEdge edge, Content content) && -> Element {
  return std::move(*this).safe_area_bar(edge, HorizontalAlignment::center,
                                        std::nullopt, std::move(content));
}

template <class Content>
auto Element::safe_area_bar(VerticalEdge edge, HorizontalAlignment alignment,
                            std::optional<float> spacing,
                            Content content) && -> Element {
  using components::SafeAreaInset;
  return SafeAreaInset::attach(std::move(*this), SafeAreaInset::edge(edge),
                               SafeAreaInset::placed(edge, alignment),
                               spacing.value_or(SafeAreaInset::bar_spacing),
                               SafeAreaInset::made(std::move(content)), true);
}

template <class Content>
auto Element::safe_area_bar(HorizontalEdge edge,
                            Content content) && -> Element {
  return std::move(*this).safe_area_bar(edge, VerticalAlignment::center,
                                        std::nullopt, std::move(content));
}

template <class Content>
auto Element::safe_area_bar(HorizontalEdge edge, VerticalAlignment alignment,
                            std::optional<float> spacing,
                            Content content) && -> Element {
  using components::SafeAreaInset;
  return SafeAreaInset::attach(std::move(*this), SafeAreaInset::edge(edge),
                               SafeAreaInset::placed(edge, alignment),
                               spacing.value_or(SafeAreaInset::bar_spacing),
                               SafeAreaInset::made(std::move(content)), true);
}

}
