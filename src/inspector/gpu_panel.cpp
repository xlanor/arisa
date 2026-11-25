#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/gpu_panel.hpp>
#include <arisa/inspector/inspector_style.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::inspector {

auto GpuPanel::drive(provider::FrameDrive drive) -> std::string {
  return drive == provider::FrameDrive::continuous ? "continuous" : "on demand";
}

auto GpuPanel::rate(const std::optional<double>& frame_rate) -> std::string {
  return frame_rate.has_value() ? std::format(" {:g} fps", *frame_rate)
                                : std::string{};
}

auto GpuPanel::state(const provider::TexturePublications::Listing& listing)
    -> std::string {
  if (listing.retired) {
    return "retired";
  }
  if (listing.sampled > listing.completed) {
    return "in flight";
  }
  return listing.generation > 0 ? "published" : "pending";
}

auto GpuPanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  static_cast<void>(context.read(model_->frames()));
  const runtime::Introspection::GpuRecord gpu = model_->introspection().gpu();
  const provider::GpuInventory& inventory = gpu.inventory;
  std::vector<composition::Element> sections;
  std::vector<composition::Element> device;
  device.push_back(InspectorStyle::value("Backend", inventory.backend));
  device.push_back(InspectorStyle::value("Startup", inventory.report));
  if (inventory.frames_presented.has_value()) {
    device.push_back(InspectorStyle::value(
        "Presented", std::format("{}", *inventory.frames_presented)));
  }
  if (inventory.missed_vsyncs.has_value()) {
    device.push_back(InspectorStyle::value(
        "Missed vsyncs", std::format("{}", *inventory.missed_vsyncs)));
  }
  sections.push_back(InspectorStyle::section("Device", std::move(device)));
  std::vector<composition::Element> memory;
  memory.push_back(InspectorStyle::value(
      "Textures",
      std::format("{}  {}", inventory.textures.sampled,
                  InspectorStyle::bytes(inventory.textures.sampled_bytes))));
  memory.push_back(InspectorStyle::value(
      "Render targets",
      std::format("{}  {}", inventory.textures.targets,
                  InspectorStyle::bytes(inventory.textures.target_bytes))));
  memory.push_back(InspectorStyle::value(
      "Glyph atlas", std::format("{} {}  {}", gpu.atlas_pages,
                                 gpu.atlas_pages == 1 ? "page" : "pages",
                                 InspectorStyle::bytes(gpu.atlas_bytes))));
  memory.push_back(InspectorStyle::value(
      "Total", InspectorStyle::bytes(inventory.textures.sampled_bytes +
                                     inventory.textures.target_bytes)));
  sections.push_back(InspectorStyle::section("Memory", std::move(memory)));
  std::vector<composition::Element> pool;
  pool.push_back(
      InspectorStyle::value("Leased", std::format("{}", gpu.targets.leased)));
  pool.push_back(
      InspectorStyle::value("Idle", std::format("{}", gpu.targets.idle)));
  pool.push_back(
      InspectorStyle::value("Created", std::format("{}", gpu.targets.created)));
  pool.push_back(
      InspectorStyle::value("Reused", std::format("{}", gpu.targets.reused)));
  pool.push_back(InspectorStyle::value(
      "Released", std::format("{}", gpu.targets.released)));
  sections.push_back(
      InspectorStyle::section("Render target pool", std::move(pool)));
  std::vector<composition::Element> external;
  for (const provider::TexturePublications::Listing& listing : gpu.external) {
    external.push_back(InspectorStyle::value(
        std::format("Texture {}", listing.texture.value()),
        std::format("{}  {} plane{}  generation {}", state(listing),
                    listing.planes, listing.planes == 1 ? "" : "s",
                    listing.generation)));
  }
  if (external.empty()) {
    external.push_back(InspectorStyle::line("none"));
  }
  sections.push_back(
      InspectorStyle::section("External textures", std::move(external)));
  std::vector<composition::Element> hooks;
  for (const provider::FrameSchedule::Listing& listing : gpu.hooks) {
    hooks.push_back(
        InspectorStyle::value(std::format("Hook {}", listing.id.value()),
                              drive(listing.drive) + rate(listing.frame_rate)));
  }
  if (hooks.empty()) {
    hooks.push_back(InspectorStyle::line("none"));
  }
  sections.push_back(InspectorStyle::section("Frame hooks", std::move(hooks)));
  std::vector<composition::Element> surfaces;
  for (const provider::SurfaceDirectory::Listing& listing : gpu.surfaces) {
    surfaces.push_back(InspectorStyle::value(
        std::format("Surface {}", listing.surface.value()),
        std::format("{}  {}{}  {} x {}",
                    listing.mode == provider::SurfaceMode::underlay ? "underlay"
                                                                    : "texture",
                    drive(listing.drive), rate(listing.frame_rate),
                    listing.width, listing.height)));
  }
  if (surfaces.empty()) {
    surfaces.push_back(InspectorStyle::line("none"));
  }
  sections.push_back(InspectorStyle::section("Surfaces", std::move(surfaces)));
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          std::move(sections))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
