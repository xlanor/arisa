#pragma once

#include <arisa/text/text_run.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace arisa::text {

class TextRuns {
 public:
  TextRuns() noexcept = default;

  explicit TextRuns(std::vector<TextRun> runs);

  auto empty() const noexcept -> bool { return runs_ == nullptr; }

  auto runs() const noexcept -> std::span<const TextRun>;

  auto find(std::uint32_t offset) const noexcept
      -> std::optional<std::reference_wrapper<const TextRun>>;

  auto spliced(std::uint32_t kept_head, std::uint32_t inserted,
               std::uint32_t tail_start, std::uint32_t size) const -> TextRuns;

  auto shifted(std::uint32_t offset) const -> std::vector<TextRun>;

  auto hash() const noexcept -> std::size_t;

  friend auto operator==(const TextRuns& left, const TextRuns& right) -> bool;

 private:
  std::shared_ptr<const std::vector<TextRun>> runs_;
};

}
