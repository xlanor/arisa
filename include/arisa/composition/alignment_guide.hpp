#pragma once

#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

class ViewDimensions;

class AlignmentGuide {
 public:
  template <class Compute>
    requires std::convertible_to<
                 std::invoke_result_t<const Compute&, const ViewDimensions&>,
                 float>
  AlignmentGuide(AlignmentKey key, Compute compute)
      : key_(key),
        compute_(std::make_shared<const Typed<Compute>>(std::move(compute))) {}

  auto key() const noexcept -> const AlignmentKey& { return key_; }

  auto value(const ViewDimensions& dimensions) const -> float {
    return compute_->value(dimensions);
  }

  friend auto operator==(const AlignmentGuide& left,
                         const AlignmentGuide& right) -> bool {
    return left.key_ == right.key_ && (left.compute_ == right.compute_ ||
                                       left.compute_->equals(*right.compute_));
  }

 private:
  class Compute {
   public:
    Compute() = default;
    Compute(const Compute&) = delete;
    auto operator=(const Compute&) -> Compute& = delete;
    Compute(Compute&&) = delete;
    auto operator=(Compute&&) -> Compute& = delete;
    virtual ~Compute() = default;

    virtual auto value(const ViewDimensions& dimensions) const -> float = 0;

    virtual auto kind() const noexcept -> ElementType = 0;

    virtual auto equals(const Compute& other) const -> bool = 0;
  };

  template <class F>
  class Typed final : public Compute {
   public:
    explicit Typed(F compute) : compute_(std::move(compute)) {}

    auto value(const ViewDimensions& dimensions) const -> float override {
      return static_cast<float>(std::invoke(compute_, dimensions));
    }

    auto kind() const noexcept -> ElementType override {
      return ElementType::of<Typed>();
    }

    auto equals(const Compute& other) const -> bool override {
      if (other.kind() != kind()) {
        return false;
      }
      if constexpr (std::equality_comparable<F>) {
        return static_cast<const Typed&>(other).compute_ == compute_;
      } else {
        return std::is_empty_v<F>;
      }
    }

   private:
    F compute_;
  };

  AlignmentKey key_;
  std::shared_ptr<const Compute> compute_;
};

}
