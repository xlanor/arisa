#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>
#include <provider/vulkan/shape_pipelines.hpp>
#include <provider/vulkan/vulkan_shaders.hpp>

#include <cstddef>
#include <utility>

namespace arisa::provider::vulkan {

auto ShapePipelines::attribute(std::uint32_t location, VkFormat format,
                               std::size_t offset) noexcept
    -> VkVertexInputAttributeDescription {
  return VkVertexInputAttributeDescription{
      .location = location,
      .binding = 0,
      .format = format,
      .offset = static_cast<std::uint32_t>(offset)};
}

auto ShapePipelines::textured_layout() -> VertexLayout {
  return VertexLayout{
      .stride = sizeof(Vertex),
      .attributes = {
          attribute(0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position)),
          attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)),
          attribute(2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, colour))}};
}

auto ShapePipelines::shape_layout() -> VertexLayout {
  return VertexLayout{
      .stride = sizeof(ShapeVertex),
      .attributes = {
          attribute(0, VK_FORMAT_R32G32_SFLOAT,
                    offsetof(ShapeVertex, position)),
          attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(ShapeVertex, uv)),
          attribute(2, VK_FORMAT_R32G32_SFLOAT, offsetof(ShapeVertex, local)),
          attribute(3, VK_FORMAT_R32G32_SFLOAT,
                    offsetof(ShapeVertex, half_size)),
          attribute(4, VK_FORMAT_R32G32B32A32_SFLOAT,
                    offsetof(ShapeVertex, shape)),
          attribute(5, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ShapeVertex, colour)),
          attribute(6, VK_FORMAT_R32_UINT, offsetof(ShapeVertex, kind)),
          attribute(7, VK_FORMAT_R32G32B32A32_SFLOAT,
                    offsetof(ShapeVertex, effect)),
          attribute(8, VK_FORMAT_R32G32B32A32_SFLOAT,
                    offsetof(ShapeVertex, detail))}};
}

auto ShapePipelines::module(VkDevice device, std::span<const std::uint8_t> code)
    -> std::expected<VulkanShaderModuleHandle, ProviderError> {
  if (code.empty() || code.size() % sizeof(std::uint32_t) != 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const VkShaderModuleCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = code.size(),
      .pCode = static_cast<const std::uint32_t*>(
          static_cast<const void*>(code.data()))};
  VkShaderModule raw = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateShaderModule(device, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return VulkanShaderModuleHandle{{.device = device, .object = raw}};
}

auto ShapePipelines::build(VkDevice device, VkRenderPass pass,
                           VkPipelineLayout layout, Stage stage,
                           const VertexLayout& vertices)
    -> std::expected<VulkanPipelineHandle, ProviderError> {
  auto vertex = module(device, stage.vertex);
  auto fragment = module(device, stage.fragment);
  if (!vertex.has_value()) {
    return std::unexpected(vertex.error());
  }
  if (!fragment.has_value()) {
    return std::unexpected(fragment.error());
  }
  const std::array stages{
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertex->get().object,
          .pName = "main",
          .pSpecializationInfo = nullptr},
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragment->get().object,
          .pName = "main",
          .pSpecializationInfo = nullptr}};
  const VkVertexInputBindingDescription binding{
      .binding = 0,
      .stride = vertices.stride,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  const VkPipelineVertexInputStateCreateInfo input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount =
          static_cast<std::uint32_t>(vertices.attributes.size()),
      .pVertexAttributeDescriptions = vertices.attributes.data()};
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
  const VkPipelineRasterizationStateCreateInfo rasterization{
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
  const VkPipelineColorBlendAttachmentState blend{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
  const VkPipelineColorBlendStateCreateInfo blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &blend,
      .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F}};
  const std::array dynamic{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic.size()),
      .pDynamicStates = dynamic.data()};
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
      .pRasterizationState = &rasterization,
      .pMultisampleState = &multisample,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &blending,
      .pDynamicState = &dynamic_state,
      .layout = layout,
      .renderPass = pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1};
  VkPipeline raw = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateGraphicsPipelines(
          device, VK_NULL_HANDLE, 1, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return VulkanPipelineHandle{{.device = device, .object = raw}};
}

