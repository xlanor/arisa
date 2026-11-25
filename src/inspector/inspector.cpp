#include <arisa/inspector/inspector.hpp>

#include <utility>

namespace arisa::inspector {

Inspector::Inspector(std::shared_ptr<InspectorModel> model,
                     support::ScopedCallback observing) noexcept
    : model_(std::move(model)), observing_(std::move(observing)) {}

auto Inspector::attach(runtime::Application& application,
                       InspectorOptions options) -> Inspector {
  auto model = std::make_shared<InspectorModel>(application, options);
  support::ScopedCallback observing = application.observe(*model);
  return Inspector{std::move(model), std::move(observing)};
}

}
