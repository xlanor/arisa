#include <arisa/provider/diagnostics/crash_report.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>

namespace arisa::provider {

CrashReport::CrashReport(std::span<char> storage) noexcept
    : storage_(storage) {}

auto CrashReport::compose(const CrashSnapshot& snapshot,
                          const LoadedModules& modules,
                          const Backtrace& backtrace,
                          const MemorySpace& memory) noexcept
    -> std::string_view {
  used_ = 0;
  line(title);
  identity(snapshot, modules);
  text("\n");
  line("Crash Info:");
  exception(snapshot);
  registers(snapshot, modules);
  trace(backtrace, modules);
  dump(snapshot, memory);
  text("\n");
  listing(modules);
  return {storage_.data(), used_};
}

auto CrashReport::class_name(CrashClass crash_class) noexcept
    -> std::string_view {
  switch (crash_class) {
    case CrashClass::cpu_exception:
      return "CPU Exception";
    case CrashClass::abort:
      return "Abort";
    case CrashClass::failed_assert:
      return "Failed Assert";
    case CrashClass::terminate:
      return "Terminate";
    case CrashClass::diag_abort:
      return "Diag Abort";
    case CrashClass::fatal:
      return "Fatal";
    case CrashClass::trap:
      return "Trap";
  }
  return "Unknown";
}

auto CrashReport::exception_name(std::uint32_t type) noexcept
    -> std::string_view {
  switch (type) {
    case 0x100:
      return "Instruction Abort";
    case 0x101:
      return "Data Abort";
    case 0x102:
      return "Unaligned Instruction";
    case 0x103:
      return "Unaligned Data";
    case 0x104:
      return "Undefined Instruction";
    case 0x105:
      return "Exception Instruction";
    case 0x106:
      return "Memory System Error";
    case 0x107:
      return "FPU Exception";
    case 0x108:
      return "Invalid System Call";
    case 0x109:
      return "System Call Break";
    case 0x301:
      return "Bad SVC";
    default:
      return "Unknown";
  }
}

auto CrashReport::syndrome_name(std::uint32_t esr) noexcept
    -> std::string_view {
  switch (esr >> 26U) {
    case 0x00:
      return "Unknown Reason";
    case 0x0E:
      return "Illegal Execution State";
    case 0x15:
      return "SVC";
    case 0x18:
      return "System Register Trap";
    case 0x20:
      return "Instruction Abort";
    case 0x22:
      return "PC Alignment Fault";
    case 0x24:
      return "Data Abort";
    case 0x26:
      return "SP Alignment Fault";
    case 0x2C:
      return "Floating Point Exception";
    case 0x2F:
      return "SError";
    case 0x3C:
      return "BRK Instruction";
    default:
      return "Other";
  }
}

auto CrashReport::stamp(std::int64_t unix_time) noexcept
    -> std::array<char, stamp_length> {
  const std::chrono::sys_seconds instant{std::chrono::seconds{unix_time}};
  const auto day = std::chrono::floor<std::chrono::days>(instant);
  const std::chrono::year_month_day date{day};
  const std::chrono::hh_mm_ss clock{instant - day};
  std::array<char, stamp_length> out{};
  const auto put = [&out](std::size_t at, std::size_t digits,
                          std::uint64_t value) {
    for (std::size_t index = digits; index > 0; --index) {
      out[at + index - 1] = static_cast<char>('0' + value % 10);
      value /= 10;
    }
  };
  put(0, 4, static_cast<std::uint64_t>(static_cast<int>(date.year())));
  out[4] = '-';
  put(5, 2, static_cast<unsigned>(date.month()));
  out[7] = '-';
  put(8, 2, static_cast<unsigned>(date.day()));
  out[10] = '_';
  put(11, 2, static_cast<std::uint64_t>(clock.hours().count()));
  out[13] = '-';
  put(14, 2, static_cast<std::uint64_t>(clock.minutes().count()));
  out[16] = '-';
  put(17, 2, static_cast<std::uint64_t>(clock.seconds().count()));
  return out;
}

auto CrashReport::result_code(std::uint32_t result) noexcept
    -> std::array<char, 9> {
  std::array<char, 9> out{};
  std::uint32_t module = 2000 + (result & 0x1FFU);
  std::uint32_t description = (result >> 9U) & 0x1FFFU;
  for (std::size_t index = 4; index > 0; --index) {
    out[index - 1] = static_cast<char>('0' + module % 10);
    module /= 10;
    out[index + 4] = static_cast<char>('0' + description % 10);
    description /= 10;
  }
  out[4] = '-';
  return out;
}

void CrashReport::identity(const CrashSnapshot& snapshot,
                           const LoadedModules& modules) {
  label("", "App:");
  line(snapshot.app);
  label("", "Build Id:");
  const std::optional<LoadedModule> main = modules.main();
  if (main.has_value() && main->build_id_length > 0) {
    bytes(main->build_id_bytes());
    text("\n");
  } else {
    line("unknown");
  }
  label("", "Time:");
  if (snapshot.unix_time.has_value()) {
    std::array<char, stamp_length> shown = stamp(*snapshot.unix_time);
    shown[10] = ' ';
    shown[13] = ':';
    shown[16] = ':';
    text({shown.data(), shown.size()});
    line(" UTC");
  } else {
    line("unknown");
  }
  label("", "Class:");
  line(class_name(snapshot.crash_class));
  label("", "Thread Id:");
  prefixed(snapshot.thread_id, 0);
  text("\n");
}

void CrashReport::exception(const CrashSnapshot& snapshot) {
  if (snapshot.crash_class == CrashClass::cpu_exception) {
    line("    Exception Info:");
    label("        ", "Type:");
    text(exception_name(snapshot.exception_type));
    text(" (");
    prefixed(snapshot.exception_type, 0);
    line(")");
    label("        ", "Syndrome:");
    line(syndrome_name(snapshot.esr));
    label("        ", "ESR:");
    prefixed(snapshot.esr, 8);
    text("\n");
    label("        ", "FAR:");
    prefixed(snapshot.far, 16);
    text("\n");
    label("        ", "PSTATE:");
    prefixed(snapshot.pstate, 8);
    text("\n");
    label("        ", "AFSR0:");
    prefixed(snapshot.afsr0, 8);
    text("\n");
    label("        ", "AFSR1:");
    prefixed(snapshot.afsr1, 8);
    text("\n");
  }
  if (!snapshot.message.empty()) {
    label("    ", "Message:");
    line(snapshot.message);
  }
  if (snapshot.result.has_value()) {
    label("    ", "Result:");
    prefixed(*snapshot.result, 8);
    text(" (");
    const std::array<char, 9> code = result_code(*snapshot.result);
    text({code.data(), code.size()});
    line(")");
  }
}

void CrashReport::registers(const CrashSnapshot& snapshot,
                            const LoadedModules& modules) {
  line("    General Purpose Registers:");
  if (snapshot.general.has_value()) {
    for (std::size_t index = 0; index < snapshot.general->size(); ++index) {
      std::array<char, 6> name{'X', '[', '0', '0', ']', ':'};
      name[2] = static_cast<char>('0' + index / 10);
      name[3] = static_cast<char>('0' + index % 10);
      label("        ", {name.data(), name.size()});
      address((*snapshot.general)[index], modules);
    }
  }
  label("        ", "FP:");
  address(snapshot.fp, modules);
  label("        ", "LR:");
  address(snapshot.lr, modules);
  label("        ", "SP:");
  address(snapshot.sp, modules);
  label("        ", "PC:");
  address(snapshot.pc, modules);
}

void CrashReport::trace(const Backtrace& backtrace,
                        const LoadedModules& modules) {
  line("    Stack Trace:");
  const auto frames = backtrace.frames();
  for (std::size_t index = 0; index < frames.size(); ++index) {
    std::array<char, 18> name{'R', 'e', 't', 'u', 'r', 'n', 'A', 'd', 'd',
                              'r', 'e', 's', 's', '[', '0', '0', ']', ':'};
    name[14] = static_cast<char>('0' + index / 10);
    name[15] = static_cast<char>('0' + index % 10);
    label("        ", {name.data(), name.size()});
    address(frames[index], modules);
  }
}

void CrashReport::dump(const CrashSnapshot& snapshot,
                       const MemorySpace& memory) {
  static constexpr std::uint64_t row = 16;
  label("    ", "Stack Dump:");
  line("0000000000000000 0000000000000008");
  const std::uint64_t start = snapshot.sp - snapshot.sp % row;
  for (std::uint64_t index = 0; index < dump_rows; ++index) {
    const std::uint64_t at = start + index * row;
    text("        ");
    hex(at, 16);
    text(":");
    for (std::size_t pad = 8 + 16 + 1; pad < label_width; ++pad) {
      text(" ");
    }
    for (std::uint64_t half = 0; half < row; half += sizeof(std::uint64_t)) {
      const std::optional<std::uint64_t> value = memory.word(at + half);
      if (value.has_value()) {
        hex(*value, 16);
      } else {
        text("????????????????");
      }
      text(half == 0 ? " " : "\n");
    }
  }
}

void CrashReport::listing(const LoadedModules& modules) {
  line("Module Info:");
  const auto listed = modules.modules();
  for (std::size_t index = 0; index < listed.size(); ++index) {
    const LoadedModule& module = listed[index];
    text("    Module ");
    decimal(index, 2);
    line(":");
    label("        ", "Address:");
    hex(module.base, 16);
    text("-");
    hex(module.base + module.size, 16);
    text("\n");
    label("        ", "Name:");
    line(module.label());
    label("        ", "Build Id:");
    if (module.build_id_length > 0) {
      bytes(module.build_id_bytes());
      text("\n");
    } else {
      line("unknown");
    }
    if (module.main) {
      label("        ", "Main:");
      line("yes");
    }
  }
}

void CrashReport::text(std::string_view part) noexcept {
  const std::size_t room = storage_.size() - used_;
  const std::size_t taken = std::min(room, part.size());
  std::ranges::copy(part.substr(0, taken),
                    storage_.begin() + static_cast<std::ptrdiff_t>(used_));
  used_ += taken;
}

void CrashReport::line(std::string_view part) noexcept {
  text(part);
  text("\n");
}

void CrashReport::label(std::string_view indent,
                        std::string_view name) noexcept {
  text(indent);
  text(name);
  const std::size_t written = indent.size() + name.size();
  for (std::size_t pad = written; pad < label_width; ++pad) {
    text(" ");
  }
  if (written >= label_width) {
    text(" ");
  }
}

void CrashReport::hex(std::uint64_t value, std::size_t digits) noexcept {
  std::array<char, 16> raw{};
  const auto written =
      std::to_chars(raw.data(), raw.data() + raw.size(), value, 16);
  const auto length = static_cast<std::size_t>(written.ptr - raw.data());
  for (std::size_t pad = length; pad < digits; ++pad) {
    text("0");
  }
  text({raw.data(), length});
}

void CrashReport::prefixed(std::uint64_t value, std::size_t digits) noexcept {
  text("0x");
  hex(value, digits);
}

void CrashReport::decimal(std::uint64_t value, std::size_t digits) noexcept {
  std::array<char, 20> raw{};
  const auto written =
      std::to_chars(raw.data(), raw.data() + raw.size(), value);
  const auto length = static_cast<std::size_t>(written.ptr - raw.data());
  for (std::size_t pad = length; pad < digits; ++pad) {
    text("0");
  }
  text({raw.data(), length});
}

void CrashReport::address(std::uint64_t value,
                          const LoadedModules& modules) noexcept {
  hex(value, 16);
  const std::optional<LoadedModule> module = modules.find(value);
  if (module.has_value()) {
    text(" (");
    text(module->label());
    text(" + 0x");
    hex(value - module->base, 0);
    text(")");
  }
  text("\n");
}

void CrashReport::bytes(std::span<const std::uint8_t> values) noexcept {
  for (const std::uint8_t value : values) {
    hex(value, 2);
  }
}

}
