#include <arisa/provider/horizon/deko3d_handle.hpp>
#include <arisa/provider/horizon/deko3d_memory.hpp>

#include <limits>
#include <mutex>
#include <optional>
#include <utility>

namespace arisa::provider::horizon {

class Deko3dMemory::Pool {
 public:
  using MemBlockHandle = Deko3dHandle<DkMemBlock, dkMemBlockDestroy>;

  Pool(DkDevice device, std::uint32_t flags, std::uint32_t block_size,
       std::uint32_t reserved_tail) noexcept
      : device_(device),
        flags_(flags),
        reserved_tail_(reserved_tail),
        blocks_(gpu::SuballocatorPolicy{
            .block_size = block_size,
            .dedicated_above = block_size - reserved_tail}) {}

  auto allocate(std::uint32_t size, gpu::Alignment alignment)
      -> std::expected<Allocation::Range, ProviderError> {
    const std::scoped_lock lock{mutex_};
    return blocks_
        .allocate(size, alignment,
                  [this](gpu::BlockRequest request) { return create(request); })
        .transform(
            [this,
             size](const gpu::Suballocator<MemBlockHandle>::Placement& placed) {
              return Allocation::Range{
                  .pool = this,
                  .memblock = placed.native.get().get(),
                  .block = placed.block,
                  .offset = static_cast<std::uint32_t>(placed.range.offset),
                  .size = size};
            });
  }

  void release(const Allocation::Range& range) noexcept {
    const std::scoped_lock lock{mutex_};
    blocks_.release(range.block,
                    gpu::ByteRange{.offset = range.offset, .size = range.size});
  }

  auto statistics() const -> gpu::SuballocatorStatistics {
    const std::scoped_lock lock{mutex_};
    return blocks_.statistics();
  }

 private:
  auto create(gpu::BlockRequest request)
      -> std::expected<gpu::NativeBlock<MemBlockHandle>, ProviderError> {
    const std::uint64_t bytes =
        request.dedicated ? align_up(request.bytes + request.alignment.bytes() +
                                         reserved_tail_,
                                     DK_MEMBLOCK_ALIGNMENT)
                          : request.bytes;
    if (bytes > std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    DkMemBlockMaker maker{};
    dkMemBlockMakerDefaults(&maker, device_, static_cast<std::uint32_t>(bytes));
    maker.flags = flags_;
    MemBlockHandle memblock{dkMemBlockCreate(&maker)};
    if (!memblock.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    const DkGpuAddr base = dkMemBlockGetGpuAddr(memblock.get());
    return gpu::NativeBlock<MemBlockHandle>{.native = std::move(memblock),
                                            .base = base,
                                            .usable = bytes - reserved_tail_,
                                            .reserved = bytes};
  }

  DkDevice device_;
  std::uint32_t flags_;
  std::uint32_t reserved_tail_;
  mutable std::mutex mutex_;
  gpu::Suballocator<MemBlockHandle> blocks_;
};

auto Deko3dMemory::Allocation::gpu_address() const noexcept -> DkGpuAddr {
  if (!valid()) {
    return DK_GPU_ADDR_INVALID;
  }
  return dkMemBlockGetGpuAddr(range_.get().memblock) + range_.get().offset;
}

auto Deko3dMemory::Allocation::cpu_bytes() const noexcept
    -> std::span<std::byte> {
  if (!valid()) {
    return {};
  }
  void* const base = dkMemBlockGetCpuAddr(range_.get().memblock);
  if (base == nullptr) {
    return {};
  }
  return std::span{static_cast<std::byte*>(base),
                   dkMemBlockGetSize(range_.get().memblock)}
      .subspan(range_.get().offset, range_.get().size);
}

void Deko3dMemory::Allocation::Release::release(handle_type& range) noexcept {
  range.pool->release(range);
}

Deko3dMemory::Deko3dMemory(DkDevice device)
    : pools_{
          std::make_unique<Pool>(device,
                                 DkMemBlockFlags_CpuUncached |
                                     DkMemBlockFlags_GpuCached |
                                     DkMemBlockFlags_Code,
                                 code_block_size, DK_SHADER_CODE_UNUSABLE_SIZE),
          std::make_unique<Pool>(
              device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image,
              image_block_size, 0),
          std::make_unique<Pool>(
              device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached,
              data_block_size, 0)} {}

Deko3dMemory::Deko3dMemory(Deko3dMemory&& other) noexcept = default;

auto Deko3dMemory::operator=(Deko3dMemory&& other) noexcept
    -> Deko3dMemory& = default;

Deko3dMemory::~Deko3dMemory() = default;

auto Deko3dMemory::allocate(Deko3dMemoryKind kind, std::uint32_t size,
                            std::uint32_t alignment)
    -> std::expected<Allocation, ProviderError> {
  const std::optional<gpu::Alignment> aligned =
      gpu::Alignment::create(alignment);
  if (size == 0 || !aligned.has_value()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return pool(kind)
      .allocate(size, *aligned)
      .transform([](Allocation::Range range) { return Allocation{range}; });
}

auto Deko3dMemory::block_count(Deko3dMemoryKind kind) const -> std::size_t {
  return pool(kind).statistics().blocks_live;
}

auto Deko3dMemory::reserved_bytes(Deko3dMemoryKind kind) const
    -> std::uint64_t {
  return pool(kind).statistics().reserved_bytes;
}

auto Deko3dMemory::created() const -> std::uint64_t {
  std::uint64_t total = 0;
  for (const std::unique_ptr<Pool>& each : pools_) {
    total += each->statistics().blocks_created;
  }
  return total;
}

auto Deko3dMemory::pool(Deko3dMemoryKind kind) const noexcept -> Pool& {
  return *pools_[static_cast<std::size_t>(kind)];
}

}
