#include <arisa/composition/persistent_system_overlays.hpp>

namespace arisa::composition {

const PreferenceKey<Visibility> PersistentSystemOverlays::key{
    "persistent-system-overlays", Visibility::automatic,
    &PersistentSystemOverlays::reduce};

void PersistentSystemOverlays::reduce(Visibility& value,
                                      Visibility next) noexcept {
  if (value == Visibility::hidden || next == Visibility::automatic) {
    return;
  }
  value = next;
}

}
