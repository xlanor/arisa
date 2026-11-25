#include <plasma_shaders.hpp>
#include <vulkan_plasma_program.hpp>

#include <array>
#include <cstring>
#include <utility>

namespace arisa::examples {

using provider::vulkan::VulkanBuffer;
using provider::vulkan::VulkanDescriptorPoolHandle;
using provider::vulkan::VulkanDescriptorSetLayoutHandle;
using provider::vulkan::VulkanFramebufferHandle;
using provider::vulkan::VulkanPipelineHandle;
using provider::vulkan::VulkanPipelineLayoutHandle;
using provider::vulkan::VulkanRenderPassHandle;
using provider::vulkan::VulkanShaderModuleHandle;

VulkanPlasmaProgram::Uniforms::Uniforms(
    VulkanBuffer buffer, std::vector<VkDescriptorSet> sets) noexcept
    : buffer_(std::move(buffer)), sets_(std::move(sets)) {}

VulkanPlasmaProgram::VulkanPlasmaProgram(
    Key, const provider::vulkan::VulkanDevice& device, Layouts layouts,
    VulkanDescriptorPoolHandle pool,
    std::flat_map<VkFormat, Variant> variants) noexcept
    : device_(device),
      layouts_(std::move(layouts)),
      pool_(std::move(pool)),
      variants_(std::move(variants)) {}

VulkanPlasmaProgram::~VulkanPlasmaProgram() { device_.get().wait_idle(); }

auto VulkanPlasmaProgram::module(VkDevice device,
                                 std::span<const std::uint8_t> code)
    -> std::expected<VulkanShaderModuleHandle, std::string> {
  std::vector<std::uint32_t> words((code.size() + 3) / 4);
  std::memcpy(words.data(), code.data(), code.size());
  const VkShaderModuleCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = code.size(),
      .pCode = words.data()};
  VkShaderModule made = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &info, nullptr, &made) != VK_SUCCESS) {
    return std::unexpected(std::string{"a plasma shader did not load"});
  }
  return VulkanShaderModuleHandle{{.device = device, .object = made}};
}

auto VulkanPlasmaProgram::layouts(VkDevice device)
    -> std::expected<Layouts, std::string> {
  const VkDescriptorSetLayoutCreateInfo empty_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = 0,
      .pBindings = nullptr};
  const VkDescriptorSetLayoutBinding binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr};
  const VkDescriptorSetLayoutCreateInfo uniform_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &binding};
  VkDescriptorSetLayout empty = VK_NULL_HANDLE;
  VkDescriptorSetLayout uniforms = VK_NULL_HANDLE;
  if (vkCreateDescriptorSetLayout(device, &empty_info, nullptr, &empty) !=
      VK_SUCCESS) {
    return std::unexpected(std::string{"the plasma set layouts failed"});
  }
  Layouts made{.empty = VulkanDescriptorSetLayoutHandle{{.device = device,
                                                         .object = empty}},
               .uniforms = {},
               .pipeline = {}};
  if (vkCreateDescriptorSetLayout(device, &uniform_info, nullptr, &uniforms) !=
      VK_SUCCESS) {
    return std::unexpected(std::string{"the plasma set layouts failed"});
  }
  made.uniforms =
      VulkanDescriptorSetLayoutHandle{{.device = device, .object = uniforms}};
  const std::array sets{empty, empty, empty, uniforms};
  const VkPipelineLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = static_cast<std::uint32_t>(sets.size()),
      .pSetLayouts = sets.data(),
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr};
  VkPipelineLayout pipeline = VK_NULL_HANDLE;
  if (vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline) !=
      VK_SUCCESS) {
    return std::unexpected(std::string{"the plasma pipeline layout failed"});
  }
  made.pipeline =
      VulkanPipelineLayoutHandle{{.device = device, .object = pipeline}};
  return made;
}

