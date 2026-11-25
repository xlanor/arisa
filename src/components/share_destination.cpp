#include <arisa/components/share_destination.hpp>

#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<std::vector<ShareDestination>>
    ShareDestination::key{"share_destinations", {}};

ShareDestination::ShareDestination(std::string title, Image::SystemName symbol,
                                   Perform perform)
    : title_(std::move(title)),
      symbol_(std::move(symbol)),
      perform_(std::make_shared<const Perform>(std::move(perform))) {}

void ShareDestination::share(const Request& request) const {
  if (perform_ != nullptr && *perform_) {
    (*perform_)(request);
  }
}

}
