#pragma once

#include <functional>
#include <string_view>

namespace arisa::conformance {

using ConformanceTrace = std::function<void(std::string_view)>;

struct ConformanceOptions {
  bool open_external{false};
  ConformanceTrace trace{};
};

}
