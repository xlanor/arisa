#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element_key.hpp>

#include <concepts>
#include <optional>

namespace arisa::composition {

template <class Source>
concept OptionalBindingSource =
    BindingSource<Source> &&
    requires { typename BoundValue<Source>::value_type; } &&
    std::same_as<BoundValue<Source>,
                 std::optional<typename BoundValue<Source>::value_type>>;

template <class Source>
concept PresentedItem = OptionalBindingSource<Source> &&
                        Identifiable<typename BoundValue<Source>::value_type>;

template <OptionalBindingSource Source>
using PresentedValue = typename BoundValue<Source>::value_type;

}
