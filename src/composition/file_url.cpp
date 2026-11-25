#include <arisa/composition/file_url.hpp>

#include <format>

namespace arisa::composition {

auto FileURL::create(std::string_view path) -> std::optional<FileURL> {
  while (path.starts_with('/')) {
    path.remove_prefix(1);
  }
  while (path.ends_with('/')) {
    path.remove_suffix(1);
  }
  if (path.empty() || path.find('\0') != std::string_view::npos ||
      path.find('\\') != std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = path.find('/', start);
    const std::string_view part = path.substr(
        start,
        end == std::string_view::npos ? std::string_view::npos : end - start);
    if (part.empty() || part == "." || part == "..") {
      return std::nullopt;
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return FileURL{std::string{path}};
}

auto FileURL::last_path_component() const noexcept -> std::string_view {
  const std::size_t slash = path_.rfind('/');
  return slash == std::string::npos ? std::string_view{path_}
                                    : std::string_view{path_}.substr(slash + 1);
}

auto FileURL::deleting_last_path_component() const -> std::optional<FileURL> {
  const std::size_t slash = path_.rfind('/');
  if (slash == std::string::npos) {
    return std::nullopt;
  }
  return FileURL{path_.substr(0, slash)};
}

auto FileURL::appending(std::string_view component) const
    -> std::optional<FileURL> {
  return create(std::format("{}/{}", path_, component));
}

auto FileError::stage_name(Stage stage) noexcept -> std::string_view {
  switch (stage) {
    case Stage::storage:
      return "storage";
    case Stage::list:
      return "list";
    case Stage::read:
      return "read";
    case Stage::write:
      return "write";
    case Stage::remove:
      return "remove";
    case Stage::name:
      return "name";
  }
  return "storage";
}

auto FileError::explanation() const -> std::string {
  return std::format("file {} failed: {}", stage_name(stage_),
                     cause_.explanation());
}

}
