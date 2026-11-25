#pragma once

#include <arisa/composition/element_type.hpp>
#include <arisa/structure/sort_order.hpp>

#include <compare>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::structure {

template <class Compared>
class KeyPathComparator {
 public:
  template <class Key>
    requires std::invocable<const Key&, const Compared&>
  explicit KeyPathComparator(Key key, SortOrder order = SortOrder::forward)
      : KeyPathComparator(std::move(key), std::compare_three_way{}, order) {}

  template <class Key, class Compare>
    requires std::invocable<const Key&, const Compared&>
  KeyPathComparator(Key key, Compare comparator,
                    SortOrder order = SortOrder::forward)
      : box_(std::make_shared<const Typed<Key, Compare>>(
            std::move(key), std::move(comparator))),
        order_(order) {}

  auto order() const noexcept -> SortOrder { return order_; }

  void set_order(SortOrder order) noexcept { order_ = order; }

  auto reversed() const -> KeyPathComparator {
    KeyPathComparator flipped = *this;
    flipped.order_ =
        order_ == SortOrder::forward ? SortOrder::reverse : SortOrder::forward;
    return flipped;
  }

  auto compare(const Compared& left, const Compared& right) const
      -> std::weak_ordering {
    const std::weak_ordering found = box_->compare(left, right);
    return order_ == SortOrder::forward ? found : 0 <=> found;
  }

  auto operator()(const Compared& left, const Compared& right) const -> bool {
    return compare(left, right) < 0;
  }

  auto same_key(const KeyPathComparator& other) const -> bool {
    return box_->equals(*other.box_);
  }

  friend auto operator==(const KeyPathComparator& left,
                         const KeyPathComparator& right) -> bool {
    return left.order_ == right.order_ && left.same_key(right);
  }

 private:
  class Box {
   public:
    Box() = default;
    Box(const Box&) = delete;
    auto operator=(const Box&) -> Box& = delete;
    Box(Box&&) = delete;
    auto operator=(Box&&) -> Box& = delete;
    virtual ~Box() = default;

    virtual auto compare(const Compared& left, const Compared& right) const
        -> std::weak_ordering = 0;

    virtual auto kind() const noexcept -> composition::ElementType = 0;

    virtual auto equals(const Box& other) const -> bool = 0;
  };

  template <class Key, class Compare>
  class Typed final : public Box {
   public:
    Typed(Key key, Compare comparator)
        : key_(std::move(key)), comparator_(std::move(comparator)) {}

    auto compare(const Compared& left, const Compared& right) const
        -> std::weak_ordering override {
      const auto& first = std::invoke(key_, left);
      const auto& second = std::invoke(key_, right);
      using Result = std::invoke_result_t<const Compare&, decltype(first),
                                          decltype(second)>;
      if constexpr (std::same_as<Result, bool>) {
        if (std::invoke(comparator_, first, second)) {
          return std::weak_ordering::less;
        }
        if (std::invoke(comparator_, second, first)) {
          return std::weak_ordering::greater;
        }
        return std::weak_ordering::equivalent;
      } else if constexpr (std::same_as<Result, std::partial_ordering>) {
        const std::partial_ordering found =
            std::invoke(comparator_, first, second);
        if (found < 0) {
          return std::weak_ordering::less;
        }
        if (found > 0) {
          return std::weak_ordering::greater;
        }
        return std::weak_ordering::equivalent;
      } else {
        return std::weak_ordering{std::invoke(comparator_, first, second)};
      }
    }

    auto kind() const noexcept -> composition::ElementType override {
      return composition::ElementType::of<Typed>();
    }

    auto equals(const Box& other) const -> bool override {
      if (other.kind() != kind()) {
        return false;
      }
      if constexpr (std::equality_comparable<Key>) {
        return static_cast<const Typed&>(other).key_ == key_;
      } else {
        return true;
      }
    }

   private:
    Key key_;
    Compare comparator_;
  };

  std::shared_ptr<const Box> box_;
  SortOrder order_;
};

}
