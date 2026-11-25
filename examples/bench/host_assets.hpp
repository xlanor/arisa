#pragma once

#include <arisa/provider/headless/headless_factory.hpp>

namespace arisa::examples {

class HostAssets {
 public:
  static void load(provider::headless::HeadlessProviderFactory& providers);
};

}
