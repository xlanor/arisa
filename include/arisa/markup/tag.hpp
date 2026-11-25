#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/markup/tag_traits.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace arisa::markup {

template <class Attribute>
concept ElementModifier =
    requires(Attribute attribute, composition::Element element) {
      {
        std::move(attribute).apply(std::move(element))
      } -> std::same_as<composition::Element>;
    };

template <class Attribute, class Options>
concept OptionOf = requires(Attribute attribute, Options& options) {
  std::move(attribute).configure(options);
};

template <class Attribute, class Tagged>
concept AttributeOf =
    OptionOf<Attribute, typename TagTraits<Tagged>::Options> ||
    ElementModifier<Attribute>;

template <class Tagged, class... Content>
concept ContentOf = requires(typename TagTraits<Tagged>::Options options,
                             Content&&... content) {
  TagTraits<Tagged>::make(std::move(options),
                          std::forward<Content>(content)...);
};

template <class Options, class... Arguments>
concept LeadingOptions =
    sizeof...(Arguments) > 0 &&
    std::same_as<
        std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Arguments...>>>,
        Options>;

template <class Tagged>
class Tag {
 public:
  using Traits = TagTraits<Tagged>;
  using Options = typename Traits::Options;

  template <class... Attributes>
  class Builder {
   public:
    Builder(const Builder&) = delete;
    auto operator=(const Builder&) -> Builder& = delete;
    Builder(Builder&&) noexcept = default;
    auto operator=(Builder&&) noexcept -> Builder& = default;
    ~Builder() = default;

    template <class... Content>
      requires ContentOf<Tagged, Content...>
    auto operator()(Content&&... content) && -> composition::Element {
      Options options = Traits::defaults();
      std::apply(
          [&options](Attributes&... each) { (configure(each, options), ...); },
          attributes_);
      composition::Element element =
          Traits::make(std::move(options), std::forward<Content>(content)...);
      std::apply(
          [&element](Attributes&... each) {
            ((element = modify(each, std::move(element))), ...);
          },
          attributes_);
      return element;
    }

   private:
    friend class Tag;

    explicit Builder(Attributes... attributes)
        : attributes_(std::move(attributes)...) {}

    template <class Attribute>
    static void configure(Attribute& attribute, Options& options) {
      if constexpr (OptionOf<Attribute, Options>) {
        std::move(attribute).configure(options);
      }
    }

    template <class Attribute>
    static auto modify(Attribute& attribute, composition::Element&& element)
        -> composition::Element {
      if constexpr (OptionOf<Attribute, Options>) {
        return std::move(element);
      } else {
        return std::move(attribute).apply(std::move(element));
      }
    }

    std::tuple<Attributes...> attributes_;
  };

  template <class... Children>
    requires(std::constructible_from<Tagged, Children...> &&
             !LeadingOptions<Options, Children...>)
  auto operator()(Children&&... children) const -> Tagged {
    Tagged tagged(std::forward<Children>(children)...);
    return tagged;
  }

  template <class... Children>
    requires std::constructible_from<Tagged, Options, Children...>
  auto operator()(Options options, Children&&... children) const -> Tagged {
    Tagged tagged(std::move(options), std::forward<Children>(children)...);
    return tagged;
  }

  template <class... Attributes>
    requires(sizeof...(Attributes) > 0 &&
             (AttributeOf<std::remove_cvref_t<Attributes>, Tagged> && ...))
  auto operator()(Attributes&&... attributes) const
      -> Builder<std::remove_cvref_t<Attributes>...> {
    return Builder<std::remove_cvref_t<Attributes>...>(
        std::forward<Attributes>(attributes)...);
  }
};

}
