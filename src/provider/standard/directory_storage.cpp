#include <arisa/provider/standard/directory_storage.hpp>

#include <arisa/provider/standard/file_stat.hpp>
#include <arisa/support/unique_handle.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

namespace arisa::provider::standard {

DirectoryStorage::DirectoryStorage(std::filesystem::path root)
    : root_(std::move(root)) {}

auto DirectoryStorage::home() -> std::filesystem::path {
  for (const char* name : {"HOME", "USERPROFILE"}) {
    if (const char* value = std::getenv(name);
        value != nullptr && *value != '\0') {
      return std::filesystem::path{value};
    }
  }
  return std::filesystem::path{"."};
}

auto DirectoryStorage::application_config(std::string_view application)
    -> DirectoryStorage {
  const std::filesystem::path name{application};
  if (const char* appdata = std::getenv("APPDATA");
      appdata != nullptr && *appdata != '\0') {
    return DirectoryStorage{std::filesystem::path{appdata} / name};
  }
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME");
      xdg != nullptr && *xdg != '\0') {
    return DirectoryStorage{std::filesystem::path{xdg} / name};
  }
  const std::filesystem::path base = home();
  std::error_code error;
  const std::filesystem::path support =
      base / "Library" / "Application Support";
  if (std::filesystem::is_directory(support, error)) {
    return DirectoryStorage{support / name};
  }
  return DirectoryStorage{base / ".config" / name};
}

auto DirectoryStorage::read(const AssetPath& path) const
    -> std::expected<AssetBlob, ProviderError> {
  const std::filesystem::path location = resolve(path);
  std::error_code error;
  if (!std::filesystem::is_regular_file(location, error)) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  const std::uintmax_t size = std::filesystem::file_size(location, error);
  if (error) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::io_failure,
                      static_cast<std::uint32_t>(error.value())});
  }
  const support::UniqueHandle<FileRelease> file{
      std::fopen(location.c_str(), "rb")};
  if (!file) {
    return std::unexpected(ProviderError{ProviderErrorCode::io_failure,
                                         static_cast<std::uint32_t>(errno)});
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size()) {
    return std::unexpected(ProviderError{ProviderErrorCode::io_failure});
  }
  return AssetBlob{std::move(bytes)};
}

auto DirectoryStorage::write(const AssetPath& path,
                             std::span<const std::byte> bytes)
    -> std::expected<void, ProviderError> {
  const std::filesystem::path location = resolve(path);
  if (auto made = ensure(location.parent_path()); !made.has_value()) {
    return made;
  }
  const support::UniqueHandle<FileRelease> file{
      std::fopen(location.c_str(), "wb")};
  if (!file) {
    return std::unexpected(failure(errno));
  }
  if (std::fwrite(bytes.data(), 1, bytes.size(), file.get()) != bytes.size() ||
      std::fflush(file.get()) != 0) {
    return std::unexpected(failure(errno));
  }
  return {};
}

auto DirectoryStorage::create_directories(const AssetPath& directory)
    -> std::expected<void, ProviderError> {
  return ensure(resolve(directory));
}

auto DirectoryStorage::ensure(const std::filesystem::path& directory)
    -> std::expected<void, ProviderError> {
  std::vector<std::filesystem::path> missing;
  std::error_code error;
  for (std::filesystem::path at = directory;
       !at.empty() && !std::filesystem::is_directory(at, error);
       at = at.parent_path()) {
    missing.push_back(at);
    if (at.parent_path() == at) {
      break;
    }
  }
  for (const std::filesystem::path& each : std::views::reverse(missing)) {
    std::filesystem::create_directory(each, error);
    if (error && error != std::errc::file_exists &&
        each.parent_path().has_relative_path()) {
      return std::unexpected(failure(error.value()));
    }
  }
  return {};
}

auto DirectoryStorage::failure(int error) noexcept -> ProviderError {
  switch (static_cast<std::errc>(error)) {
    case std::errc::no_such_file_or_directory:
      return ProviderError{ProviderErrorCode::not_found,
                           static_cast<std::uint32_t>(error)};
    case std::errc::no_space_on_device:
      return ProviderError{ProviderErrorCode::resource_exhausted,
                           static_cast<std::uint32_t>(error)};
    default:
      return ProviderError{ProviderErrorCode::io_failure,
                           static_cast<std::uint32_t>(error)};
  }
}

auto DirectoryStorage::location(const AssetPath& path) const -> std::string {
  return resolve(path).string();
}

auto DirectoryStorage::list(const AssetPath& directory) const
    -> std::expected<std::vector<std::string>, ProviderError> {
  const std::filesystem::path location = resolve(directory);
  std::error_code error;
  if (!std::filesystem::is_directory(location, error)) {
    return std::vector<std::string>{};
  }
  std::vector<std::string> names;
  for (std::filesystem::directory_iterator each{location, error};
       !error && each != std::filesystem::directory_iterator{};
       each.increment(error)) {
    if (each->is_regular_file(error)) {
      names.push_back(each->path().filename().string());
    }
  }
  if (error) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::io_failure,
                      static_cast<std::uint32_t>(error.value())});
  }
  std::ranges::sort(names);
  return names;
}

auto DirectoryStorage::entries(const std::optional<AssetPath>& directory) const
    -> std::expected<std::vector<DirectoryEntry>, ProviderError> {
  const std::filesystem::path location =
      directory.has_value() ? resolve(*directory) : root_;
  std::error_code error;
  if (!std::filesystem::is_directory(location, error)) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  std::vector<DirectoryEntry> found;
  for (std::filesystem::directory_iterator each{location, error};
       !error && each != std::filesystem::directory_iterator{};
       each.increment(error)) {
    std::error_code kind;
    const bool folder = each->is_directory(kind);
    std::string name = each->path().filename().string();
    if (name.empty() || name.front() == '.') {
      continue;
    }
    std::error_code sized;
    const std::uintmax_t size = folder ? 0U : each->file_size(sized);
    found.push_back(
        DirectoryEntry{.name = std::move(name),
                       .kind = folder ? DirectoryEntry::Kind::directory
                                      : DirectoryEntry::Kind::file,
                       .size = sized ? 0U : static_cast<std::uint64_t>(size)});
  }
  if (error) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::io_failure,
                      static_cast<std::uint32_t>(error.value())});
  }
  return found;
}

auto DirectoryStorage::remove(const AssetPath& path)
    -> std::expected<void, ProviderError> {
  std::error_code error;
  if (!std::filesystem::remove(resolve(path), error)) {
    return std::unexpected(
        error ? ProviderError{ProviderErrorCode::io_failure,
                              static_cast<std::uint32_t>(error.value())}
              : ProviderError{ProviderErrorCode::not_found});
  }
  return {};
}

auto DirectoryStorage::resolve(const AssetPath& path) const
    -> std::filesystem::path {
  return root_ / std::filesystem::path{path.value()};
}

void DirectoryStorage::FileRelease::release(handle_type& file) noexcept {
  std::fclose(file);
}

auto DirectoryStorage::stamp(const AssetPath& path) const
    -> std::optional<FileStamp> {
  return FileStat::stamp(resolve(path));
}

}
