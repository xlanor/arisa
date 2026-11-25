#pragma once

#include <cstdint>
#include <string>

namespace arisa::provider {

struct DirectoryEntry {
  enum class Kind : std::uint8_t {
    file,
    directory,
  };

  std::string name;
  Kind kind{Kind::file};
  std::uint64_t size{0};

  friend auto operator==(const DirectoryEntry&, const DirectoryEntry&)
      -> bool = default;
};

}