auto VulkanPlasmaProgram::pool(VkDevice device)
    -> std::expected<VulkanDescriptorPoolHandle, std::string> {
  const VkDescriptorPoolSize size{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                  .descriptorCount = maximum_sets};
  const VkDescriptorPoolCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = maximum_sets,
      .poolSizeCount = 1,
      .pPoolSizes = &size};
  VkDescriptorPool made = VK_NULL_HANDLE;
  if (vkCreateDescriptorPool(device, &info, nullptr, &made) != VK_SUCCESS) {
    return std::unexpected(std::string{"the plasma descriptor pool failed"});
  }
  return VulkanDescriptorPoolHandle{{.device = device, .object = made}};
}

auto VulkanPlasmaProgram::variant(VkDevice device, VkFormat format,
                                  VkPipelineLayout layout,
                                  VkShaderModule vertex,
                                  VkShaderModule fragment)
    -> std::expected<Variant, std::string> {
  const VkAttachmentDescription attachment{
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  const VkAttachmentReference reference{
      .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  const VkSubpassDescription subpass{
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1,
      .pColorAttachments = &reference,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr};
  const VkRenderPassCreateInfo pass_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = 1,
      .pAttachments = &attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 0,
      .pDependencies = nullptr};
  VkRenderPass pass = VK_NULL_HANDLE;
  if (vkCreateRenderPass(device, &pass_info, nullptr, &pass) != VK_SUCCESS) {
    return std::unexpected(std::string{"a plasma render pass failed"});
  }
  Variant made{
      .render_pass = VulkanRenderPassHandle{{.device = device, .object = pass}},
      .pipeline = {}};
  const std::array stages{
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertex,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragment,
          .pName = "main",
          .pSpecializationInfo = nullptr}};
  const VkPipelineVertexInputStateCreateInfo input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr};
  const VkPipelineInputAssemblyStateCreateInfo assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};
  const VkPipelineViewportStateCreateInfo viewport{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr};
  const VkPipelineRasterizationStateCreateInfo raster{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0F,
      .depthBiasClamp = 0.0F,
      .depthBiasSlopeFactor = 0.0F,
      .lineWidth = 1.0F};
  const VkPipelineMultisampleStateCreateInfo multisample{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 0.0F,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE};
  const VkPipelineColorBlendAttachmentState blend_attachment{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
  const VkPipelineColorBlendStateCreateInfo blend{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &blend_attachment,
      .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F}};
  const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT,
                                  VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data()};
  const VkGraphicsPipelineCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = static_cast<std::uint32_t>(stages.size()),
      .pStages = stages.data(),
      .pVertexInputState = &input,
      .pInputAssemblyState = &assembly,
      .pTessellationState = nullptr,
      .pViewportState = &viewport,
      .pRasterizationState = &raster,
      .pMultisampleState = &multisample,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &blend,
      .pDynamicState = &dynamic,
      .layout = layout,
      .renderPass = pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1};
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                                &pipeline) != VK_SUCCESS) {
    return std::unexpected(std::string{"a plasma pipeline failed"});
  }
  made.pipeline = VulkanPipelineHandle{{.device = device, .object = pipeline}};
  return made;
}

auto VulkanPlasmaProgram::create(const provider::vulkan::VulkanDevice& device,
                                 std::span<const VkFormat> formats)
    -> std::expected<std::shared_ptr<VulkanPlasmaProgram>, std::string> {
  VkDevice const handle = device.handle();
  auto vertex = module(handle, PlasmaShaders::vertex());
  auto fragment = module(handle, PlasmaShaders::fragment());
  if (!vertex.has_value() || !fragment.has_value()) {
    return std::unexpected(std::string{"the plasma shaders did not load"});
  }
  auto made_layouts = layouts(handle);
  if (!made_layouts.has_value()) {
    return std::unexpected(made_layouts.error());
  }
  auto made_pool = pool(handle);
  if (!made_pool.has_value()) {
    return std::unexpected(made_pool.error());
  }
  std::flat_map<VkFormat, Variant> variants;
  for (const VkFormat format : formats) {
    if (variants.contains(format)) {
      continue;
    }
    auto made = variant(handle, format, made_layouts->pipeline.get().object,
                        vertex->get().object, fragment->get().object);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    variants.insert_or_assign(format, std::move(*made));
  }
  return std::make_shared<VulkanPlasmaProgram>(
      Key{}, device, std::move(*made_layouts), std::move(*made_pool),
      std::move(variants));
}

