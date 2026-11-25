#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::provider {

enum class CrashClass : std::uint8_t {
  cpu_exception,
  abort,
  failed_assert,
  terminate,
  diag_abort,
  fatal,
  trap,
};

struct CrashSnapshot {
  static constexpr std::size_t general_registers = 29;

  std::string_view app{};
  std::optional<std::int64_t> unix_time{};
  CrashClass crash_class{CrashClass::abort};
  std::uint32_t exception_type{0};
  std::optional<std::uint32_t> result{};
  std::string_view message{};
  std::uint64_t thread_id{0};
  std::optional<std::array<std::uint64_t, general_registers>> general{};
  std::uint64_t fp{0};
  std::uint64_t lr{0};
  std::uint64_t sp{0};
  std::uint64_t pc{0};
  std::uint32_t pstate{0};
  std::uint32_t afsr0{0};
  std::uint32_t afsr1{0};
  std::uint32_t esr{0};
  std::uint64_t far{0};
};

}
