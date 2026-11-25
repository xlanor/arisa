#include <arisa/provider/standard/directory_assets.hpp>

#include <arisa/provider/standard/file_stat.hpp>
#include <arisa/support/unique_handle.hpp>

#include <cerrno>
#include <system_error>
#include <utility>
#include <vector>

namespace arisa::provider::standard {

DirectoryAssets::DirectoryAssets(std::filesystem::path root)
    : root_(std::move(root)) {}

auto DirectoryAssets::current_directory() -> DirectoryAssets {
  std::error_code error;
  std::filesystem::path working = std::filesystem::current_path(error);
  return DirectoryAssets{error ? std::filesystem::path{"."}
                               : std::move(working)};
}

auto DirectoryAssets::read(const AssetPath& path) const
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
    return std::unexpected(ProviderError{errno == ENOENT
                                             ? ProviderErrorCode::not_found
                                             : ProviderErrorCode::io_failure,
                                         static_cast<std::uint32_t>(errno)});
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  const std::size_t read =
      std::fread(bytes.data(), 1, bytes.size(), file.get());
  if (read != bytes.size()) {
    return std::unexpected(ProviderError{ProviderErrorCode::io_failure});
  }
  return AssetBlob{std::move(bytes)};
}

auto DirectoryAssets::exists(const AssetPath& path) const -> bool {
  std::error_code error;
  return std::filesystem::is_regular_file(resolve(path), error);
}

auto DirectoryAssets::resolve(const AssetPath& path) const
    -> std::filesystem::path {
  return root_ / std::filesystem::path{path.value()};
}

void DirectoryAssets::FileRelease::release(handle_type& file) noexcept {
  std::fclose(file);
}

auto DirectoryAssets::stamp(const AssetPath& path) const
    -> std::optional<FileStamp> {
  return FileStat::stamp(resolve(path));
}

}
