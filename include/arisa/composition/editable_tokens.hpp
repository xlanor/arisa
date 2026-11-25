#pragma once

#include <arisa/composition/binding.hpp>

#include <utility>
#include <vector>

namespace arisa::composition {

template <class T>
class EditableTokens {
 public:
  explicit EditableTokens(Binding<std::vector<T>> tokens) noexcept
      : tokens_(std::move(tokens)) {}

  template <BindingSource Source>
    requires std::same_as<BoundValue<Source>, std::vector<T>>
  explicit EditableTokens(Source tokens)
      : tokens_(Binding<std::vector<T>>{std::move(tokens)}) {}

  auto binding() const noexcept -> const Binding<std::vector<T>>& {
    return tokens_;
  }

 private:
  Binding<std::vector<T>> tokens_;
};

template <BindingSource Source>
EditableTokens(Source)
    -> EditableTokens<typename BoundValue<Source>::value_type>;

}
