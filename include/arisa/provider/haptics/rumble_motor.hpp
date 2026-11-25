#pragma once

#include <arisa/provider/haptics/rumble_pulse.hpp>

namespace arisa::provider {

class RumbleMotor {
 public:
  virtual ~RumbleMotor() = default;

  virtual void prepare() = 0;

  virtual void vibrate(const RumblePulse& pulse) = 0;
};

}
