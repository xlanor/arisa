#pragma once

#include <arisa/components/proposed_view_size.hpp>
#include <arisa/components/view_spacing.hpp>
#include <arisa/composition/keyed_values.hpp>
#include <arisa/composition/layout_value_key.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <functional>

namespace arisa::components {

class ViewThatFits;

class LayoutSubview {
 public:
  class Engine {
   public:
    Engine() = default;
    Engine(const Engine&) = delete;
    auto operator=(const Engine&) -> Engine& = delete;
    Engine(Engine&&) = delete;
    auto operator=(Engine&&) -> Engine& = delete;
    virtual ~Engine() = default;

    virtual auto size_that_fits(std::size_t index, ProposedViewSize proposal)
        -> composition::Size = 0;

    virtual auto dimensions(std::size_t index, ProposedViewSize proposal)
        -> composition::ViewDimensions = 0;

    virtual auto priority(std::size_t index) const -> double = 0;

    virtual auto spacing(std::size_t index) const -> ViewSpacing = 0;

    virtual auto values(std::size_t index) const
        -> const composition::KeyedValues& = 0;

    virtual auto container_values(std::size_t index) const
        -> const composition::KeyedValues& = 0;

    virtual auto points(std::size_t index, theme::Space space) const
        -> float = 0;

    virtual void place(std::size_t index, composition::Point position,
                       motion::UnitPoint anchor, ProposedViewSize proposal) = 0;

    virtual void hide(std::size_t index) = 0;
  };

  LayoutSubview(Engine& engine, std::size_t index) noexcept
      : engine_(engine), index_(index) {}

  auto size_that_fits(ProposedViewSize proposal) const -> composition::Size {
    return engine_.get().size_that_fits(index_, proposal);
  }

  auto dimensions(ProposedViewSize proposal) const
      -> composition::ViewDimensions {
    return engine_.get().dimensions(index_, proposal);
  }

  auto priority() const -> double { return engine_.get().priority(index_); }

  auto spacing() const -> ViewSpacing { return engine_.get().spacing(index_); }

  auto container_values() const -> const composition::KeyedValues& {
    return engine_.get().container_values(index_);
  }

  auto points(theme::Space space) const -> float {
    return engine_.get().points(index_, space);
  }

  void place(composition::Point position,
             motion::UnitPoint anchor = motion::UnitPoint::top_leading,
             ProposedViewSize proposal = ProposedViewSize::unspecified) const {
    engine_.get().place(index_, position, anchor, proposal);
  }

  template <class T>
  auto operator[](const composition::LayoutValueKey<T>& key) const -> const T& {
    return engine_.get().values(index_).get(key);
  }

  auto index() const noexcept -> std::size_t { return index_; }

  friend auto operator==(const LayoutSubview& left,
                         const LayoutSubview& right) noexcept -> bool {
    return &left.engine_.get() == &right.engine_.get() &&
           left.index_ == right.index_;
  }

 private:
  friend class ViewThatFits;

  void hide() const { engine_.get().hide(index_); }

  std::reference_wrapper<Engine> engine_;
  std::size_t index_;
};

}
