#pragma once

#include <arisa/support/unique_handle.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/writing_direction.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

struct _SBAlgorithm;
struct _SBParagraph;
struct _SBLine;
struct _SBScriptLocator;

namespace arisa::text {

class BidiText {
 public:
  struct VisualRun {
    std::uint32_t start{0};
    std::uint32_t end{0};
    std::uint8_t level{0};
  };

  [[nodiscard]] static auto create(
      std::string_view text, bool prefer_rtl,
      WritingDirection direction = WritingDirection::natural)
      -> std::expected<BidiText, TextError>;

  [[nodiscard]] static auto scripts(std::string_view text)
      -> std::expected<std::vector<std::uint32_t>, TextError>;

  auto level(std::size_t offset) const noexcept -> std::uint8_t;

  auto paragraph_end(std::size_t offset) const noexcept -> std::size_t;

  auto paragraph_right_to_left(std::size_t offset) const noexcept -> bool;

  auto visual_runs(std::size_t start, std::size_t end) const
      -> std::vector<VisualRun>;

 private:
  struct AlgorithmRelease {
    using handle_type = const _SBAlgorithm*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct ParagraphRelease {
    using handle_type = const _SBParagraph*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct LineRelease {
    using handle_type = const _SBLine*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct LocatorRelease {
    using handle_type = _SBScriptLocator*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type handle) noexcept -> bool {
      return handle != nullptr;
    }
    static void release(handle_type& handle) noexcept;
  };

  struct Paragraph {
    std::size_t start{0};
    std::size_t end{0};
    bool right_to_left{false};
    support::UniqueHandle<ParagraphRelease> handle;
  };

  BidiText() noexcept = default;

  auto paragraph(std::size_t offset) const noexcept -> const Paragraph*;

  support::UniqueHandle<AlgorithmRelease> algorithm_;
  std::vector<Paragraph> paragraphs_;
  std::vector<std::uint8_t> levels_;
};

}
