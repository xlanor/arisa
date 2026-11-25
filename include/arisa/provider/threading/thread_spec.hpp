#pragma once

#include <arisa/provider/threading/thread_role.hpp>

#include <string>

namespace arisa::provider {

struct ThreadSpec {
  std::string name;
  ThreadRole role{ThreadRole::background};
};

}
