#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/gpu_inventory.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/support/strong_value.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

namespace arisa::provider {

struct RegistryTicketTag;

using RegistryTicket = support::StrongValue<std::uint64_t, RegistryTicketTag>;

template <class Native>
class TextureRegistry {
 public:
  struct Record {
    TextureDescription description;
    Native native;
    std::uint64_t retired{0};
  };

  using Destroy = std::move_only_function<void(Native&) noexcept>;

  explicit TextureRegistry(Destroy destroy) noexcept
      : destroy_(std::move(destroy)) {}

  TextureRegistry(const TextureRegistry&) = delete;
  auto operator=(const TextureRegistry&) -> TextureRegistry& = delete;
  TextureRegistry(TextureRegistry&&) = delete;
  auto operator=(TextureRegistry&&) -> TextureRegistry& = delete;

  ~TextureRegistry() { retire(); }

  static auto adopt(const std::shared_ptr<TextureRegistry>& registry,
                    TextureDescription description, Native native) -> Texture {
    const TextureId id = registry->insert(description, std::move(native));
    return Texture{id, description,
                   support::ScopedCallback{
                       [registry, id]() noexcept { registry->release(id); }}};
  }

  auto find(TextureId id) const -> std::optional<Record> {
    const std::scoped_lock lock{mutex_};
    const auto found = records_.find(id.value());
    if (found == records_.end()) {
      return std::nullopt;
    }
    return found->second;
  }

  auto live(TextureId id) const -> std::optional<Record> {
    const std::scoped_lock lock{mutex_};
    const auto found = records_.find(id.value());
    if (found == records_.end() || found->second.retired != 0) {
      return std::nullopt;
    }
    return found->second;
  }

  auto encoded(TextureId id, EncodeStage stage) -> std::optional<Record> {
    const std::scoped_lock lock{mutex_};
    const auto found = records_.find(id.value());
    if (found == records_.end()) {
      misses_ = EncodeMiss{
          .count = misses_.count + 1, .texture = id.value(), .stage = stage};
      return std::nullopt;
    }
    return found->second;
  }

  auto encode_misses() const -> EncodeMiss {
    const std::scoped_lock lock{mutex_};
    return misses_;
  }

  auto refusal(std::span<const DrawCommand> commands) const
      -> std::optional<ProviderErrorDetail> {
    const std::scoped_lock lock{mutex_};
    for (const DrawCommand& command : commands) {
      if (const auto refused = state(command.texture)) {
        return refused;
      }
      if (command.detail.value() != 0) {
        if (const auto refused = state(command.detail)) {
          return refused;
        }
      }
    }
    return std::nullopt;
  }

  auto size() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return records_.size() -
           static_cast<std::size_t>(std::ranges::count_if(
               records_ | std::views::values,
               [](const Record& record) { return record.retired != 0; }));
  }

  auto open() -> RegistryTicket {
    const std::scoped_lock lock{mutex_};
    const std::uint64_t ticket = ++issued_;
    open_.insert(ticket);
    return RegistryTicket{ticket};
  }

  void close(RegistryTicket ticket) noexcept {
    const std::scoped_lock lock{mutex_};
    open_.erase(ticket.value());
    if (graves_ == 0) {
      return;
    }
    const std::uint64_t floor = open_.empty()
                                    ? std::numeric_limits<std::uint64_t>::max()
                                    : *open_.begin();
    for (auto entry = records_.begin(); entry != records_.end();) {
      if (entry->second.retired != 0 && entry->second.retired < floor) {
        destroy_(entry->second.native);
        entry = records_.erase(entry);
        --graves_;
      } else {
        ++entry;
      }
    }
  }

  auto memory() const -> GpuInventory::Textures {
    const std::scoped_lock lock{mutex_};
    GpuInventory::Textures found;
    for (const Record& record : records_ | std::views::values) {
      if (record.retired != 0) {
        continue;
      }
      const TextureDescription& described = record.description;
      const std::uint64_t bytes = static_cast<std::uint64_t>(described.width) *
                                  described.height *
                                  Texture::bytes_per_pixel(described.format);
      if (described.usage == TextureUsage::render_target) {
        ++found.targets;
        found.target_bytes += bytes;
      } else {
        ++found.sampled;
        found.sampled_bytes += bytes;
      }
    }
    return found;
  }

  auto retired() const -> bool {
    const std::scoped_lock lock{mutex_};
    return retired_;
  }

  void retire() noexcept {
    const std::scoped_lock lock{mutex_};
    if (retired_) {
      return;
    }
    for (auto&& entry : records_) {
      destroy_(entry.second.native);
    }
    records_.clear();
    graves_ = 0;
    destroy_ = nullptr;
    retired_ = true;
  }

 private:
  auto insert(TextureDescription description, Native native) -> TextureId {
    const TextureId id = next_id();
    const std::scoped_lock lock{mutex_};
    records_.insert_or_assign(id.value(), Record{.description = description,
                                                 .native = std::move(native),
                                                 .retired = 0});
    return id;
  }

  void release(TextureId id) noexcept {
    const std::scoped_lock lock{mutex_};
    const auto found = records_.find(id.value());
    if (found == records_.end() || found->second.retired != 0) {
      return;
    }
    if (!open_.empty()) {
      found->second.retired = issued_;
      ++graves_;
      return;
    }
    destroy_(found->second.native);
    records_.erase(found);
  }

  auto state(TextureId id) const -> std::optional<ProviderErrorDetail> {
    const auto found = records_.find(id.value());
    if (found == records_.end()) {
      return ProviderErrorDetail::unknown_texture;
    }
    if (found->second.retired != 0) {
      return ProviderErrorDetail::released_texture;
    }
    return std::nullopt;
  }

  static auto next_id() noexcept -> TextureId {
    static std::atomic<std::uint64_t> counter{0};
    return TextureId{counter.fetch_add(1, std::memory_order_relaxed) + 1};
  }

  mutable std::mutex mutex_;
  Destroy destroy_;
  std::flat_map<std::uint64_t, Record> records_;
  std::flat_set<std::uint64_t> open_;
  std::uint64_t issued_{0};
  std::size_t graves_{0};
  EncodeMiss misses_{};
  bool retired_{false};
};

}
