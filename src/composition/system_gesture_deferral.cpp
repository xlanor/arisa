#include <arisa/composition/system_gesture_deferral.hpp>

namespace arisa::composition {

const PreferenceKey<std::uint8_t> SystemGestureDeferral::key{
    "system-gesture-deferral", 0U, [](std::uint8_t& value, std::uint8_t next) {
      value = static_cast<std::uint8_t>(value | next);
    }};

}
