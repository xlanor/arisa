#include <arisa/components/file_wrapper.hpp>

#include <arisa/components/file_storage.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace arisa::components {

FileWrapper::FileWrapper(std::shared_ptr<const std::vector<std::byte>> contents,
                         std::vector<FileWrapper> children,
                         bool directory) noexcept
    : contents_(std::move(contents)),
      children_(std::move(children)),
      directory_(directory) {}

auto FileWrapper::regular_file(std::vector<std::byte> contents) -> FileWrapper {
  return FileWrapper{
      std::make_shared<const std::vector<std::byte>>(std::move(contents)),
      {},
      false};
}

auto FileWrapper::regular_file(std::string_view text) -> FileWrapper {
  std::vector<std::byte> bytes(text.size());
  std::ranges::transform(text, bytes.begin(), [](char character) {
    return static_cast<std::byte>(character);
  });
  return regular_file(std::move(bytes));
}

auto FileWrapper::directory(std::vector<FileWrapper> children) -> FileWrapper {
  return FileWrapper{nullptr, std::move(children), true};
}

auto FileWrapper::regular_file_contents() const noexcept
    -> std::optional<std::span<const std::byte>> {
  if (directory_) {
    return std::nullopt;
  }
  if (contents_ == nullptr) {
    return std::span<const std::byte>{};
  }
  return std::span<const std::byte>{*contents_};
}

auto FileWrapper::text() const -> std::optional<std::string> {
  const auto contents = regular_file_contents();
  if (!contents.has_value()) {
    return std::nullopt;
  }
  std::string text(contents->size(), '\0');
  std::ranges::transform(*contents, text.begin(), [](std::byte value) {
    return static_cast<char>(value);
  });
  return text;
}

auto FileWrapper::name_or(std::string_view fallback) const -> std::string {
  if (preferred_filename_.has_value()) {
    return *preferred_filename_;
  }
  if (filename_.has_value()) {
    return *filename_;
  }
  return std::string{fallback};
}

auto FileWrapper::read(const FileStorage& storage,
                       const composition::FileURL& file)
    -> std::expected<FileWrapper, composition::FileError> {
  auto bytes = storage.read(file);
  if (bytes.has_value()) {
    FileWrapper made = regular_file(std::move(*bytes));
    made.filename_ = std::string{file.last_path_component()};
    return made;
  }
  auto listed = storage.entries(file);
  if (!listed.has_value()) {
    return std::unexpected(bytes.error());
  }
  std::vector<FileWrapper> children;
  for (const provider::DirectoryEntry& entry : *listed) {
    const auto child = file.appending(entry.name);
    if (!child.has_value()) {
      continue;
    }
    auto read_child = read(storage, *child);
    if (!read_child.has_value()) {
      return std::unexpected(read_child.error());
    }
    children.push_back(std::move(*read_child));
  }
  FileWrapper made = directory(std::move(children));
  made.filename_ = std::string{file.last_path_component()};
  return made;
}

auto FileWrapper::write(const FileStorage& storage,
                        const composition::FileURL& file) const
    -> std::expected<void, composition::FileError> {
  if (!directory_) {
    const auto contents = regular_file_contents();
    return storage.write(file, contents.value_or(std::span<const std::byte>{}));
  }
  if (auto made = storage.create_directory(file); !made.has_value()) {
    return made;
  }
  for (std::size_t index = 0; index < children_.size(); ++index) {
    const FileWrapper& child = children_[index];
    const auto target = file.appending(child.name_or(std::format("{}", index)));
    if (!target.has_value()) {
      return std::unexpected(composition::FileError{
          composition::FileError::Stage::name,
          provider::ProviderError{
              provider::ProviderErrorCode::invalid_argument}});
    }
    if (auto written = child.write(storage, *target); !written.has_value()) {
      return written;
    }
  }
  return {};
}

auto operator==(const FileWrapper& left, const FileWrapper& right) -> bool {
  if (left.directory_ != right.directory_ ||
      left.filename_ != right.filename_ ||
      left.preferred_filename_ != right.preferred_filename_) {
    return false;
  }
  if (left.directory_) {
    return left.children_ == right.children_;
  }
  return std::ranges::equal(
      left.regular_file_contents().value_or(std::span<const std::byte>{}),
      right.regular_file_contents().value_or(std::span<const std::byte>{}));
}

}
