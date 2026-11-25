#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::composition {

class UTType {
 public:
  explicit UTType(std::string identifier) noexcept
      : identifier_(std::move(identifier)) {}

  static auto plain_text() -> UTType { return UTType{"public.plain-text"}; }

  auto identifier() const noexcept -> std::string_view { return identifier_; }

  static auto exported(std::string_view filename_extension) -> UTType {
    return UTType{std::string{extension_prefix} +
                  std::string{filename_extension}};
  }

  auto preferred_filename_extension() const -> std::optional<std::string> {
    if (identifier_.starts_with(extension_prefix)) {
      return identifier_.substr(extension_prefix.size());
    }
    for (const auto& [identifier, extension] : extensions) {
      if (identifier == identifier_) {
        return std::string{extension};
      }
    }
    return std::nullopt;
  }

  auto mime_type() const -> std::string {
    for (const auto& [identifier, mime] : known) {
      if (identifier == identifier_) {
        return std::string{mime};
      }
    }
    return std::string{private_prefix} + identifier_;
  }

  static auto from_mime_type(std::string_view mime) -> std::optional<UTType> {
    if (mime.starts_with(private_prefix)) {
      return UTType{std::string{mime.substr(private_prefix.size())}};
    }
    for (const auto& [identifier, type] : known) {
      if (type == mime) {
        return UTType{std::string{identifier}};
      }
    }
    if (mime == "text/plain" || mime == "UTF8_STRING" || mime == "TEXT") {
      return plain_text();
    }
    return std::nullopt;
  }

  friend auto operator==(const UTType&, const UTType&) -> bool = default;

 private:
  static constexpr std::string_view private_prefix = "application/x-uttype.";
  static constexpr std::string_view extension_prefix =
      "public.filename-extension.";

  static constexpr std::array<std::pair<std::string_view, std::string_view>, 7>
      extensions{{{"public.plain-text", "txt"},
                  {"public.utf8-plain-text", "txt"},
                  {"public.text", "txt"},
                  {"public.json", "json"},
                  {"public.html", "html"},
                  {"public.png", "png"},
                  {"public.jpeg", "jpg"}}};

  static constexpr std::array<std::pair<std::string_view, std::string_view>, 6>
      known{{{"public.plain-text", "text/plain;charset=utf-8"},
             {"public.utf8-plain-text", "text/plain;charset=utf-8"},
             {"public.html", "text/html"},
             {"public.url", "text/uri-list"},
             {"public.png", "image/png"},
             {"public.jpeg", "image/jpeg"}}};

  std::string identifier_;
};

}
