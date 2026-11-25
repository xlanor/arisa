#include <vulkan_plasma.hpp>

#include <array>
#include <utility>

namespace arisa::examples {

VulkanPlasma::VulkanPlasma(Painting screen, Painting surface,
                           provider::FrameHook hook,
                           provider::GpuSurface texture,
                           provider::GpuSurface underlay) noexcept
    : program_(std::move(screen.program)),
      uniforms_(std::move(screen.uniforms)),
      surface_program_(std::move(surface.program)),
      surface_uniforms_(std::move(surface.uniforms)),
      hook_(std::move(hook)),
      texture_(std::move(texture)),
      underlay_(std::move(underlay)) {}

VulkanPlasma::~VulkanPlasma() {
  texture_.reset();
  underlay_.reset();
  hook_.reset();
  if (program_ != nullptr) {
    program_->device().wait_idle();
  }
}

auto VulkanPlasma::painting(const provider::vulkan::VulkanGpu& gpu,
                            std::uint32_t slots)
    -> std::expected<Painting, std::string> {
  const std::array formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM,
                           VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM};
  auto program = VulkanPlasmaProgram::create(gpu.device(), formats);
  if (!program.has_value()) {
    return std::unexpected(program.error());
  }
  auto made = (*program)->uniforms(slots);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  return Painting{
      .program = std::move(*program),
      .uniforms = std::make_shared<const VulkanPlasmaProgram::Uniforms>(
          std::move(*made))};
}

auto VulkanPlasma::create(provider::vulkan::VulkanGpu& gpu,
                          StageControl& control)
    -> std::expected<VulkanPlasma, std::string> {
  constexpr std::size_t hook_slot = 0;
  constexpr std::size_t underlay_slot = 1;
  auto screen = painting(gpu, 2);
  if (!screen.has_value()) {
    return std::unexpected(screen.error());
  }
  auto surface = painting(gpu, 1);
  if (!surface.has_value()) {
    return std::unexpected(surface.error());
  }
  provider::FrameHook hook = gpu.add_frame_hook(
      provider::FrameStage::before_ui,
      [painter = screen->program, slots = screen->uniforms, &control](
          VkCommandBuffer commands,
          const provider::vulkan::VulkanGpu::FrameTarget& target,
          const provider::FrameInfo& info) {
        if (control.page() != StageControl::Page::stage) {
          return;
        }
        const auto pass = painter->target(target);
        if (!pass.has_value()) {
          return;
        }
        painter->draw(commands, *pass, *slots, hook_slot,
                      control.advance(info));
      },
      StageControl::playing_options);
  auto texture = gpu.add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::texture},
      [painter = surface->program, slots = surface->uniforms, &control](
          VkCommandBuffer commands,
          const provider::vulkan::VulkanGpu::FrameTarget& target,
          const provider::SurfaceFrame& frame) {
        const auto pass = painter->target(target);
        if (!pass.has_value()) {
          return;
        }
        painter->draw(commands, *pass, *slots, 0, control.advance(frame));
      });
  auto underlay = gpu.add_surface(
      provider::SurfaceOptions{.mode = provider::SurfaceMode::underlay},
      [painter = screen->program, slots = screen->uniforms, &control](
          VkCommandBuffer commands,
          const provider::vulkan::VulkanGpu::FrameTarget& target,
          const provider::SurfaceFrame& frame) {
        const auto pass = painter->target(target);
        if (!pass.has_value()) {
          return;
        }
        painter->draw(commands, pass->within(frame), *slots, underlay_slot,
                      control.advance(frame));
      });
  if (!texture.has_value() || !underlay.has_value()) {
    return std::unexpected(std::string{"the plasma surfaces were refused"});
  }
  return VulkanPlasma{std::move(*screen), std::move(*surface), std::move(hook),
                      std::move(*texture), std::move(*underlay)};
}
}
