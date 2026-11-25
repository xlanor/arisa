#pragma once

#include <arisa/components/grid_layout.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack_layout.hpp>
#include <arisa/components/layout.hpp>
#include <arisa/components/layout_properties.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/components/view_spacing.hpp>
#include <arisa/components/vstack_layout.hpp>
#include <arisa/components/zstack_layout.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::components {

class AnyLayout {
 public:
  class Content;

  class State {
   public:
    State() = default;
    State(const State&) = delete;
    auto operator=(const State&) -> State& = delete;
    State(State&&) = delete;
    auto operator=(State&&) -> State& = delete;
    virtual ~State() = default;
  };

  class Box {
   public:
    Box() = default;
    Box(const Box&) = delete;
    auto operator=(const Box&) -> Box& = delete;
    Box(Box&&) = delete;
    auto operator=(Box&&) -> Box& = delete;
    virtual ~Box() = default;

    virtual auto make_state(const LayoutSubviews& subviews) const
        -> std::unique_ptr<State> = 0;

    virtual void update_state(State& state,
                              const LayoutSubviews& subviews) const = 0;

    virtual auto size_that_fits(ProposedViewSize proposal,
                                const LayoutSubviews& subviews,
                                State& state) const -> composition::Size = 0;

    virtual void place_subviews(composition::Rect bounds,
                                ProposedViewSize proposal,
                                const LayoutSubviews& subviews,
                                State& state) const = 0;

    virtual auto spacing(const LayoutSubviews& subviews, State& state) const
        -> ViewSpacing = 0;

    virtual auto explicit_alignment(const composition::AlignmentKey& guide,
                                    composition::Rect bounds,
                                    ProposedViewSize proposal,
                                    const LayoutSubviews& subviews,
                                    State& state) const
        -> std::optional<float> = 0;

    virtual auto aligns() const noexcept -> bool = 0;

    virtual auto properties() const noexcept -> LayoutProperties = 0;

    virtual auto kind() const noexcept -> composition::ElementType = 0;

    virtual auto equals(const Box& other) const -> bool = 0;
  };

  template <Layout L>
  class Typed;

  template <Layout L>
  explicit AnyLayout(L layout)
      : layout_(std::make_shared<const Typed<L>>(std::move(layout))) {}

  explicit AnyLayout(HStackLayout layout) noexcept : layout_(layout) {}

  explicit AnyLayout(VStackLayout layout) noexcept : layout_(layout) {}

  explicit AnyLayout(ZStackLayout layout) noexcept : layout_(layout) {}

  explicit AnyLayout(GridLayout layout) noexcept : layout_(layout) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  auto operator()(Children&&... children) const -> composition::Element {
    return (*this)(Group::members(Group(std::forward<Children>(children)...)));
  }

  auto operator()(std::vector<composition::Element> children) const
      -> composition::Element;

  friend auto operator==(const AnyLayout& left, const AnyLayout& right) -> bool;

 private:
  class Container;

  using Variant = std::variant<HStackLayout, VStackLayout, ZStackLayout,
                               GridLayout, std::shared_ptr<const Box>>;

  Variant layout_;
};

template <Layout L>
class AnyLayout::Typed final : public AnyLayout::Box {
 public:
  explicit Typed(L layout) : layout_(std::move(layout)) {}

  auto make_state(const LayoutSubviews& subviews) const
      -> std::unique_ptr<State> override {
    if constexpr (CachingLayout<L>) {
      return std::make_unique<Cached>(layout_.make_cache(subviews));
    } else {
      static_cast<void>(subviews);
      return std::make_unique<State>();
    }
  }

  void update_state(State& state,
                    const LayoutSubviews& subviews) const override {
    if constexpr (CachingLayout<L>) {
      auto& cached = static_cast<Cached&>(state);
      if constexpr (requires {
                      layout_.update_cache(cached.cache, subviews);
                    }) {
        layout_.update_cache(cached.cache, subviews);
      } else {
        cached.cache = layout_.make_cache(subviews);
      }
    } else {
      static_cast<void>(state);
      static_cast<void>(subviews);
    }
  }

  auto size_that_fits(ProposedViewSize proposal, const LayoutSubviews& subviews,
                      State& state) const -> composition::Size override {
    if constexpr (CachingLayout<L>) {
      return layout_.size_that_fits(proposal, subviews,
                                    static_cast<Cached&>(state).cache);
    } else {
      static_cast<void>(state);
      return layout_.size_that_fits(proposal, subviews);
    }
  }

