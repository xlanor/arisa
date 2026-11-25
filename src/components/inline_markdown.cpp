#include <components/inline_markdown.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>

namespace arisa::components {

InlineMarkdown::InlineMarkdown(std::string_view source) : source_(source) {
  items_.reserve((source.size() / 8) + 1);
  scan();
  emphasise();
  link();
}

auto InlineMarkdown::punctuation(char character) noexcept -> bool {
  const auto code = static_cast<unsigned char>(character);
  return (code >= 0x21 && code <= 0x2F) || (code >= 0x3A && code <= 0x40) ||
         (code >= 0x5B && code <= 0x60) || (code >= 0x7B && code <= 0x7E);
}

auto InlineMarkdown::space(char character) noexcept -> bool {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r' || character == '\f' || character == '\v';
}

void InlineMarkdown::text(std::size_t start, std::size_t end) {
  if (end > start) {
    items_.push_back(
        Item{.kind = Kind::text, .text = source_.substr(start, end - start)});
  }
}

void InlineMarkdown::scan() {
  std::size_t start = 0;
  std::size_t at = 0;
  while (at < source_.size()) {
    const char character = source_[at];
    if (character == '\\' && at + 1 < source_.size() &&
        source_[at + 1] == '\n') {
      text(start, at);
      items_.push_back(
          Item{.kind = Kind::text, .text = source_.substr(at + 1, 1)});
      at += 2;
      start = at;
    } else if (character == '\n') {
      line_break(start, at);
      ++at;
    } else if (character == '<') {
      text(start, at);
      at = autolink(at);
      start = at;
    } else if (character == '!' && at + 1 < source_.size() &&
               source_[at + 1] == '[') {
      text(start, at);
      at = image(at);
      start = at;
    } else if (character == '\\' && at + 1 < source_.size() &&
               punctuation(source_[at + 1])) {
      text(start, at);
      items_.push_back(
          Item{.kind = Kind::text, .text = source_.substr(at + 1, 1)});
      at += 2;
      start = at;
    } else if (character == '`') {
      text(start, at);
      at = code(at);
      start = at;
    } else if (character == '*' || character == '_' || character == '~') {
      text(start, at);
      at = delimiter(at);
      start = at;
    } else if (character == '[') {
      text(start, at);
      items_.push_back(
          Item{.kind = Kind::link_open, .text = source_.substr(at, 1)});
      ++at;
      start = at;
    } else if (character == ']') {
      text(start, at);
      at = closing(at);
      start = at;
    } else {
      ++at;
    }
  }
  text(start, source_.size());
}

void InlineMarkdown::line_break(std::size_t& start, std::size_t at) {
  std::size_t spaces = 0;
  while (at - spaces > start && source_[at - spaces - 1] == ' ') {
    ++spaces;
  }
  if (spaces >= 2) {
    text(start, at - spaces);
    start = at;
  }
}

auto InlineMarkdown::autolink(std::size_t at) -> std::size_t {
  const std::size_t close = source_.find('>', at + 1);
  if (close == std::string_view::npos) {
    text(at, at + 1);
    return at + 1;
  }
  const std::string_view inside = source_.substr(at + 1, close - at - 1);
  const std::size_t colon = inside.find(':');
  const bool scheme =
      colon != std::string_view::npos && colon >= 2 &&
      std::ranges::all_of(inside.substr(0, colon),
                          [](char character) {
                            return (character >= 'a' && character <= 'z') ||
                                   (character >= 'A' && character <= 'Z') ||
                                   (character >= '0' && character <= '9') ||
                                   character == '+' || character == '.' ||
                                   character == '-';
                          }) &&
      std::ranges::none_of(inside, [](char character) {
        return space(character) || character == '<';
      });
  if (!scheme) {
    text(at, at + 1);
    return at + 1;
  }
  items_.push_back(
      Item{.kind = Kind::autolink, .text = inside, .destination = inside});
  return close + 1;
}

auto InlineMarkdown::image(std::size_t at) -> std::size_t {
  const std::size_t label_end = source_.find(']', at + 2);
  if (label_end == std::string_view::npos || label_end + 1 >= source_.size() ||
      source_[label_end + 1] != '(') {
    text(at, at + 1);
    return at + 1;
  }
  const std::size_t close = source_.find(')', label_end + 2);
  if (close == std::string_view::npos) {
    text(at, at + 1);
    return at + 1;
  }
  const std::string_view destination =
      source_.substr(label_end + 2, close - label_end - 2);
  if (destination.empty() || std::ranges::any_of(destination, space)) {
    text(at, at + 1);
    return at + 1;
  }
  items_.push_back(Item{.kind = Kind::image,
                        .text = source_.substr(at + 2, label_end - at - 2),
                        .destination = destination});
  return close + 1;
}

auto InlineMarkdown::code(std::size_t at) -> std::size_t {
  std::size_t end = at;
  while (end < source_.size() && source_[end] == '`') {
    ++end;
  }
  const std::size_t length = end - at;
  std::size_t search = end;
  while (search < source_.size()) {
    if (source_[search] != '`') {
      ++search;
      continue;
    }
    std::size_t run = search;
    while (run < source_.size() && source_[run] == '`') {
      ++run;
    }
    if (run - search == length) {
      std::string_view content = source_.substr(end, search - end);
      if (content.size() >= 2 && content.front() == ' ' &&
          content.back() == ' ' &&
          content.find_first_not_of(' ') != std::string_view::npos) {
        content = content.substr(1, content.size() - 2);
      }
      items_.push_back(Item{.kind = Kind::code, .text = content});
      return run;
    }
    search = run;
  }
  text(at, end);
  return end;
}

auto InlineMarkdown::delimiter(std::size_t at) -> std::size_t {
  const char marker = source_[at];
  std::size_t end = at;
  while (end < source_.size() && source_[end] == marker) {
    ++end;
  }
  const auto count = static_cast<std::uint32_t>(end - at);
  if (marker == '~' && count < 2) {
    text(at, end);
    return end;
  }
  const char before = at > 0 ? source_[at - 1] : ' ';
  const char after = end < source_.size() ? source_[end] : ' ';
  const bool left = !space(after) && (!punctuation(after) || space(before) ||
                                      punctuation(before));
  const bool right = !space(before) && (!punctuation(before) || space(after) ||
                                        punctuation(after));
  Item item{.kind = Kind::delimiter,
            .text = source_.substr(at, count),
            .marker = marker,
            .count = count,
            .remaining = count,
            .opens = left,
            .closes = right};
  if (marker == '_') {
    item.opens = left && (!right || punctuation(before));
    item.closes = right && (!left || punctuation(after));
  }
  items_.push_back(item);
  return end;
}

auto InlineMarkdown::closing(std::size_t at) -> std::size_t {
  if (at + 1 < source_.size() && source_[at + 1] == '(') {
    std::size_t end = at + 2;
    while (end < source_.size() && source_[end] != ')' &&
           !space(source_[end])) {
      ++end;
    }
    if (end < source_.size() && source_[end] == ')') {
      items_.push_back(
          Item{.kind = Kind::link_close,
               .text = source_.substr(at, end + 1 - at),
               .destination = source_.substr(at + 2, end - at - 2)});
      return end + 1;
    }
  }
  text(at, at + 1);
  return at + 1;
}

void InlineMarkdown::emphasise() {
  for (std::size_t close = 0; close < items_.size(); ++close) {
    if (items_[close].kind != Kind::delimiter || !items_[close].closes) {
      continue;
    }
    while (items_[close].remaining > 0) {
      std::optional<std::size_t> found;
      const std::size_t floor = close > AttributedString::markdown_depth
                                    ? close - AttributedString::markdown_depth
                                    : 0;
      for (std::size_t open = close; open-- > floor;) {
        const Item& candidate = items_[open];
        if (candidate.kind != Kind::delimiter || !candidate.opens ||
            candidate.marker != items_[close].marker ||
            candidate.remaining == 0) {
          continue;
        }
        const bool both = candidate.closes || items_[close].opens;
        const std::uint32_t sum = candidate.count + items_[close].count;
        if (candidate.marker != '~' && both && sum % 3 == 0 &&
            (candidate.count % 3 != 0 || items_[close].count % 3 != 0)) {
          continue;
        }
        found = open;
        break;
      }
      if (!found.has_value()) {
        break;
      }
      Item& opener = items_[*found];
      Item& closer = items_[close];
      std::uint32_t used = 1;
      Style style = emphasis;
      if (opener.marker == '~') {
        if (opener.remaining < 2 || closer.remaining < 2) {
          break;
        }
        used = 2;
        style = struck;
      } else if (opener.remaining >= 2 && closer.remaining >= 2) {
        used = 2;
        style = strong;
      }
      opener.remaining -= used;
      closer.remaining -= used;
      spans_.push_back(Span{.open = *found, .close = close, .style = style});
      for (std::size_t between = *found + 1; between < close; ++between) {
        if (items_[between].kind == Kind::delimiter) {
          items_[between].opens = false;
        }
      }
    }
  }
}

void InlineMarkdown::link() {
  std::vector<std::size_t> open;
  for (std::size_t index = 0; index < items_.size(); ++index) {
    if (items_[index].kind == Kind::link_open) {
      if (open.size() < AttributedString::markdown_depth) {
        open.push_back(index);
      }
    } else if (items_[index].kind == Kind::link_close && !open.empty()) {
      const std::size_t start = open.back();
      open.pop_back();
      items_[start].matched = true;
      items_[start].partner = static_cast<std::uint32_t>(index);
      items_[index].matched = true;
      items_[index].partner = static_cast<std::uint32_t>(start);
      open.clear();
    }
  }
}

auto InlineMarkdown::parsed() const -> AttributedString {
  const std::size_t count = items_.size();
  std::array<std::vector<int>, style_count> depth;
  for (std::vector<int>& levels : depth) {
    levels.assign(count + 1, 0);
  }
  for (const Span& span : spans_) {
    depth[span.style][span.open + 1] += 1;
    depth[span.style][span.close] -= 1;
  }
  std::array<int, style_count> active{};
  std::optional<std::string_view> destination;
  AttributedString result;
  for (std::size_t index = 0; index < count; ++index) {
    for (std::size_t style = 0; style < style_count; ++style) {
      active[style] += depth[style][index];
    }
    const Item& item = items_[index];
    AttributedString::Attributes attributes;
    if (active[strong] > 0) {
      attributes.weight = theme::Font::Weight::bold;
    }
    attributes.italic = active[emphasis] > 0;
    attributes.strikethrough = active[struck] > 0;
    if (destination.has_value()) {
      attributes.link = std::string{*destination};
    }
    switch (item.kind) {
      case Kind::text:
        result.append(item.text, attributes);
        break;
      case Kind::code:
        attributes.design = theme::Font::Design::monospaced;
        result.append(item.text, attributes);
        break;
      case Kind::delimiter:
        result.append(item.text.substr(0, item.remaining), attributes);
        break;
      case Kind::link_open:
        if (item.matched) {
          destination = items_[item.partner].destination;
        } else {
          result.append(item.text, attributes);
        }
        break;
      case Kind::link_close:
        if (item.matched) {
          destination.reset();
        } else {
          result.append(item.text, attributes);
        }
        break;
      case Kind::autolink:
        attributes.link = std::string{item.destination};
        result.append(item.text, attributes);
        break;
      case Kind::image:
        result.append(AttributedString::image(
            AttributedString::InlineImage{.pixels = nullptr,
                                          .scale = 1.0F,
                                          .name = std::string{item.destination},
                                          .label = std::string{item.text}},
            attributes));
        break;
    }
  }
  return result;
}

}