auto ShapePipelines::make_sampler(VkDevice device, VkFilter filter)
    -> std::expected<VulkanSamplerHandle, ProviderError> {
  const VkSamplerCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = filter,
      .minFilter = filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0F,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0F,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0F,
      .maxLod = 0.0F,
      .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
      .unnormalizedCoordinates = VK_FALSE};
  VkSampler raw = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateSampler(device, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return VulkanSamplerHandle{{.device = device, .object = raw}};
}

auto ShapePipelines::create(const VulkanDevice& device, VkFormat target)
    -> std::expected<ShapePipelines, ProviderError> {
  VkDevice const handle = device.handle();
  ShapePipelines pipelines;
  const VkDescriptorSetLayoutBinding binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr};
  const VkDescriptorSetLayoutCreateInfo set_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &binding};
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateDescriptorSetLayout(handle, &set_info, nullptr, &set_layout);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  pipelines.set_layout_ =
      VulkanDescriptorSetLayoutHandle{{.device = handle, .object = set_layout}};
  const std::array sets{set_layout, set_layout};
  const std::array ranges{
      VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                          .offset = 0,
                          .size = sizeof(VertexConstants)},
      VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                          .offset = fragment_offset,
                          .size = sizeof(FragmentConstants)}};
  const VkPipelineLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = static_cast<std::uint32_t>(sets.size()),
      .pSetLayouts = sets.data(),
      .pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size()),
      .pPushConstantRanges = ranges.data()};
  VkPipelineLayout layout = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreatePipelineLayout(handle, &layout_info, nullptr, &layout);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  pipelines.layout_ =
      VulkanPipelineLayoutHandle{{.device = handle, .object = layout}};
  auto linear = make_sampler(handle, VK_FILTER_LINEAR);
  auto nearest = make_sampler(handle, VK_FILTER_NEAREST);
  if (!linear.has_value()) {
    return std::unexpected(linear.error());
  }
  if (!nearest.has_value()) {
    return std::unexpected(nearest.error());
  }
  pipelines.linear_ = std::move(*linear);
  pipelines.nearest_ = std::move(*nearest);
  if (auto targeted = pipelines.retarget(device, target);
      !targeted.has_value()) {
    return std::unexpected(targeted.error());
  }
  auto offscreen = pipelines.build_target(handle, offscreen_format);
  if (!offscreen.has_value()) {
    return std::unexpected(offscreen.error());
  }
  pipelines.offscreen_ = std::move(*offscreen);
  return pipelines;
}

auto ShapePipelines::retarget(const VulkanDevice& device, VkFormat target)
    -> std::expected<void, ProviderError> {
  auto built = build_target(device.handle(), target);
  if (!built.has_value()) {
    return std::unexpected(built.error());
  }
  screen_ = std::move(*built);
  format_ = target;
  return {};
}

auto ShapePipelines::build_target(VkDevice handle, VkFormat format) const
    -> std::expected<Target, ProviderError> {
  const VkAttachmentDescription attachment{
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
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
  if (const VkResult result =
          vkCreateRenderPass(handle, &pass_info, nullptr, &pass);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanRenderPassHandle render_pass{{.device = handle, .object = pass}};
  auto textured = build(handle, pass, layout(),
                        Stage{.vertex = VulkanShaders::textured_vertex(),
                              .fragment = VulkanShaders::textured_fragment()},
                        textured_layout());
  if (!textured.has_value()) {
    return std::unexpected(textured.error());
  }
  auto shape = build(handle, pass, layout(),
                     Stage{.vertex = VulkanShaders::shape_vertex(),
                           .fragment = VulkanShaders::shape_fragment()},
                     shape_layout());
  if (!shape.has_value()) {
    return std::unexpected(shape.error());
  }
  return Target{.render_pass = std::move(render_pass),
                .textured = std::move(*textured),
                .shape = std::move(*shape)};
}

}
