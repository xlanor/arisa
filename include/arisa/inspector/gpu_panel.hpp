#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/texture_publications.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::inspector {

class GpuPanel final : public composition::Component {
 public:
  explicit GpuPanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const GpuPanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto drive(provider::FrameDrive drive) -> std::string;

  static auto rate(const std::optional<double>& frame_rate) -> std::string;

  static auto state(const provider::TexturePublications::Listing& listing)
      -> std::string;

  std::shared_ptr<InspectorModel> model_;
};

}
