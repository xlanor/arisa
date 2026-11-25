#include <arisa/provider/standard/file_stat.hpp>

#include <chrono>
#include <cstdint>
#include <system_error>

namespace arisa::provider::standard {

auto FileStat::stamp(const std::filesystem::path& location)
    -> std::optional<FileStamp> {
  constexpr std::uint64_t prime = 1'099'511'628'211ULL;
  std::error_code error;
  if (!std::filesystem::is_regular_file(location, error)) {
    return std::nullopt;
  }
  const std::filesystem::file_time_type written =
      std::filesystem::last_write_time(location, error);
  if (error) {
    return std::nullopt;
  }
  const std::uintmax_t size = std::filesystem::file_size(location, error);
  if (error) {
    return std::nullopt;
  }
  const auto ticks = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          written.time_since_epoch())
          .count());
  return FileStamp{(ticks * prime) ^ static_cast<std::uint64_t>(size)};
}

}
