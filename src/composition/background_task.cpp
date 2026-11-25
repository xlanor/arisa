#include <arisa/composition/background_task.hpp>

namespace arisa::composition {

auto BackgroundTask::app_refresh(std::string identifier) -> BackgroundTask {
  return BackgroundTask{Kind::app_refresh, std::move(identifier), nullptr};
}

auto BackgroundTask::processing_task(std::string identifier) -> BackgroundTask {
  return BackgroundTask{Kind::processing_task, std::move(identifier), nullptr};
}

auto BackgroundTask::matches(const Request& request) const -> bool {
  if (request.kind != kind_) {
    return false;
  }
  if (matching_ != nullptr) {
    return *matching_ && (*matching_)(request.identifier);
  }
  return !identifier_.has_value() || *identifier_ == request.identifier;
}

}
