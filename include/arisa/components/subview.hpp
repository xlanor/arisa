#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/keyed_values.hpp>

#include <utility>

namespace arisa::components {

class Subview {
 public:
  class ContainerSizingOptions {
   public:
    static const ContainerSizingOptions variable;

    static constexpr auto uniform(composition::Axes axes) noexcept
        -> ContainerSizingOptions {
      return ContainerSizingOptions{axes, true};
    }

    constexpr auto is_uniform() const noexcept -> bool { return uniform_; }

    constexpr auto axes() const noexcept -> composition::Axes { return axes_; }

    friend constexpr auto operator==(const ContainerSizingOptions&,
                                     const ContainerSizingOptions&)
        -> bool = default;

   private:
    constexpr ContainerSizingOptions(composition::Axes axes,
                                     bool uniform) noexcept
        : axes_(axes), uniform_(uniform) {}

    composition::Axes axes_;
    bool uniform_;
  };

  Subview(composition::Element element, composition::ElementKey id,
          composition::KeyedValues values)
      : element_(std::move(element)),
        id_(std::move(id)),
        values_(std::move(values)) {}

  auto id() const noexcept -> const composition::ElementKey& { return id_; }

  auto container_values() const noexcept -> const composition::KeyedValues& {
    return values_;
  }

  template <class T>
  auto operator[](const composition::ContainerValueKey<T>& key) const
      -> const T& {
    return values_.get(key);
  }

  auto view() && -> composition::Element { return std::move(element_); }

  operator composition::Element() && { return std::move(element_); }

 private:
  composition::Element element_;
  composition::ElementKey id_;
  composition::KeyedValues values_;
};

inline constexpr Subview::ContainerSizingOptions
    Subview::ContainerSizingOptions::variable{composition::Axes{}, false};

}
