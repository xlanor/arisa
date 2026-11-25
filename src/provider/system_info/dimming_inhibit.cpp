#include <arisa/provider/system_info/dimming_inhibit.hpp>

#include <utility>

namespace arisa::provider {

DimmingInhibit::DimmingInhibit(support::ScopedCallback release) noexcept
    : release_(std::move(release)) {}

auto DimmingInhibit::active() const noexcept -> bool {
  return release_.valid();
}

}