auto VulkanPlasmaProgram::uniforms(std::uint32_t slots)
    -> std::expected<Uniforms, std::string> {
  auto buffer = VulkanBuffer::create(
      device_.get(), uniform_stride * slots,
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      provider::vulkan::MemoryUse::device_local);
  if (!buffer.has_value()) {
    return std::unexpected(std::string{"the plasma uniforms got no memory"});
  }
  VkDevice const handle = device_.get().handle();
  std::vector<VkDescriptorSetLayout> layouts(slots,
                                             layouts_.uniforms.get().object);
  std::vector<VkDescriptorSet> sets(slots, VK_NULL_HANDLE);
  const VkDescriptorSetAllocateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = pool_.get().object,
      .descriptorSetCount = slots,
      .pSetLayouts = layouts.data()};
  if (vkAllocateDescriptorSets(handle, &info, sets.data()) != VK_SUCCESS) {
    return std::unexpected(std::string{"the plasma ran out of uniform sets"});
  }
  for (std::uint32_t slot = 0; slot < slots; ++slot) {
    const VkDescriptorBufferInfo described{.buffer = buffer->handle(),
                                           .offset = uniform_stride * slot,
                                           .range = uniform_range};
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = sets[slot],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &described,
        .pTexelBufferView = nullptr};
    vkUpdateDescriptorSets(handle, 1, &write, 0, nullptr);
  }
  return Uniforms{std::move(*buffer), std::move(sets)};
}

auto VulkanPlasmaProgram::framebuffer(VkImageView view, VkFormat format,
                                      VkExtent2D extent) const
    -> std::expected<VulkanFramebufferHandle, std::string> {
  const auto found = variants_.find(format);
  if (found == variants_.end()) {
    return std::unexpected(
        std::string{"the plasma has no pipeline for a format"});
  }
  VkDevice const handle = device_.get().handle();
  const VkFramebufferCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderPass = found->second.render_pass.get().object,
      .attachmentCount = 1,
      .pAttachments = &view,
      .width = extent.width,
      .height = extent.height,
      .layers = 1};
  VkFramebuffer made = VK_NULL_HANDLE;
  if (vkCreateFramebuffer(handle, &info, nullptr, &made) != VK_SUCCESS) {
    return std::unexpected(std::string{"a plasma framebuffer failed"});
  }
  return VulkanFramebufferHandle{{.device = handle, .object = made}};
}

auto VulkanPlasmaProgram::Pass::covering(VkFramebuffer framebuffer,
                                         VkFormat format, VkExtent2D extent)
    -> Pass {
  return Pass{
      .framebuffer = framebuffer,
      .format = format,
      .extent = extent,
      .area = VkRect2D{.offset = VkOffset2D{.x = 0, .y = 0}, .extent = extent},
      .viewport = VkViewport{.x = 0.0F,
                             .y = 0.0F,
                             .width = static_cast<float>(extent.width),
                             .height = static_cast<float>(extent.height),
                             .minDepth = 0.0F,
                             .maxDepth = 1.0F}};
}

