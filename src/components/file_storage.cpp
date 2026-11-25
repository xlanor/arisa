#include <arisa/components/file_storage.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace arisa::components {

auto FileStorage::path(const composition::FileURL& file,
                       composition::FileError::Stage stage)
    -> std::expected<provider::AssetPath, composition::FileError> {
  auto made = provider::AssetPath::create(file.path());
  if (!made.has_value()) {
    return std::unexpected(composition::FileError{stage, made.error()});
  }
  return std::move(*made);
}

auto FileStorage::entries(const std::optional<composition::FileURL>& directory)
    const -> std::expected<std::vector<provider::DirectoryEntry>,
                           composition::FileError> {
  using Stage = composition::FileError::Stage;
  std::optional<provider::AssetPath> folder;
  if (directory.has_value()) {
    auto made = path(*directory, Stage::list);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    folder = std::move(*made);
  }
  auto listed = storage_.get().entries(folder);
  if (!listed.has_value()) {
    return std::unexpected(composition::FileError{Stage::list, listed.error()});
  }
  const auto lowered = [](const std::string& name) {
    std::string key = name;
    std::ranges::transform(key, key.begin(), [](char character) {
      return static_cast<char>(
          std::tolower(static_cast<unsigned char>(character)));
    });
    return key;
  };
  std::ranges::sort(*listed, [&lowered](const provider::DirectoryEntry& left,
                                        const provider::DirectoryEntry& right) {
    if (left.kind != right.kind) {
      return left.kind == provider::DirectoryEntry::Kind::directory;
    }
    return lowered(left.name) < lowered(right.name);
  });
  return std::move(*listed);
}

auto FileStorage::read(const composition::FileURL& file) const
    -> std::expected<std::vector<std::byte>, composition::FileError> {
  using Stage = composition::FileError::Stage;
  auto located = path(file, Stage::read);
  if (!located.has_value()) {
    return std::unexpected(located.error());
  }
  auto blob = storage_.get().read(*located);
  if (!blob.has_value()) {
    return std::unexpected(composition::FileError{Stage::read, blob.error()});
  }
  return std::vector<std::byte>{blob->bytes().begin(), blob->bytes().end()};
}

auto FileStorage::create_directory(const composition::FileURL& directory) const
    -> std::expected<void, composition::FileError> {
  using Stage = composition::FileError::Stage;
  auto located = path(directory, Stage::write);
  if (!located.has_value()) {
    return std::unexpected(located.error());
  }
  if (auto made = storage_.get().create_directories(*located);
      !made.has_value()) {
    return std::unexpected(composition::FileError{Stage::write, made.error()});
  }
  return {};
}

auto FileStorage::exists(const composition::FileURL& file) const -> bool {
  const auto located = provider::AssetPath::create(file.path());
  return located.has_value() && storage_.get().read(*located).has_value();
}

auto FileStorage::stamp(const composition::FileURL& file) const
    -> std::optional<provider::FileStamp> {
  const auto located = provider::AssetPath::create(file.path());
  if (!located.has_value()) {
    return std::nullopt;
  }
  return storage_.get().stamp(*located);
}

auto FileStorage::write(const composition::FileURL& file,
                        std::span<const std::byte> bytes) const
    -> std::expected<void, composition::FileError> {
  using Stage = composition::FileError::Stage;
  auto located = path(file, Stage::write);
  if (!located.has_value()) {
    return std::unexpected(located.error());
  }
  if (auto written = storage_.get().write(*located, bytes);
      !written.has_value()) {
    return std::unexpected(
        composition::FileError{Stage::write, written.error()});
  }
  return {};
}

auto FileStorage::move(const composition::FileURL& file,
                       const composition::FileURL& destination) const
    -> std::expected<void, composition::FileError> {
  using Stage = composition::FileError::Stage;
  auto bytes = read(file);
  if (!bytes.has_value()) {
    return std::unexpected(bytes.error());
  }
  if (auto written = write(destination, *bytes); !written.has_value()) {
    return std::unexpected(written.error());
  }
  auto located = path(file, Stage::remove);
  if (!located.has_value()) {
    return std::unexpected(located.error());
  }
  if (auto removed = storage_.get().remove(*located); !removed.has_value()) {
    return std::unexpected(
        composition::FileError{Stage::remove, removed.error()});
  }
  return {};
}

}
