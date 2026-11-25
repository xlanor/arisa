#pragma once

#include <arisa/composition/user_defaults.hpp>
#include <arisa/composition/ut_type.hpp>

#include <concepts>
#include <string>

namespace arisa::composition {

template <class T>
concept Transferable =
    std::same_as<T, std::string> || (Encodable<T> && requires {
      requires std::constructible_from<UTType, decltype(T::content_type())>;
    });

}
