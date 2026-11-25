#pragma once

#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/visibility.hpp>

namespace arisa::composition {

class PersistentSystemOverlays {
 public:
  static const PreferenceKey<Visibility> key;

  static void reduce(Visibility& value, Visibility next) noexcept;
};

}
