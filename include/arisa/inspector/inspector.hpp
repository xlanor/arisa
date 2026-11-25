#pragma once

#include <arisa/inspector/inspector_model.hpp>
#include <arisa/inspector/inspector_options.hpp>
#include <arisa/inspector/inspector_panel.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <memory>

namespace arisa::inspector {

class Inspector {
 public:
  static auto attach(runtime::Application& application,
                     InspectorOptions options = {}) -> Inspector;

  Inspector(Inspector&&) noexcept = default;
  auto operator=(Inspector&&) noexcept -> Inspector& = default;
  Inspector(const Inspector&) = delete;
  auto operator=(const Inspector&) -> Inspector& = delete;
  ~Inspector() = default;

  void open() { model_->open(); }

  void close() { model_->close(); }

  void toggle() { model_->toggle(); }

  auto is_open() const noexcept -> bool { return model_->is_open(); }

  void show(InspectorPanel panel) { model_->show(panel); }

  auto model() noexcept -> InspectorModel& { return *model_; }

  auto model() const noexcept -> const InspectorModel& { return *model_; }

 private:
  Inspector(std::shared_ptr<InspectorModel> model,
            support::ScopedCallback observing) noexcept;

  std::shared_ptr<InspectorModel> model_;
  support::ScopedCallback observing_;
};

}
