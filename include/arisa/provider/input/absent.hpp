#pragma once

#include <arisa/provider/input/provider.hpp>

namespace arisa::provider {

class AbsentInputProvider final : public InputProvider {
 public:
  auto sample() -> InputFrame override;
  auto tuning() const noexcept -> InputTuning override;
  void set_tuning(InputTuning tuning) noexcept override;
  auto pointer_host() const noexcept -> PointerHost override;
  void present_pointer(PointerAppearance appearance) override;

 private:
  InputTuning tuning_;
};

}
