#pragma once

#include <arisa/inspector/cost_ledger.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace arisa::inspector {

class FrameHistory {
 public:
  struct Entry {
    runtime::FrameTimings timings{};
    std::uint64_t number{0};
    std::vector<CostLedger::Ranked> top;
  };

  explicit FrameHistory(std::size_t capacity);

  void record(const runtime::FrameTimings& timings,
              std::vector<CostLedger::Ranked> top);

  void clear() noexcept;

  auto size() const noexcept -> std::size_t { return size_; }

  auto capacity() const noexcept -> std::size_t { return entries_.size(); }

  auto empty() const noexcept -> bool { return size_ == 0; }

  auto at(std::size_t index) const -> const Entry&;

  auto index_of(std::uint64_t number) const -> std::optional<std::size_t>;

  auto recorded() const noexcept -> std::uint64_t { return recorded_; }

 private:
  std::vector<Entry> entries_;
  std::size_t head_{0};
  std::size_t size_{0};
  std::uint64_t recorded_{0};
};

}
