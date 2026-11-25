#include <arisa/provider/vulkan/vulkan_status.hpp>
#include <provider/vulkan/timestamp_queries.hpp>

#include <array>
#include <cmath>
#include <utility>

namespace arisa::provider::vulkan {

TimestampQueries::TimestampQueries(VulkanQueryPoolHandle pool, double period,
                                   std::uint64_t mask) noexcept
    : pool_(std::move(pool)), period_(period), mask_(mask) {}

auto TimestampQueries::create(const VulkanDevice& device)
    -> std::expected<TimestampQueries, ProviderError> {
  const VulkanCapabilities& capabilities = device.capabilities();
  if (capabilities.timestamp_bits == 0 ||
      capabilities.timestamp_period <= 0.0F) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  const VkQueryPoolCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = count,
      .pipelineStatistics = 0};
  VkQueryPool raw = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateQueryPool(device.handle(), &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  const std::uint64_t mask =
      capabilities.timestamp_bits >= 64
          ? ~std::uint64_t{0}
          : (std::uint64_t{1} << capabilities.timestamp_bits) - 1;
  return TimestampQueries{
      VulkanQueryPoolHandle{{.device = device.handle(), .object = raw}},
      static_cast<double>(capabilities.timestamp_period), mask};
}

void TimestampQueries::reset(VkCommandBuffer commands) const noexcept {
  vkCmdResetQueryPool(commands, pool_.get().object, 0, count);
}

void TimestampQueries::mark(VkCommandBuffer commands,
                            Mark mark) const noexcept {
  vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      pool_.get().object, std::to_underlying(mark));
}

auto TimestampQueries::nanoseconds(std::uint64_t from,
                                   std::uint64_t to) const noexcept
    -> std::chrono::nanoseconds {
  const std::uint64_t ticks = (to - from) & mask_;
  return std::chrono::nanoseconds{
      std::llround(static_cast<double>(ticks) * period_)};
}

auto TimestampQueries::read() -> Reading {
  const std::optional<Armed> armed = std::exchange(armed_, std::nullopt);
  if (!armed.has_value()) {
    return Reading{};
  }
  std::array<std::uint64_t, count> stamps{};
  const VkResult result = vkGetQueryPoolResults(
      pool_.get().device, pool_.get().object, 0, count, sizeof(stamps),
      stamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
  if (result != VK_SUCCESS) {
    return Reading{.timings = std::nullopt, .stalled = true};
  }
  const auto at = [&stamps](Mark mark) {
    return stamps[std::to_underlying(mark)];
  };
  const std::uint64_t started = at(Mark::started);
  const std::uint64_t effects = at(Mark::effects_drawn);
  const std::uint64_t under_started = at(Mark::under_started);
  const std::uint64_t under_drawn = at(Mark::under_drawn);
  const std::uint64_t drawn = at(Mark::interface_drawn);
  const std::uint64_t over = at(Mark::over_drawn);
  return Reading{
      .timings =
          GpuTimings{
              .effects = armed->passes > 0
                             ? std::optional{nanoseconds(started, effects)}
                             : std::nullopt,
              .interface = nanoseconds(effects, under_started) +
                           nanoseconds(under_drawn, drawn),
              .passes = armed->passes,
              .measured_on_gpu = true,
              .hooks = armed->hooked ? std::optional{nanoseconds(under_started,
                                                                 under_drawn) +
                                                     nanoseconds(drawn, over)}
                                     : std::nullopt},
      .stalled = false};
}

}
