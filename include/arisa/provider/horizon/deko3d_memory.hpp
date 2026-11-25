#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/suballocator.hpp>
#include <arisa/support/unique_handle.hpp>

#include <deko3d.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace arisa::provider::horizon {

enum class Deko3dMemoryKind : std::uint8_t {
  code,
  image,
  data,
};

class Deko3dMemory {
 public:
  class Pool;

  class Allocation {
   public:
    Allocation() noexcept = default;

    auto valid() const noexcept -> bool { return range_.valid(); }

    auto memblock() const noexcept -> DkMemBlock {
      return range_.get().memblock;
    }

    auto offset() const noexcept -> std::uint32_t {
      return range_.get().offset;
    }

    auto size() const noexcept -> std::uint32_t { return range_.get().size; }

    auto gpu_address() const noexcept -> DkGpuAddr;

    auto cpu_bytes() const noexcept -> std::span<std::byte>;

   private:
    friend class Deko3dMemory;
    friend class Pool;

    struct Range {
      Pool* pool;
      DkMemBlock memblock;
      gpu::BlockId block;
      std::uint32_t offset;
      std::uint32_t size;
    };

    struct Release {
      using handle_type = Range;

      static auto empty() noexcept -> handle_type {
        return Range{.pool = nullptr,
                     .memblock = nullptr,
                     .block = gpu::BlockId{0},
                     .offset = 0,
                     .size = 0};
      }

      static auto is_valid(const handle_type& range) noexcept -> bool {
        return range.pool != nullptr;
      }

      static void release(handle_type& range) noexcept;
    };

    explicit Allocation(Range range) noexcept : range_(range) {}

    support::UniqueHandle<Release> range_;
  };

  static constexpr std::uint32_t code_block_size = 0x10000;
  static constexpr std::uint32_t image_block_size = 0x1000000;
  static constexpr std::uint32_t data_block_size = 0x400000;

  explicit Deko3dMemory(DkDevice device);

  Deko3dMemory(const Deko3dMemory&) = delete;
  auto operator=(const Deko3dMemory&) -> Deko3dMemory& = delete;
  Deko3dMemory(Deko3dMemory&& other) noexcept;
  auto operator=(Deko3dMemory&& other) noexcept -> Deko3dMemory&;
  ~Deko3dMemory();

  [[nodiscard]] auto allocate(Deko3dMemoryKind kind, std::uint32_t size,
                              std::uint32_t alignment)
      -> std::expected<Allocation, ProviderError>;

  auto block_count(Deko3dMemoryKind kind) const -> std::size_t;

  auto reserved_bytes(Deko3dMemoryKind kind) const -> std::uint64_t;

  auto created() const -> std::uint64_t;

  static constexpr auto align_up(std::uint64_t value,
                                 std::uint64_t alignment) noexcept
      -> std::uint64_t {
    return (value + alignment - 1) & ~(alignment - 1);
  }

 private:
  auto pool(Deko3dMemoryKind kind) const noexcept -> Pool&;

  std::array<std::unique_ptr<Pool>, 3> pools_;
};

}
