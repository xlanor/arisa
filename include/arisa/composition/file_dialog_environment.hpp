#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/file_url.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace arisa::composition {

class FileDialogEnvironment {
 public:
  using Enabled = std::shared_ptr<const std::function<bool(const FileURL&)>>;

  static const EnvironmentKey<std::optional<std::string>> confirmation_label;
  static const EnvironmentKey<std::optional<std::string>> customization_id;
  static const EnvironmentKey<std::optional<FileURL>> default_directory;
  static const EnvironmentKey<std::optional<std::string>> message;
  static const EnvironmentKey<Enabled> url_enabled;
};

}
