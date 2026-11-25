#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/user_defaults.hpp>

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

template <class T>
concept NavigationCodable =
    Encodable<T> || std::same_as<T, std::string> ||
    (std::integral<T> && !std::same_as<T, bool>) || std::same_as<T, bool>;

class NavigationPath {
 public:
  class CodableRepresentation;

  class Entry {
   public:
    template <class T>
      requires std::equality_comparable<T> && std::copy_constructible<T>
    static auto of(T value) -> Entry {
      return Entry{std::make_shared<const Typed<T>>(std::move(value))};
    }

    static auto pending(std::string tag, std::string payload) -> Entry {
      return Entry{
          std::make_shared<const Pending>(std::move(tag), std::move(payload))};
    }

    template <class T>
    static auto type_of() noexcept -> Identity {
      return Identity{&tag<T>};
    }

    template <class T>
    static constexpr auto type_name() noexcept -> std::string_view {
      constexpr std::string_view signature = __PRETTY_FUNCTION__;
      constexpr std::string_view marker = "T = ";
      constexpr std::size_t from = signature.find(marker) + marker.size();
      constexpr std::size_t to = signature.find_first_of(";]", from);
      return signature.substr(from, to - from);
    }

    template <class T>
    auto get() const noexcept -> const T* {
      if (value_->type() == type_of<T>()) {
        return &static_cast<const Typed<T>&>(*value_).value();
      }
      if constexpr (NavigationCodable<T>) {
        if (value_->type() == Identity{&pending_tag}) {
          return static_cast<const Pending&>(*value_).template resolve<T>();
        }
      }
      return nullptr;
    }

    auto type() const noexcept -> Identity { return value_->type(); }

    auto tag_name() const -> std::string_view { return value_->tag_name(); }

    auto encoded() const -> std::optional<std::string> {
      return value_->encoded();
    }

    friend auto operator==(const Entry& left, const Entry& right) -> bool {
      return left.value_ == right.value_ ||
             (left.type() == right.type() &&
              left.value_->equals(*right.value_));
    }

   private:
    class Value {
     public:
      Value() = default;
      Value(const Value&) = delete;
      auto operator=(const Value&) -> Value& = delete;
      Value(Value&&) = delete;
      auto operator=(Value&&) -> Value& = delete;
      virtual ~Value() = default;

      virtual auto type() const noexcept -> Identity = 0;

      virtual auto equals(const Value& other) const -> bool = 0;

      virtual auto tag_name() const -> std::string_view = 0;

      virtual auto encoded() const -> std::optional<std::string> = 0;
    };

    template <class T>
    class Typed final : public Value {
     public:
      explicit Typed(T value) : value_(std::move(value)) {}

      auto type() const noexcept -> Identity override { return type_of<T>(); }

      auto equals(const Value& other) const -> bool override {
        return static_cast<const Typed&>(other).value_ == value_;
      }

      auto tag_name() const -> std::string_view override {
        if constexpr (NavigationCodable<T>) {
          return type_name<T>();
        } else {
          return {};
        }
      }

      auto encoded() const -> std::optional<std::string> override {
        if constexpr (Encodable<T>) {
          return std::string{value_.encoded()};
        } else if constexpr (std::same_as<T, std::string>) {
          return value_;
        } else if constexpr (std::same_as<T, bool>) {
          return std::string{value_ ? "1" : "0"};
        } else if constexpr (std::integral<T>) {
          return std::format("{}", value_);
        } else {
          return std::nullopt;
        }
      }

      auto value() const noexcept -> const T& { return value_; }

     private:
      T value_;
    };

    class Pending final : public Value {
     public:
      Pending(std::string tag, std::string payload) noexcept
          : tag_(std::move(tag)), payload_(std::move(payload)) {}

      auto type() const noexcept -> Identity override {
        return Identity{&pending_tag};
      }

      auto equals(const Value& other) const -> bool override {
        const auto& theirs = static_cast<const Pending&>(other);
        return theirs.tag_ == tag_ && theirs.payload_ == payload_;
      }

      auto tag_name() const -> std::string_view override { return tag_; }

      auto encoded() const -> std::optional<std::string> override {
        return payload_;
      }

      template <class T>
      auto resolve() const -> const T* {
        if (tag_ != type_name<T>()) {
          return nullptr;
        }
        if (resolved_ == nullptr) {
          std::optional<T> decoded = decode<T>(payload_);
          if (!decoded.has_value()) {
            return nullptr;
          }
          resolved_ = std::make_shared<const Typed<T>>(std::move(*decoded));
        }
        return &static_cast<const Typed<T>&>(*resolved_).value();
      }

     private:
      template <class T>
      static auto decode(std::string_view text) -> std::optional<T> {
        if constexpr (Encodable<T>) {
          return T::decoded(text);
        } else if constexpr (std::same_as<T, std::string>) {
          return std::string{text};
        } else if constexpr (std::same_as<T, bool>) {
          if (text == "1" || text == "0") {
            return text == "1";
          }
          return std::nullopt;
        } else {
          T value{};
          const auto [end, failure] =
              std::from_chars(text.data(), text.data() + text.size(), value);
          if (failure != std::errc{} || end != text.data() + text.size()) {
            return std::nullopt;
          }
          return value;
        }
      }

