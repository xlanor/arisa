#include <arisa/provider/lifecycle/home_button_block.hpp>

#include <utility>

namespace arisa::provider {

HomeButtonBlock::HomeButtonBlock(support::ScopedCallback release) noexcept
    : release_(std::move(release)) {}

auto HomeButtonBlock::active() const noexcept -> bool {
  return release_.valid();
}

}
