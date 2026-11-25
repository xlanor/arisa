#pragma once

#include <arisa/provider/capability.hpp>

#include <cstdint>
#include <string>

namespace arisa::provider {

inline constexpr std::uint32_t abi_version = 4;

struct ProviderInfo {
  std::string name;
  std::uint32_t abi_version{provider::abi_version};
  Capability capabilities{Capability::none};
};

}
