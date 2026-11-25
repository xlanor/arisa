#pragma once

#include <arisa/composition/animatable_data.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/motion/transaction.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class First, class Second>
class ConcatenatedModifier;

template <class Modifier>
class TransactedModifier;

class ViewModifier {
 public:
  using Content = PlaceholderContentView;
  using TransactionEdit = std::function<void(motion::Transaction&)>;

  virtual ~ViewModifier() = default;

  virtual auto body(Context& context, Content content) const -> Element = 0;

  template <class Self, class Other>
  auto concat(this const Self& self, Other other)
      -> ConcatenatedModifier<Self, Other> {
    return ConcatenatedModifier<Self, Other>{self, std::move(other)};
  }

  template <class Self>
  auto transaction(this const Self& self, TransactionEdit edit)
      -> TransactedModifier<Self> {
    return TransactedModifier<Self>{self, std::move(edit)};
  }

  auto operator==(const ViewModifier&) const -> bool = default;
};

template <class Modifier>
concept ViewModifierValue =
    std::derived_from<Modifier, ViewModifier> &&
    std::move_constructible<Modifier> && std::equality_comparable<Modifier>;

class EmptyModifier final : public ViewModifier {
 public:
  static const EmptyModifier identity;

  EmptyModifier() noexcept = default;

  auto body(Context& context, Content content) const -> Element override;

  friend auto operator==(const EmptyModifier&, const EmptyModifier&)
      -> bool = default;
};

inline const EmptyModifier EmptyModifier::identity{};

template <class First, class Second>
class ConcatenatedModifier final : public ViewModifier {
 public:
  ConcatenatedModifier(First first, Second second)
      : first_(std::move(first)), second_(std::move(second)) {}

  auto body(Context& context, Content content) const -> Element override {
    Element inner = first_.body(context, std::move(content));
    return second_.body(
        context,
        PlaceholderContentView{std::make_shared<PlaceholderContentView::Source>(
            std::move(inner))});
  }

  auto first() const noexcept -> const First& { return first_; }

  auto second() const noexcept -> const Second& { return second_; }

  friend auto operator==(const ConcatenatedModifier&,
                         const ConcatenatedModifier&) -> bool = default;

 private:
  First first_;
  Second second_;
};

template <class Modifier>
class TransactedModifier final : public ViewModifier {
 public:
  TransactedModifier(Modifier modifier, TransactionEdit edit)
      : modifier_(std::move(modifier)),
        edit_(std::make_shared<const TransactionEdit>(std::move(edit))) {}

  auto body(Context& context, Content content) const -> Element override {
    return modifier_.body(context, std::move(content))
        .transaction([edit = edit_](motion::Transaction& transaction) {
          if (*edit) {
            (*edit)(transaction);
          }
        });
  }

  auto modifier() const noexcept -> const Modifier& { return modifier_; }

  friend auto operator==(const TransactedModifier& left,
                         const TransactedModifier& right) -> bool {
    return left.modifier_ == right.modifier_ && left.edit_ == right.edit_;
  }

 private:
  Modifier modifier_;
  std::shared_ptr<const TransactionEdit> edit_;
};

template <ViewModifierValue Modifier>
class ModifiedContent final : public Component {
 public:
  ModifiedContent(Element content, Modifier modifier)
      : source_(std::make_shared<PlaceholderContentView::Source>(
            std::move(content))),
        modifier_(std::move(modifier)) {}

  auto operator==(const ModifiedContent& other) const -> bool {
    return source_ == other.source_ && modifier_ == other.modifier_;
  }

  auto body(Context& context) const -> Element override {
    if constexpr (AnimatableView<Modifier>) {
      return AnimatableData::presented(context, modifier_)
          .body(context, PlaceholderContentView{source_});
    } else {
      return modifier_.body(context, PlaceholderContentView{source_});
    }
  }

  auto modifier() const noexcept -> const Modifier& { return modifier_; }

 private:
  std::shared_ptr<PlaceholderContentView::Source> source_;
  Modifier modifier_;
};

template <class Modifier>
auto Element::modifier(Modifier modifier) && -> Element {
  static_assert(ViewModifierValue<Modifier>,
                "a modifier derives from ViewModifier and compares equal");
  return ModifiedContent<Modifier>{std::move(*this), std::move(modifier)};
}

}
