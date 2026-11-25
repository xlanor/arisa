#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transaction_stack.hpp>
#include <arisa/motion/with_animation.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::composition {

template <class T>
class Binding {
 public:
  using Getter = std::move_only_function<T() const>;
  using Setter = std::move_only_function<void(T) const>;
  using TransactedSetter =
      std::move_only_function<void(T, const motion::Transaction&) const>;

  Binding(StateHandle<T> state) noexcept : source_(std::move(state)) {}

  Binding(Getter get, Setter set)
      : source_(Computed{std::make_shared<const Accessors>(
            Accessors{.get = std::move(get), .set = std::move(set)})}) {}

  Binding(Getter get, TransactedSetter set)
      : source_(Transacted{
            std::make_shared<const TransactedAccessors>(TransactedAccessors{
                .get = std::move(get), .set = std::move(set)})}) {}

  template <class U>
    requires(!std::same_as<U, T>) && std::convertible_to<U, T> &&
            (std::convertible_to<T, U> || std::same_as<T, std::optional<U>>)
  explicit Binding(Binding<U> base)
      : Binding(Getter{[base] { return static_cast<T>(base.get()); }},
                Setter{[base](T value) {
                  if constexpr (std::same_as<T, std::optional<U>>) {
                    if (value.has_value()) {
                      base.set(std::move(*value));
                    }
                  } else {
                    base.set(static_cast<U>(std::move(value)));
                  }
                }}) {}

  static auto unwrapping(const Binding<std::optional<T>>& base)
      -> std::optional<Binding> {
    if (!base.get().has_value()) {
      return std::nullopt;
    }
    return Binding{Getter{[base, last = *base.get()] {
                     const std::optional<T> current = base.get();
                     return current.has_value() ? *current : last;
                   }},
                   Setter{[base](T value) {
                     base.set(std::optional<T>{std::move(value)});
                   }}};
  }

  auto size() const -> std::size_t
    requires std::ranges::sized_range<T>
  {
    return std::ranges::size(get());
  }

  auto indices() const
    requires std::ranges::sized_range<T>
  {
    return std::views::iota(std::size_t{0}, size());
  }

  template <std::ranges::random_access_range Whole = T>
  auto operator[](std::size_t index) const
      -> Binding<std::ranges::range_value_t<Whole>> {
    using Item = std::ranges::range_value_t<Whole>;
    return Binding<Item>{[parent = *this, index] {
                           const T whole = parent.get();
                           return index < std::ranges::size(whole)
                                      ? Item{std::ranges::begin(whole)[index]}
                                      : Item{};
                         },
                         [parent = *this, index](Item value) {
                           parent.update([&value, index](T& whole) {
                             if (index < std::ranges::size(whole)) {
                               std::ranges::begin(whole)[index] =
                                   std::move(value);
                             }
                           });
                         }};
  }

  static auto constant(T value) -> Binding {
    return Binding{Constant{std::move(value)}};
  }

  template <class Model>
  static auto projecting(Observable<Model>& model, T Model::* member)
      -> Binding {
    static_assert(sizeof(member) == sizeof(Member));
    return Binding{Field{.model = &model,
                         .member = std::bit_cast<Member>(member),
                         .read = &read_field<Model>,
                         .write = &write_field<Model>}};
  }

  auto get() const -> T {
    return std::visit(
        [](const auto& source) -> T { return Binding::read(source); }, source_);
  }

  void set(T value) const {
    const auto written = [this, &value] {
      std::visit(
          [&value](const auto& source) {
            Binding::write(source, std::move(value));
          },
          source_);
    };
    if (transaction_ != nullptr) {
      motion::with_transaction(*transaction_, written);
      return;
    }
    written();
  }

  auto transaction() const -> motion::Transaction {
    return transaction_ != nullptr ? *transaction_ : motion::Transaction{};
  }

  auto transaction(motion::Transaction transaction) const -> Binding {
    Binding carried = *this;
    carried.transaction_ =
        std::make_shared<const motion::Transaction>(std::move(transaction));
    return carried;
  }

  auto animation(std::optional<motion::Animation> animation =
                     motion::Animation::default_animation()) const -> Binding {
    return transaction(motion::Transaction{std::move(animation)});
  }

