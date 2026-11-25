#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/observable.hpp>

#include <functional>
#include <optional>

namespace arisa::composition {

template <class Model>
class Bindable {
 public:
  Bindable(Context& context, Observable<Model>& model) noexcept
      : context_(context), model_(model) {}

  explicit Bindable(Observable<Model>& model) noexcept : model_(model) {}

  template <class Field>
  auto operator[](Field Model::* member) const -> Binding<Field> {
    if (context_.has_value()) {
      static_cast<void>(context_->get().read(model_.get(), member));
    }
    return Binding<Field>::projecting(model_.get(), member);
  }

 private:
  std::optional<std::reference_wrapper<Context>> context_;
  std::reference_wrapper<Observable<Model>> model_;
};

}
