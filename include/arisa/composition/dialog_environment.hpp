#pragma once

#include <arisa/composition/dialog_severity.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/presentation_options.hpp>

#include <memory>
#include <optional>

namespace arisa::composition {

class DialogEnvironment {
 public:
  using Icon = std::shared_ptr<Element>;

  static const EnvironmentKey<Icon> icon;
  static const EnvironmentKey<DialogSeverity> severity;
  static const EnvironmentKey<std::optional<PresentationOptions::Suppression>>
      suppression;
};

}