  auto identity() const noexcept -> Identity {
    return std::visit(
        [](const auto& source) { return Binding::locate(source); }, source_);
  }

  template <std::invocable<T&> Mutation>
  void update(Mutation&& mutation) const {
    T value = get();
    std::invoke(std::forward<Mutation>(mutation), value);
    set(std::move(value));
  }

  template <class Part, class Whole = T>
    requires std::is_class_v<Whole>
  auto projected(Part Whole::* member) const -> Binding<Part> {
    return Binding<Part>{
        [parent = *this, member] { return parent.get().*member; },
        [parent = *this, member](Part value) {
          parent.update([&value, member](Whole& whole) {
            whole.*member = std::move(value);
          });
        }};
  }

 private:
  using Member = std::array<std::byte, sizeof(std::ptrdiff_t)>;

  struct Accessors {
    Getter get;
    Setter set;
  };

  struct Computed {
    std::shared_ptr<const Accessors> accessors;
  };

  struct TransactedAccessors {
    Getter get;
    TransactedSetter set;
  };

  struct Transacted {
    std::shared_ptr<const TransactedAccessors> accessors;
  };

  struct Constant {
    T value;
  };

  struct Field {
    void* model;
    Member member;
    auto (*read)(const void* model, const Member& member) -> const T&;
    void (*write)(void* model, const Member& member, T value);
  };

  explicit Binding(Constant constant) : source_(std::move(constant)) {}

  explicit Binding(Field field) noexcept : source_(field) {}

  template <class Model>
  static auto read_field(const void* model, const Member& member) -> const T& {
    return static_cast<const Observable<Model>*>(model)->get().*
           std::bit_cast<T Model::*>(member);
  }

  template <class Model>
  static void write_field(void* model, const Member& member, T value) {
    static_cast<Observable<Model>*>(model)->write(
        std::bit_cast<T Model::*>(member), std::move(value));
  }

  static auto locate(const StateHandle<T>& state) noexcept -> Identity {
    return state.identity();
  }

  static auto locate(const Computed& computed) noexcept -> Identity {
    return Identity{computed.accessors.get()};
  }

  static auto locate(const Transacted& transacted) noexcept -> Identity {
    return Identity{transacted.accessors.get()};
  }

  static auto locate(const Constant&) noexcept -> Identity {
    return Identity{nullptr};
  }

  static auto locate(const Field& field) noexcept -> Identity {
    return Identity{&field.read(field.model, field.member)};
  }

  static auto read(const StateHandle<T>& state) -> T { return state.get(); }

  static auto read(const Computed& computed) -> T {
    return computed.accessors->get();
  }

  static auto read(const Transacted& transacted) -> T {
    return transacted.accessors->get();
  }

  static auto read(const Constant& constant) -> T { return constant.value; }

  static auto read(const Field& field) -> T {
    return field.read(field.model, field.member);
  }

  static void write(const StateHandle<T>& state, T value) {
    state.set(std::move(value));
  }

  static void write(const Computed& computed, T value) {
    computed.accessors->set(std::move(value));
  }

  static void write(const Transacted& transacted, T value) {
    const auto current = motion::TransactionStack::current();
    transacted.accessors->set(std::move(value), current.has_value()
                                                    ? current->get()
                                                    : motion::Transaction{});
  }

  static void write(const Constant&, T) {}

  static void write(const Field& field, T value) {
    field.write(field.model, field.member, std::move(value));
  }

  std::variant<StateHandle<T>, Computed, Transacted, Constant, Field> source_;
  std::shared_ptr<const motion::Transaction> transaction_;
};

template <class Source>
struct BindingTraits {};

template <class T>
struct BindingTraits<StateHandle<T>> {
  using Value = T;
};

template <class T>
struct BindingTraits<Binding<T>> {
  using Value = T;
};

template <class Source>
concept BindingSource =
    requires { typename BindingTraits<std::remove_cvref_t<Source>>::Value; };

template <BindingSource Source>
using BoundValue = typename BindingTraits<std::remove_cvref_t<Source>>::Value;

}
