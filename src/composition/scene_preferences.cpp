#include <arisa/composition/persistent_system_overlays.hpp>
#include <arisa/composition/scene_preferences.hpp>
#include <arisa/composition/system_gesture_deferral.hpp>

namespace arisa::composition {

auto ScenePreferences::crosses(Identity preference) noexcept -> bool {
  return preference == PersistentSystemOverlays::key.identity() ||
         preference == SystemGestureDeferral::key.identity();
}

}