      std::string tag_;
      std::string payload_;
      mutable std::shared_ptr<const Value> resolved_;
    };

    template <class T>
    static constexpr char tag{};

    static constexpr char pending_tag{};

    explicit Entry(std::shared_ptr<const Value> value) noexcept
        : value_(std::move(value)) {}

    std::shared_ptr<const Value> value_;
  };

  NavigationPath() = default;

  template <std::ranges::input_range Elements>
    requires std::equality_comparable<std::ranges::range_value_t<Elements>> &&
             std::copy_constructible<std::ranges::range_value_t<Elements>> &&
             (!std::same_as<std::remove_cvref_t<Elements>, NavigationPath>) &&
             (!std::same_as<std::remove_cvref_t<Elements>,
                            CodableRepresentation>)
  explicit NavigationPath(Elements&& elements) {
    for (auto&& element : elements) {
      append(std::forward<decltype(element)>(element));
    }
  }

  explicit NavigationPath(const CodableRepresentation& codable);

  template <class T>
    requires std::equality_comparable<T> && std::copy_constructible<T>
  void append(T value) {
    entries_.push_back(Entry::of(std::move(value)));
  }

  void append(Entry entry) { entries_.push_back(std::move(entry)); }

  void remove_last(std::size_t count = 1) {
    entries_.erase(entries_.end() - static_cast<std::ptrdiff_t>(
                                        std::min(count, entries_.size())),
                   entries_.end());
  }

  void clear() noexcept { entries_.clear(); }

  auto count() const noexcept -> std::size_t { return entries_.size(); }

  auto empty() const noexcept -> bool { return entries_.empty(); }

  auto entries() const noexcept -> std::span<const Entry> { return entries_; }

  auto codable() const -> std::optional<CodableRepresentation>;

  friend auto operator==(const NavigationPath&, const NavigationPath&)
      -> bool = default;

 private:
  std::vector<Entry> entries_;
};

class NavigationPath::CodableRepresentation {
 public:
  struct Element {
    std::string tag;
    std::string payload;

    friend auto operator==(const Element&, const Element&) -> bool = default;
  };

  CodableRepresentation() = default;

  explicit CodableRepresentation(std::vector<Element> elements) noexcept
      : elements_(std::move(elements)) {}

  auto elements() const noexcept -> std::span<const Element> {
    return elements_;
  }

  auto encoded() const -> std::string {
    std::string text;
    for (const Element& element : elements_) {
      text += std::format("{}:{}{}:{}", element.tag.size(), element.tag,
                          element.payload.size(), element.payload);
    }
    return text;
  }

  static auto decoded(std::string_view text)
      -> std::optional<CodableRepresentation> {
    std::vector<Element> found;
    while (!text.empty()) {
      std::optional<std::string> tag = field(text);
      if (!tag.has_value()) {
        return std::nullopt;
      }
      std::optional<std::string> payload = field(text);
      if (!payload.has_value()) {
        return std::nullopt;
      }
      found.push_back(
          Element{.tag = std::move(*tag), .payload = std::move(*payload)});
    }
    return CodableRepresentation{std::move(found)};
  }

  friend auto operator==(const CodableRepresentation&,
                         const CodableRepresentation&) -> bool = default;

 private:
  static auto field(std::string_view& text) -> std::optional<std::string> {
    std::size_t length = 0;
    const auto [end, failure] =
        std::from_chars(text.data(), text.data() + text.size(), length);
    if (failure != std::errc{} || end == text.data() + text.size() ||
        *end != ':') {
      return std::nullopt;
    }
    const std::size_t start = static_cast<std::size_t>(end - text.data()) + 1;
    if (text.size() - start < length) {
      return std::nullopt;
    }
    std::string value{text.substr(start, length)};
    text.remove_prefix(start + length);
    return value;
  }

  std::vector<Element> elements_;
};

inline NavigationPath::NavigationPath(const CodableRepresentation& codable) {
  for (const CodableRepresentation::Element& element : codable.elements()) {
    entries_.push_back(Entry::pending(element.tag, element.payload));
  }
}

inline auto NavigationPath::codable() const
    -> std::optional<CodableRepresentation> {
  std::vector<CodableRepresentation::Element> elements;
  elements.reserve(entries_.size());
  for (const Entry& entry : entries_) {
    std::optional<std::string> payload = entry.encoded();
    if (!payload.has_value() || entry.tag_name().empty()) {
      return std::nullopt;
    }
    elements.push_back(CodableRepresentation::Element{
        .tag = std::string{entry.tag_name()}, .payload = std::move(*payload)});
  }
  return CodableRepresentation{std::move(elements)};
}

}