auto VulkanPlasmaProgram::Pass::within(
    const provider::SurfaceFrame& frame) const -> Pass {
  Pass placed = *this;
  placed.area = VkRect2D{
      .offset = VkOffset2D{.x = static_cast<std::int32_t>(frame.scissor.x),
                           .y = static_cast<std::int32_t>(frame.scissor.y)},
      .extent = VkExtent2D{.width = frame.scissor.width,
                           .height = frame.scissor.height}};
  placed.viewport = VkViewport{.x = frame.viewport.x,
                               .y = frame.viewport.y,
                               .width = frame.viewport.width,
                               .height = frame.viewport.height,
                               .minDepth = 0.0F,
                               .maxDepth = 1.0F};
  return placed;
}

auto VulkanPlasmaProgram::target(
    const provider::vulkan::VulkanGpu::FrameTarget& target)
    -> std::expected<Pass, std::string> {
  ++targets_;
  std::erase_if(retired_, [this](const Retired& retired) {
    return targets_ - retired.at > retire_after;
  });
  const auto found = shown_.find(target.index);
  if (found != shown_.end() && found->second.generation == target.generation &&
      found->second.view == target.view &&
      found->second.extent.width == target.extent.width &&
      found->second.extent.height == target.extent.height) {
    return Pass::covering(found->second.framebuffer.get().object, target.format,
                          target.extent);
  }
  auto made = framebuffer(target.view, target.format, target.extent);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  if (found != shown_.end()) {
    retired_.push_back(Retired{
        .at = targets_, .framebuffer = std::move(found->second.framebuffer)});
  }
  const VkFramebuffer raw = made->get().object;
  shown_.insert_or_assign(target.index, Shown{.generation = target.generation,
                                              .view = target.view,
                                              .extent = target.extent,
                                              .framebuffer = std::move(*made)});
  return Pass::covering(raw, target.format, target.extent);
}

void VulkanPlasmaProgram::uniform_barrier(VkCommandBuffer commands,
                                          VkBuffer buffer, VkDeviceSize offset,
                                          bool writing) noexcept {
  const VkBufferMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = writing ? VkAccessFlags{VK_ACCESS_UNIFORM_READ_BIT}
                               : VkAccessFlags{VK_ACCESS_TRANSFER_WRITE_BIT},
      .dstAccessMask = writing ? VkAccessFlags{VK_ACCESS_TRANSFER_WRITE_BIT}
                               : VkAccessFlags{VK_ACCESS_UNIFORM_READ_BIT},
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer,
      .offset = offset,
      .size = uniform_stride};
  vkCmdPipelineBarrier(
      commands,
      writing ? VkPipelineStageFlags{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}
              : VkPipelineStageFlags{VK_PIPELINE_STAGE_TRANSFER_BIT},
      writing ? VkPipelineStageFlags{VK_PIPELINE_STAGE_TRANSFER_BIT}
              : VkPipelineStageFlags{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
      0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanPlasmaProgram::record(VkCommandBuffer commands, const Pass& pass,
                                 const Uniforms& uniforms, std::size_t slot,
                                 std::span<const std::byte> values) const {
  const auto found = variants_.find(pass.format);
  if (found == variants_.end() || slot >= uniforms.slots()) {
    return;
  }
  const VkDeviceSize offset = uniform_stride * slot;
  uniform_barrier(commands, uniforms.buffer(), offset, true);
  vkCmdUpdateBuffer(commands, uniforms.buffer(), offset, values.size(),
                    values.data());
  uniform_barrier(commands, uniforms.buffer(), offset, false);
  const VkRenderPassBeginInfo begin{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = found->second.render_pass.get().object,
      .framebuffer = pass.framebuffer,
      .renderArea = pass.area,
      .clearValueCount = 0,
      .pClearValues = nullptr};
  vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(commands, 0, 1, &pass.viewport);
  vkCmdSetScissor(commands, 0, 1, &pass.area);
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    found->second.pipeline.get().object);
  const VkDescriptorSet set = uniforms.set(slot);
  vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          layouts_.pipeline.get().object, 3, 1, &set, 0,
                          nullptr);
  vkCmdDraw(commands, 3, 1, 0, 0);
  vkCmdEndRenderPass(commands);
}

}
