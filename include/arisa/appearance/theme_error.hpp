#pragma once

#include <cstdint>
#include <string>

namespace arisa::appearance {

enum class ThemeErrorCode : std::uint8_t {
  not_found,
  syntax,
  unsupported_schema,
  invalid_value,
  unknown_key,
  cyclic_extends,
  io_failure,
};

class ThemeError {
 public:
  ThemeError(ThemeErrorCode code, std::string file, std::uint32_t line,
             std::string key, std::string message)
      : code_(code),
        file_(std::move(file)),
        line_(line),
        key_(std::move(key)),
        message_(std::move(message)) {}

  auto code() const noexcept -> ThemeErrorCode { return code_; }

  auto file() const noexcept -> const std::string& { return file_; }

  auto line() const noexcept -> std::uint32_t { return line_; }

  auto key() const noexcept -> const std::string& { return key_; }

  auto message() const noexcept -> const std::string& { return message_; }

  auto description() const -> std::string;

  friend auto operator==(const ThemeError&, const ThemeError&)
      -> bool = default;

 private:
  ThemeErrorCode code_;
  std::string file_;
  std::uint32_t line_;
  std::string key_;
  std::string message_;
};

}
