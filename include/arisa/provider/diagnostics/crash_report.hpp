#pragma once

#include <arisa/provider/diagnostics/backtrace.hpp>
#include <arisa/provider/diagnostics/crash_snapshot.hpp>
#include <arisa/provider/diagnostics/loaded_modules.hpp>
#include <arisa/provider/diagnostics/memory_space.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace arisa::provider {

class CrashReport {
 public:
  static constexpr std::size_t label_width = 33;
  static constexpr std::size_t dump_rows = 16;
  static constexpr std::size_t stamp_length = 19;
  static constexpr std::string_view title = "arisa Crash Report (v1):";

  explicit CrashReport(std::span<char> storage) noexcept;

  auto compose(const CrashSnapshot& snapshot, const LoadedModules& modules,
               const Backtrace& backtrace, const MemorySpace& memory) noexcept
      -> std::string_view;

  static auto class_name(CrashClass crash_class) noexcept -> std::string_view;

  static auto exception_name(std::uint32_t type) noexcept -> std::string_view;

  static auto syndrome_name(std::uint32_t esr) noexcept -> std::string_view;

  static auto stamp(std::int64_t unix_time) noexcept
      -> std::array<char, stamp_length>;

  static auto result_code(std::uint32_t result) noexcept -> std::array<char, 9>;

 private:
  void identity(const CrashSnapshot& snapshot, const LoadedModules& modules);
  void exception(const CrashSnapshot& snapshot);
  void registers(const CrashSnapshot& snapshot, const LoadedModules& modules);
  void trace(const Backtrace& backtrace, const LoadedModules& modules);
  void dump(const CrashSnapshot& snapshot, const MemorySpace& memory);
  void listing(const LoadedModules& modules);

  void text(std::string_view part) noexcept;
  void line(std::string_view part) noexcept;
  void label(std::string_view indent, std::string_view name) noexcept;
  void hex(std::uint64_t value, std::size_t digits) noexcept;
  void prefixed(std::uint64_t value, std::size_t digits) noexcept;
  void decimal(std::uint64_t value, std::size_t digits) noexcept;
  void address(std::uint64_t value, const LoadedModules& modules) noexcept;
  void bytes(std::span<const std::uint8_t> values) noexcept;

  std::span<char> storage_;
  std::size_t used_{0};
};

}