  void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                      const LayoutSubviews& subviews,
                      State& state) const override {
    if constexpr (CachingLayout<L>) {
      layout_.place_subviews(bounds, proposal, subviews,
                             static_cast<Cached&>(state).cache);
    } else {
      static_cast<void>(state);
      layout_.place_subviews(bounds, proposal, subviews);
    }
  }

  auto spacing(const LayoutSubviews& subviews, State& state) const
      -> ViewSpacing override {
    if constexpr (spacing_with_cache) {
      return layout_.spacing(subviews, static_cast<Cached&>(state).cache);
    } else if constexpr (spacing_without_cache) {
      static_cast<void>(state);
      return layout_.spacing(subviews);
    } else {
      static_cast<void>(state);
      return subviews.spacing();
    }
  }

  auto explicit_alignment(const composition::AlignmentKey& guide,
                          composition::Rect bounds, ProposedViewSize proposal,
                          const LayoutSubviews& subviews, State& state) const
      -> std::optional<float> override {
    if constexpr (explicit_with_cache) {
      return layout_.explicit_alignment(guide, bounds, proposal, subviews,
                                        static_cast<Cached&>(state).cache);
    } else if constexpr (explicit_without_cache) {
      static_cast<void>(state);
      return layout_.explicit_alignment(guide, bounds, proposal, subviews);
    } else {
      static_cast<void>(guide);
      static_cast<void>(bounds);
      static_cast<void>(proposal);
      static_cast<void>(subviews);
      static_cast<void>(state);
      return std::nullopt;
    }
  }

  auto aligns() const noexcept -> bool override {
    return explicit_with_cache || explicit_without_cache;
  }

  auto properties() const noexcept -> LayoutProperties override {
    if constexpr (requires { L::layout_properties; }) {
      return L::layout_properties;
    } else {
      return LayoutProperties{};
    }
  }

  auto kind() const noexcept -> composition::ElementType override {
    return composition::ElementType::of<Typed>();
  }

  auto equals(const Box& other) const -> bool override {
    return other.kind() == kind() &&
           static_cast<const Typed&>(other).layout_ == layout_;
  }

  auto layout() const noexcept -> const L& { return layout_; }

 private:
  struct Cacheless {};

  template <class T>
  struct CacheOf {
    using type = Cacheless;
  };

  template <CachingLayout T>
  struct CacheOf<T> {
    using type = typename T::Cache;
  };

  struct Cached final : State {
    explicit Cached(typename CacheOf<L>::type made) : cache(std::move(made)) {}

    typename CacheOf<L>::type cache;
  };

  static constexpr bool spacing_with_cache =
      CachingLayout<L> &&
      requires(const L& layout, const LayoutSubviews& subviews,
               typename CacheOf<L>::type& cache) {
        { layout.spacing(subviews, cache) } -> std::convertible_to<ViewSpacing>;
      };

  static constexpr bool spacing_without_cache =
      requires(const L& layout, const LayoutSubviews& subviews) {
        { layout.spacing(subviews) } -> std::convertible_to<ViewSpacing>;
      };

  static constexpr bool explicit_with_cache =
      CachingLayout<L> &&
      requires(const L& layout, const composition::AlignmentKey& guide,
               composition::Rect bounds, ProposedViewSize proposal,
               const LayoutSubviews& subviews,
               typename CacheOf<L>::type& cache) {
        {
          layout.explicit_alignment(guide, bounds, proposal, subviews, cache)
        } -> std::convertible_to<std::optional<float>>;
      };

  static constexpr bool explicit_without_cache =
      requires(const L& layout, const composition::AlignmentKey& guide,
               composition::Rect bounds, ProposedViewSize proposal,
               const LayoutSubviews& subviews) {
        {
          layout.explicit_alignment(guide, bounds, proposal, subviews)
        } -> std::convertible_to<std::optional<float>>;
      };

  L layout_;
};

class AnyLayout::Content final : public composition::ElementContent {
 public:
  explicit Content(std::shared_ptr<const Box> box) noexcept
      : box_(std::move(box)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto kind() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto box() const noexcept -> const Box& { return *box_; }

  auto shared_box() const noexcept -> const std::shared_ptr<const Box>& {
    return box_;
  }

 private:
  std::shared_ptr<const Box> box_;
};

}
