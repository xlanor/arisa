#pragma once

#include <arisa/components/attributed_string.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace arisa::components {

class InlineMarkdown {
 public:
  explicit InlineMarkdown(std::string_view source);

  auto parsed() const -> AttributedString;

 private:
  enum class Kind : std::uint8_t {
    text,
    code,
    delimiter,
    link_open,
    link_close,
    autolink,
    image,
  };

  enum Style : std::uint8_t {
    emphasis = 0,
    strong = 1,
    struck = 2,
    style_count = 3,
  };

  struct Item {
    Kind kind{Kind::text};
    std::string_view text{};
    char marker{0};
    std::uint32_t count{0};
    std::uint32_t remaining{0};
    bool opens{false};
    bool closes{false};
    std::uint32_t partner{0};
    bool matched{false};
    std::string_view destination{};
  };

  struct Span {
    std::size_t open{0};
    std::size_t close{0};
    Style style{emphasis};
  };

  static auto punctuation(char character) noexcept -> bool;
  static auto space(char character) noexcept -> bool;

  void scan();
  void text(std::size_t start, std::size_t end);
  auto code(std::size_t at) -> std::size_t;
  auto delimiter(std::size_t at) -> std::size_t;
  auto closing(std::size_t at) -> std::size_t;
  auto autolink(std::size_t at) -> std::size_t;
  auto image(std::size_t at) -> std::size_t;
  void line_break(std::size_t& start, std::size_t at);
  void emphasise();
  void link();

  std::string_view source_;
  std::vector<Item> items_;
  std::vector<Span> spans_;
};

}
