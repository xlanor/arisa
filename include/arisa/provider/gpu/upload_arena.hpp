#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/alignment.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <span>
#include <type_traits>
#include <utility>

namespace arisa::provider::gpu {

template <class Chunk>
concept UploadChunk = std::movable<Chunk> && requires(const Chunk& chunk) {
  { chunk.bytes() } -> std::convertible_to<std::span<std::byte>>;
};

template <UploadChunk Chunk>
class UploadArena {
 public:
  using Made = std::expected<Chunk, ProviderError>;

  struct Space {
    std::reference_wrapper<const Chunk> chunk;
    std::uint64_t offset{0};
    std::span<std::byte> bytes;
  };

  UploadArena(Alignment alignment, std::uint64_t minimum_chunk) noexcept
      : alignment_(alignment), minimum_chunk_(minimum_chunk) {}

  template <class Make>
    requires std::is_invocable_r_v<Made, Make&, std::uint64_t>
  [[nodiscard]] auto take(std::uint64_t size, Make&& make)
      -> std::expected<Space, ProviderError> {
    const std::uint64_t wanted = alignment_.up(size);
    for (Entry& entry : chunks_) {
      if (entry.fits(wanted)) {
        return entry.claim(size, wanted);
      }
    }
    const std::uint64_t previous =
        chunks_.empty() ? 0 : chunks_.back().capacity();
    Made made = std::invoke(
        make, std::max({wanted, minimum_chunk_, alignment_.up(2 * previous)}));
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    if (made->bytes().size() < wanted) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    chunks_.emplace_back(std::move(*made));
    return chunks_.back().claim(size, wanted);
  }

  void flush() const noexcept
    requires requires(const Chunk& chunk, std::uint64_t length) {
      chunk.flush(length, length);
    }
  {
    for (const Entry& entry : chunks_) {
      entry.flush();
    }
  }

  void reset() noexcept {
    if (chunks_.size() > 1) {
      auto largest = std::ranges::max_element(chunks_, {}, &Entry::capacity);
      Entry kept = std::move(*largest);
      chunks_.clear();
      chunks_.push_back(std::move(kept));
    }
    for (Entry& entry : chunks_) {
      entry.reset();
    }
  }

  auto chunk_count() const noexcept -> std::size_t { return chunks_.size(); }

  auto capacity() const noexcept -> std::uint64_t {
    std::uint64_t total = 0;
    for (const Entry& entry : chunks_) {
      total += entry.capacity();
    }
    return total;
  }

  auto used() const noexcept -> std::uint64_t {
    std::uint64_t total = 0;
    for (const Entry& entry : chunks_) {
      total += entry.used();
    }
    return total;
  }

 private:
  class Entry {
   public:
    explicit Entry(Chunk chunk) noexcept(
        std::is_nothrow_move_constructible_v<Chunk>)
        : chunk_(std::move(chunk)) {}

    auto capacity() const noexcept -> std::uint64_t {
      return chunk_.bytes().size();
    }

    auto used() const noexcept -> std::uint64_t { return used_; }

    auto fits(std::uint64_t wanted) const noexcept -> bool {
      return capacity() - used_ >= wanted;
    }

    auto claim(std::uint64_t size, std::uint64_t wanted) -> Space {
      const std::uint64_t offset = used_;
      used_ += wanted;
      return Space{.chunk = std::cref(chunk_),
                   .offset = offset,
                   .bytes = chunk_.bytes().subspan(offset, size)};
    }

    void flush() const noexcept { chunk_.flush(0, used_); }

    void reset() noexcept { used_ = 0; }

   private:
    Chunk chunk_;
    std::uint64_t used_{0};
  };

  Alignment alignment_;
  std::uint64_t minimum_chunk_;
  std::deque<Entry> chunks_;
};

}
