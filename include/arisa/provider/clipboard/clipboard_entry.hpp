#pragma once

#include <string>

namespace arisa::provider {

struct ClipboardEntry {
  std::string mime_type;
  std::string data;

  friend auto operator==(const ClipboardEntry&, const ClipboardEntry&)
      -> bool = default;
};

}
