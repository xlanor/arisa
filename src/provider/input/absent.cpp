#include <arisa/provider/input/absent.hpp>

namespace arisa::provider {

auto AbsentInputProvider::sample() -> InputFrame { return {}; }

auto AbsentInputProvider::tuning() const noexcept -> InputTuning {
  return tuning_;
}

void AbsentInputProvider::set_tuning(InputTuning tuning) noexcept {
  tuning_ = tuning;
}

auto AbsentInputProvider::pointer_host() const noexcept -> PointerHost {
  return PointerHost::system;
}

void AbsentInputProvider::present_pointer(PointerAppearance) {}

}
