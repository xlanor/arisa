#pragma once

#include <arisa/provider/diagnostics/backtrace.hpp>
#include <arisa/provider/diagnostics/crash_reporter_options.hpp>
#include <arisa/provider/diagnostics/crash_snapshot.hpp>
#include <arisa/provider/diagnostics/exception_dump.hpp>
#include <arisa/provider/diagnostics/loaded_modules.hpp>
#include <arisa/provider/diagnostics/memory_space.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace arisa::provider {

enum class CrashDrill : std::uint8_t {
  null_read,
  trap,
  abort,
  terminate,
  failed_assert,
  stack_overflow,
  diag_abort,
};

enum class ExceptionDelivery : std::uint8_t {
  unsupported,
  delivered,
};

enum class CrashReporterError : std::uint8_t {
  already_installed,
  path_too_long,
  directory_unavailable,
};

class CrashReporter {
 public:
  static constexpr std::uint32_t result_module = 430;
  static constexpr std::uint32_t crash_result = result_module | (1U << 9U);
  static constexpr std::uint16_t delivery_probe = 0xA15A;
  static constexpr std::size_t report_capacity = 0x4000;
  static constexpr std::size_t path_capacity = 0x301;
  static constexpr std::size_t device_capacity = 32;
  static constexpr std::size_t app_capacity = 64;
  static constexpr std::size_t message_capacity = 384;
  static constexpr std::array drills{
      CrashDrill::null_read,     CrashDrill::trap,
      CrashDrill::abort,         CrashDrill::terminate,
      CrashDrill::failed_assert, CrashDrill::stack_overflow,
      CrashDrill::diag_abort};

  struct Caller {
    std::uint64_t pc{0};
    std::uint64_t frame{0};
  };

  [[nodiscard]] static auto install(
      const CrashReporterOptions& options) noexcept
      -> std::expected<CrashReporter, CrashReporterError>;

  CrashReporter(const CrashReporter&) = delete;
  auto operator=(const CrashReporter&) -> CrashReporter& = delete;
  CrashReporter(CrashReporter&& other) noexcept;
  auto operator=(CrashReporter&&) -> CrashReporter& = delete;
  ~CrashReporter();

  static auto armed() noexcept -> bool;

  static auto exception_delivery() noexcept -> ExceptionDelivery;

  [[noreturn]] static void drill(CrashDrill drill);

  static void on_cpu_exception(const ExceptionDump& dump) noexcept;
  [[noreturn]] static void on_abort(Caller caller) noexcept;
  [[noreturn]] static void on_trap(Caller caller) noexcept;
  [[noreturn]] static void on_failed_assert(Caller caller, const char* file,
                                            int line, const char* function,
                                            const char* expression) noexcept;
  [[noreturn]] static void on_diag_abort(Caller caller,
                                         std::uint32_t result) noexcept;
  [[noreturn]] static void on_fatal(Caller caller,
                                    std::uint32_t result) noexcept;
  static void on_fatal_with_policy(Caller caller, std::uint32_t result,
                                   std::uint32_t policy) noexcept;

 private:
  class ProcessMemory;
  struct FileRelease;

  CrashReporter() noexcept = default;

  [[noreturn]] static void terminating();
  static auto descend(std::uint32_t depth) -> std::uint32_t;
  static void record(CrashClass crash_class, std::string_view message,
                     std::optional<std::uint32_t> result,
                     Caller caller) noexcept;
  static void report(CrashSnapshot snapshot, std::optional<std::uint64_t> pc,
                     std::uint64_t fp) noexcept;
  static void publish(const CrashSnapshot& snapshot, const Backtrace& backtrace,
                      const MemorySpace& memory) noexcept;
  static void notify_system(const CrashSnapshot& snapshot,
                            const Backtrace& backtrace) noexcept;
  static auto prepare() noexcept -> bool;
  static auto now() noexcept -> std::optional<std::int64_t>;
  static auto main_base() noexcept -> std::uint64_t;
  static auto copy(std::string_view from, std::span<char> into) noexcept
      -> bool;
  static auto view(std::span<const char> from) noexcept -> std::string_view;
  static auto assertion(const char* file, int line, const char* function,
                        const char* expression) noexcept -> std::string_view;

  bool owner_{true};

  static std::atomic<bool> armed_;
  static std::atomic_flag reporting_;
  static std::terminate_handler previous_;
  static std::int64_t installed_time_;
  static std::uint64_t installed_tick_;
  static std::array<char, device_capacity> device_;
  static std::array<char, path_capacity> directory_;
  static std::array<char, path_capacity> path_;
  static std::array<char, app_capacity> app_;
  static std::array<char, message_capacity> message_;
  static std::array<char, report_capacity> text_;
  static LoadedModules modules_;
};

}
