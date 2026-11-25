#pragma once

#include <string_view>

namespace arisa::provider {

struct CrashReporterOptions {
  std::string_view directory{};
  std::string_view app{};
};

}
