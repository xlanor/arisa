#pragma once

#include <arisa/provider/assets/file_stamp.hpp>

#include <filesystem>
#include <optional>

namespace arisa::provider::standard {

class FileStat {
 public:
  static auto stamp(const std::filesystem::path& location)
      -> std::optional<FileStamp>;
};

}
