#include <arisa/provider/headless/memory_storage.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace arisa::provider::headless {

void MemoryStorage::add(AssetPath path, std::string_view text) {
  const std::scoped_lock lock{mutex_};
  const std::span<const std::byte> bytes = std::as_bytes(std::span{text});
  revisions_.insert_or_assign(path, ++revision_);
  files_.insert_or_assign(std::move(path),
                          std::vector<std::byte>(bytes.begin(), bytes.end()));
}

auto MemoryStorage::read(const AssetPath& path) const
    -> std::expected<AssetBlob, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const auto found = files_.find(path);
  if (found == files_.end()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return AssetBlob{found->second};
}

auto MemoryStorage::write(const AssetPath& path,
                          std::span<const std::byte> bytes)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  files_.insert_or_assign(path,
                          std::vector<std::byte>(bytes.begin(), bytes.end()));
  revisions_.insert_or_assign(path, ++revision_);
  ++writes_;
  return {};
}

auto MemoryStorage::list(const AssetPath& directory) const
    -> std::expected<std::vector<std::string>, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const std::string prefix = std::string{directory.value()} + "/";
  std::vector<std::string> names;
  for (const auto& [path, bytes] : files_) {
    const std::string_view value = path.value();
    if (value.starts_with(prefix) &&
        value.find('/', prefix.size()) == std::string_view::npos) {
      names.emplace_back(value.substr(prefix.size()));
    }
  }
  return names;
}

auto MemoryStorage::entries(const std::optional<AssetPath>& directory) const
    -> std::expected<std::vector<DirectoryEntry>, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const std::string prefix = directory.has_value()
                                 ? std::string{directory->value()} + "/"
                                 : std::string{};
  std::vector<DirectoryEntry> found;
  const auto add = [&found](std::string_view name, DirectoryEntry::Kind kind,
                            std::uint64_t size) {
    const bool seen = std::ranges::any_of(
        found, [name](const auto& entry) { return entry.name == name; });
    if (!seen) {
      found.push_back(DirectoryEntry{
          .name = std::string{name}, .kind = kind, .size = size});
    }
  };
  for (const auto& [path, bytes] : files_) {
    const std::string_view value = path.value();
    if (!value.starts_with(prefix)) {
      continue;
    }
    const std::string_view rest = value.substr(prefix.size());
    const std::size_t slash = rest.find('/');
    if (slash == std::string_view::npos) {
      add(rest, DirectoryEntry::Kind::file, bytes.size());
    } else {
      add(rest.substr(0, slash), DirectoryEntry::Kind::directory, 0);
    }
  }
  for (const std::string& folder : directories_) {
    const std::string_view value = folder;
    if (value.starts_with(prefix) && value.size() > prefix.size() &&
        value.find('/', prefix.size()) == std::string_view::npos) {
      add(value.substr(prefix.size()), DirectoryEntry::Kind::directory, 0);
    }
  }
  return found;
}

auto MemoryStorage::remove(const AssetPath& path)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  if (files_.erase(path) == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  revisions_.erase(path);
  return {};
}

auto MemoryStorage::create_directories(const AssetPath& directory)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const std::string_view value = directory.value();
  for (std::size_t at = value.find('/'); at != std::string_view::npos;
       at = value.find('/', at + 1)) {
    directories_.emplace(value.substr(0, at));
  }
  directories_.emplace(value);
  return {};
}

auto MemoryStorage::has_directory(const AssetPath& directory) const -> bool {
  const std::scoped_lock lock{mutex_};
  return directories_.contains(directory.value());
}

auto MemoryStorage::text(const AssetPath& path) const -> std::string {
  const std::scoped_lock lock{mutex_};
  const auto found = files_.find(path);
  if (found == files_.end()) {
    return {};
  }
  std::string text(found->second.size(), '\0');
  std::ranges::transform(found->second, text.begin(), [](std::byte value) {
    return static_cast<char>(value);
  });
  return text;
}

auto MemoryStorage::stamp(const AssetPath& path) const
    -> std::optional<FileStamp> {
  const std::scoped_lock lock{mutex_};
  ++stamps_;
  const auto found = revisions_.find(path);
  if (found == revisions_.end()) {
    return std::nullopt;
  }
  return FileStamp{found->second};
}

auto MemoryStorage::stamps() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  return stamps_;
}

}
