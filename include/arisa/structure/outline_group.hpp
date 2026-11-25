#pragma once

#include <arisa/components/group.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/structure/disclosure_group.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

class OutlineGroup : public components::Group {
 public:
  template <std::ranges::input_range Items, class KeyOf, class ChildrenOf,
            class Build>
    requires std::convertible_to<
                 std::invoke_result_t<const KeyOf&,
                                      const std::ranges::range_value_t<Items>&>,
                 composition::ElementKey> &&
             std::convertible_to<
                 std::invoke_result_t<const Build&,
                                      const std::ranges::range_value_t<Items>&>,
                 composition::Element> &&
             std::invocable<const ChildrenOf&,
                            const std::ranges::range_value_t<Items>&>
  OutlineGroup(Items&& roots, KeyOf key_of, ChildrenOf children_of, Build build)
      : Group(nodes(std::forward<Items>(roots) |
                        std::ranges::to<
                            std::vector<std::ranges::range_value_t<Items>>>(),
                    std::move(key_of), std::move(children_of),
                    std::move(build))) {}

 private:
  template <class Item>
  static auto children(std::vector<Item> found) -> std::vector<Item> {
    return found;
  }

  template <class Item>
  static auto children(std::optional<std::vector<Item>> found)
      -> std::vector<Item> {
    return found.value_or(std::vector<Item>{});
  }

  template <class Item, class KeyOf, class ChildrenOf, class Build>
  static auto nodes(std::vector<Item> items, KeyOf key_of,
                    ChildrenOf children_of, Build build)
      -> std::vector<composition::Element> {
    std::vector<composition::Element> built;
    built.reserve(items.size());
    for (const Item& item : items) {
      std::vector<Item> below = children<Item>(std::invoke(children_of, item));
      composition::ElementKey key{std::invoke(key_of, item)};
      if (below.empty()) {
        built.push_back(
            composition::Element{std::invoke(build, item)}.id(std::move(key)));
        continue;
      }
      built.push_back(composition::Element{
          DisclosureGroup(
              composition::Element{std::invoke(build, item)},
              [below = std::move(below), key_of, children_of, build] {
                return composition::Element{
                    OutlineGroup(below, key_of, children_of, build)};
              })}.id(std::move(key)));
    }
    return built;
  }
};

}
