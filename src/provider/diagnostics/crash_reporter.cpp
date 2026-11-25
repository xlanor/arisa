#include <arisa/provider/diagnostics/crash_report.hpp>
#include <arisa/provider/diagnostics/crash_reporter.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#if defined(__SWITCH__)
#include <arisa/support/unique_handle.hpp>

#include <assert.h>
#include <switch.h>

extern "C" {
extern char __start__[];
[[noreturn]] void __real_abort();
[[noreturn]] void __real___assert_func(const char* file, int line,
                                       const char* function,
                                       const char* expression);
[[noreturn]] void __real_diagAbortWithResult(Result result);
[[noreturn]] void __real_fatalThrow(Result result);
void __real_fatalThrowWithPolicy(Result result, FatalPolicy policy);

void arisa_crash_cpu_exception(const arisa::provider::ExceptionDump* dump) {
  arisa::provider::CrashReporter::on_cpu_exception(*dump);
}

[[noreturn]] void arisa_trap() {
  arisa::provider::CrashReporter::on_trap(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))});
}

[[noreturn]] void __wrap_abort() {
  arisa::provider::CrashReporter::on_abort(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))});
}

[[noreturn]] void __wrap___assert_func(const char* file, int line,
                                       const char* function,
                                       const char* expression) {
  arisa::provider::CrashReporter::on_failed_assert(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))},
      file, line, function, expression);
}

[[noreturn]] void __wrap_diagAbortWithResult(Result result) {
  arisa::provider::CrashReporter::on_diag_abort(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))},
      result);
}

[[noreturn]] void __wrap_fatalThrow(Result result) {
  arisa::provider::CrashReporter::on_fatal(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))},
      result);
}

void __wrap_fatalThrowWithPolicy(Result result, FatalPolicy policy) {
  arisa::provider::CrashReporter::on_fatal_with_policy(
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))},
      result, static_cast<std::uint32_t>(policy));
}
}

asm(R"(
  .pushsection .text.__libnx_exception_entry, "ax", %progbits
  .global __libnx_exception_entry
  .type __libnx_exception_entry, %function
  .balign 4
__libnx_exception_entry:
  cbz x1, 2f
  ldr w2, [x1, #0x6c]
  lsr w3, w2, #26
  cmp w3, #0x3c
  b.ne 1f
  and w3, w2, #0xffff
  mov w4, #0xa15a
  cmp w3, w4
  b.ne 1f
  ldr x3, [x1, #0x58]
  add x3, x3, #4
  str x3, [x1, #0x58]
  mov x3, #1
  str x3, [x1]
  mov w0, #0
  bl svcReturnFromException
  b .
1:
  adrp x2, arisa_crash_dump
  add x2, x2, :lo12:arisa_crash_dump
  str w0, [x2]
  str wzr, [x2, #0x4]
  str xzr, [x2, #0x8]
  ldp x3, x4, [x1, #0x00]
  stp x3, x4, [x2, #0x10]
  ldp x3, x4, [x1, #0x10]
  stp x3, x4, [x2, #0x20]
  ldp x3, x4, [x1, #0x20]
  stp x3, x4, [x2, #0x30]
  ldp x3, x4, [x1, #0x30]
  stp x3, x4, [x2, #0x40]
  ldr x3, [x1, #0x40]
  str x3, [x2, #0x50]
  str x9, [x2, #0x58]
  stp x10, x11, [x2, #0x60]
  stp x12, x13, [x2, #0x70]
  stp x14, x15, [x2, #0x80]
  stp x16, x17, [x2, #0x90]
  stp x18, x19, [x2, #0xa0]
  stp x20, x21, [x2, #0xb0]
  stp x22, x23, [x2, #0xc0]
  stp x24, x25, [x2, #0xd0]
  stp x26, x27, [x2, #0xe0]
  str x28, [x2, #0xf0]
  str x29, [x2, #0xf8]
  ldr x3, [x1, #0x48]
  str x3, [x2, #0x100]
  ldr x3, [x1, #0x50]
  str x3, [x2, #0x108]
  ldr x3, [x1, #0x58]
  str x3, [x2, #0x110]
  str xzr, [x2, #0x118]
  add x3, x2, #0x120
  stp q0, q1, [x3, #0x000]
  stp q2, q3, [x3, #0x020]
  stp q4, q5, [x3, #0x040]
  stp q6, q7, [x3, #0x060]
  stp q8, q9, [x3, #0x080]
  stp q10, q11, [x3, #0x0a0]
  stp q12, q13, [x3, #0x0c0]
  stp q14, q15, [x3, #0x0e0]
  stp q16, q17, [x3, #0x100]
  stp q18, q19, [x3, #0x120]
  stp q20, q21, [x3, #0x140]
  stp q22, q23, [x3, #0x160]
  stp q24, q25, [x3, #0x180]
  stp q26, q27, [x3, #0x1a0]
  stp q28, q29, [x3, #0x1c0]
  stp q30, q31, [x3, #0x1e0]
  ldr w3, [x1, #0x60]
  str w3, [x2, #0x320]
  ldr w3, [x1, #0x64]
  str w3, [x2, #0x324]
  ldr w3, [x1, #0x68]
  str w3, [x2, #0x328]
  ldr w3, [x1, #0x6c]
  str w3, [x2, #0x32c]
  ldr x3, [x1, #0x70]
  str x3, [x2, #0x330]
  mov x19, sp
  adrp x3, arisa_crash_stack
  add x3, x3, :lo12:arisa_crash_stack
  add x3, x3, #0x10, lsl #12
  mov sp, x3
  mov x0, x2
  bl arisa_crash_cpu_exception
  mov sp, x19
2:
  mov w0, #0xf801
  bl svcReturnFromException
  b .
  .size __libnx_exception_entry, . - __libnx_exception_entry
  .popsection

  .pushsection .bss.arisa_crash_dump, "aw", %nobits
  .balign 16
  .type arisa_crash_dump, %object
arisa_crash_dump:
  .space 0x340
  .size arisa_crash_dump, 0x340
  .popsection

  .pushsection .bss.arisa_crash_stack, "aw", %nobits
  .balign 16
  .type arisa_crash_stack, %object
arisa_crash_stack:
  .space 0x10000
  .size arisa_crash_stack, 0x10000
  .popsection
)");

namespace arisa::provider {

static_assert(sizeof(ExceptionDump) == sizeof(ThreadExceptionDump));
static_assert(offsetof(ExceptionDump, gprs) ==
              offsetof(ThreadExceptionDump, cpu_gprs));
static_assert(offsetof(ExceptionDump, fp) == offsetof(ThreadExceptionDump, fp));
static_assert(offsetof(ExceptionDump, pc) == offsetof(ThreadExceptionDump, pc));
static_assert(offsetof(ExceptionDump, fpu) ==
              offsetof(ThreadExceptionDump, fpu_gprs));
static_assert(offsetof(ExceptionDump, pstate) ==
              offsetof(ThreadExceptionDump, pstate));
static_assert(offsetof(ExceptionDump, esr) ==
              offsetof(ThreadExceptionDump, esr));
static_assert(offsetof(ExceptionDump, far) ==
              offsetof(ThreadExceptionDump, far));
static_assert(offsetof(ThreadExceptionFrameA64, elr_el1) == 0x58);
static_assert(offsetof(ThreadExceptionFrameA64, esr) == 0x6C);
static_assert(offsetof(ThreadExceptionFrameA64, far) == 0x70);

struct CrashReporter::FileRelease {
  using handle_type = FsFile;
  static auto empty() noexcept -> handle_type { return FsFile{}; }
  static auto is_valid(const handle_type& file) noexcept -> bool {
    return file.s.session != INVALID_HANDLE;
  }
  static void release(handle_type& file) noexcept { fsFileClose(&file); }
};

class CrashReporter::ProcessMemory final : public MemorySpace {
 public:
  auto region(std::uint64_t address) const noexcept
      -> std::optional<MemoryRegion> override {
    MemoryInfo info{};
    std::uint32_t page = 0;
    if (R_FAILED(svcQueryMemory(&info, &page, address))) {
      return std::nullopt;
    }
    const std::uint32_t type = info.type & 0xFFU;
    return MemoryRegion{.base = info.addr,
                        .size = info.size,
                        .readable = (info.perm & Perm_R) != 0,
                        .writable = (info.perm & Perm_W) != 0,
                        .executable = (info.perm & Perm_X) != 0,
                        .module_image = type == MemType_CodeStatic ||
                                        type == MemType_CodeMutable ||
                                        type == MemType_ModuleCodeStatic ||
                                        type == MemType_ModuleCodeMutable};
  }

 private:
  auto fetch(std::uint64_t address) const noexcept -> std::uint64_t override {
    std::uint64_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
  }
};

}
#endif

namespace arisa::provider {

std::atomic<bool> CrashReporter::armed_{false};
std::atomic_flag CrashReporter::reporting_{};
std::terminate_handler CrashReporter::previous_{nullptr};
std::int64_t CrashReporter::installed_time_{0};
std::uint64_t CrashReporter::installed_tick_{0};
std::array<char, CrashReporter::device_capacity> CrashReporter::device_{};
std::array<char, CrashReporter::path_capacity> CrashReporter::directory_{};
std::array<char, CrashReporter::path_capacity> CrashReporter::path_{};
std::array<char, CrashReporter::app_capacity> CrashReporter::app_{};
std::array<char, CrashReporter::message_capacity> CrashReporter::message_{};
std::array<char, CrashReporter::report_capacity> CrashReporter::text_{};
LoadedModules CrashReporter::modules_{};

auto CrashReporter::install(const CrashReporterOptions& options) noexcept
    -> std::expected<CrashReporter, CrashReporterError> {
#if defined(__SWITCH__)
  if (armed_.load(std::memory_order_acquire)) {
    return std::unexpected{CrashReporterError::already_installed};
  }
  const std::size_t colon = options.directory.find(":/");
  if (colon == std::string_view::npos) {
    return std::unexpected{CrashReporterError::directory_unavailable};
  }
  std::string_view directory = options.directory.substr(colon + 1);
  while (directory.size() > 1 && directory.back() == '/') {
    directory.remove_suffix(1);
  }
  if (!copy(options.directory.substr(0, colon), device_) ||
      !copy(directory, directory_) || !copy(options.app, app_) ||
      directory.size() + options.app.size() + 32 >= path_capacity) {
    return std::unexpected{CrashReporterError::path_too_long};
  }
  if (!prepare()) {
    return std::unexpected{CrashReporterError::directory_unavailable};
  }
  installed_time_ = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  installed_tick_ = armGetSystemTick();
  previous_ = std::set_terminate(&CrashReporter::terminating);
  armed_.store(true, std::memory_order_release);
#else
  static_cast<void>(options);
#endif
  return CrashReporter{};
}

CrashReporter::CrashReporter(CrashReporter&& other) noexcept
    : owner_(std::exchange(other.owner_, false)) {}

CrashReporter::~CrashReporter() {
#if defined(__SWITCH__)
  if (owner_ && armed_.exchange(false, std::memory_order_acq_rel)) {
    std::set_terminate(previous_);
  }
#endif
}

auto CrashReporter::armed() noexcept -> bool {
  return armed_.load(std::memory_order_acquire);
}

auto CrashReporter::exception_delivery() noexcept -> ExceptionDelivery {
#if defined(__SWITCH__)
  std::uint64_t answer = 0;
  asm volatile(
      "mov x0, #0\n\t"
      "brk #0xa15a\n\t"
      "mov %0, x0"
      : "=r"(answer)
      :
      : "x0", "memory");
  return answer == 1 ? ExceptionDelivery::delivered
                     : ExceptionDelivery::unsupported;
#else
  return ExceptionDelivery::unsupported;
#endif
}

void CrashReporter::drill(CrashDrill drill) {
  switch (drill) {
    case CrashDrill::null_read: {
      volatile std::uintptr_t address = 0;
      static_cast<void>(*reinterpret_cast<volatile std::uint32_t*>(address));
      break;
    }
    case CrashDrill::trap:
      __builtin_trap();
    case CrashDrill::abort:
      std::abort();
    case CrashDrill::terminate:
      std::terminate();
    case CrashDrill::failed_assert:
#if defined(__SWITCH__)
      __assert_func(__FILE__, __LINE__, __func__, "drill == failed_assert");
#else
      std::abort();
#endif
    case CrashDrill::stack_overflow:
      static_cast<void>(descend(0));
      break;
    case CrashDrill::diag_abort:
#if defined(__SWITCH__)
      diagAbortWithResult(crash_result);
#else
      std::abort();
#endif
  }
  std::abort();
}

auto CrashReporter::descend(std::uint32_t depth) -> std::uint32_t {
  static constexpr std::size_t frame = 1024;
  std::array<std::uint8_t, frame> pad{};
  pad[depth % frame] = static_cast<std::uint8_t>(depth);
  asm volatile("" : : "r"(pad.data()) : "memory");
  if (depth == std::numeric_limits<std::uint32_t>::max()) {
    return pad[0];
  }
  return descend(depth + 1) + pad[(depth + 1) % frame];
}

void CrashReporter::on_cpu_exception(const ExceptionDump& dump) noexcept {
  report(dump.snapshot(), dump.pc, dump.fp);
}

void CrashReporter::on_abort(Caller caller) noexcept {
  record(CrashClass::abort, "abort() called", std::nullopt, caller);
#if defined(__SWITCH__)
  __real_abort();
#else
  std::abort();
#endif
}

void CrashReporter::on_trap(Caller caller) noexcept {
  record(CrashClass::trap, "__builtin_trap reached", std::nullopt, caller);
#if defined(__SWITCH__)
  asm volatile("brk #0x3e8");
#endif
  std::abort();
}

void CrashReporter::on_failed_assert(Caller caller, const char* file, int line,
                                     const char* function,
                                     const char* expression) noexcept {
  record(CrashClass::failed_assert, assertion(file, line, function, expression),
         std::nullopt, caller);
#if defined(__SWITCH__)
  __real___assert_func(file, line, function, expression);
#else
  std::abort();
#endif
}

void CrashReporter::on_diag_abort(Caller caller,
                                  std::uint32_t result) noexcept {
  record(CrashClass::diag_abort, "diagAbortWithResult called", result, caller);
#if defined(__SWITCH__)
  __real_diagAbortWithResult(result);
#else
  std::abort();
#endif
}

void CrashReporter::on_fatal(Caller caller, std::uint32_t result) noexcept {
  record(CrashClass::fatal, "fatalThrow called", result, caller);
#if defined(__SWITCH__)
  __real_fatalThrow(result);
#else
  std::abort();
#endif
}

void CrashReporter::on_fatal_with_policy(Caller caller, std::uint32_t result,
                                         std::uint32_t policy) noexcept {
#if defined(__SWITCH__)
  if (policy != FatalPolicy_ErrorReport) {
    record(CrashClass::fatal, "fatalThrowWithPolicy called", result, caller);
  }
  __real_fatalThrowWithPolicy(result, static_cast<FatalPolicy>(policy));
#else
  static_cast<void>(caller);
  static_cast<void>(result);
  static_cast<void>(policy);
#endif
}

void CrashReporter::terminating() {
  record(
      CrashClass::terminate, "std::terminate called", std::nullopt,
      {.pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
       .frame = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0))});
#if defined(__SWITCH__)
  __real_abort();
#else
  std::abort();
#endif
}

void CrashReporter::record(CrashClass crash_class, std::string_view message,
                           std::optional<std::uint32_t> result,
                           Caller caller) noexcept {
  report(CrashSnapshot{.app = {},
                       .unix_time = {},
                       .crash_class = crash_class,
                       .exception_type = 0,
                       .result = result,
                       .message = message,
                       .thread_id = 0,
                       .general = {},
                       .fp = caller.frame,
                       .lr = caller.pc,
                       .sp = caller.frame,
                       .pc = caller.pc,
                       .pstate = 0,
                       .afsr0 = 0,
                       .afsr1 = 0,
                       .esr = 0,
                       .far = 0},
         std::nullopt, caller.frame);
}

void CrashReporter::report(CrashSnapshot snapshot,
                           std::optional<std::uint64_t> pc,
                           std::uint64_t fp) noexcept {
#if defined(__SWITCH__)
  if (!armed_.load(std::memory_order_acquire) ||
      reporting_.test_and_set(std::memory_order_acq_rel)) {
    return;
  }
  snapshot.app = view(app_);
  snapshot.unix_time = now();
  std::uint64_t thread = 0;
  if (R_SUCCEEDED(svcGetThreadId(&thread, CUR_THREAD_HANDLE))) {
    snapshot.thread_id = thread;
  }
  const ProcessMemory memory;
  modules_ = LoadedModules::discover(memory, main_base());
  const Backtrace backtrace = Backtrace::walk(memory, pc, fp);
  publish(snapshot, backtrace, memory);
  notify_system(snapshot, backtrace);
#else
  static_cast<void>(snapshot);
  static_cast<void>(pc);
  static_cast<void>(fp);
#endif
}

void CrashReporter::publish(const CrashSnapshot& snapshot,
                            const Backtrace& backtrace,
                            const MemorySpace& memory) noexcept {
#if defined(__SWITCH__)
  FsFileSystem* const device = fsdevGetDeviceFileSystem(device_.data());
  if (device == nullptr) {
    return;
  }
  std::size_t used = 0;
  const auto append = [&used](std::string_view part) {
    const std::size_t taken = std::min(part.size(), path_capacity - 1 - used);
    std::ranges::copy(part.substr(0, taken),
                      path_.begin() + static_cast<std::ptrdiff_t>(used));
    used += taken;
    path_[used] = '\0';
  };
  append(view(directory_));
  append("/");
  if (snapshot.unix_time.has_value()) {
    const std::array<char, CrashReport::stamp_length> stamp =
        CrashReport::stamp(*snapshot.unix_time);
    append({stamp.data(), stamp.size()});
  } else {
    std::array<char, 20> tick{};
    const auto written = std::to_chars(tick.data(), tick.data() + tick.size(),
                                       armGetSystemTick());
    append({tick.data(), static_cast<std::size_t>(written.ptr - tick.data())});
  }
  append("_");
  append(view(app_));
  append(".log");
  static_cast<void>(fsFsCreateFile(device, path_.data(), 0, 0));
  FsFile opened{};
  if (R_FAILED(fsFsOpenFile(device, path_.data(),
                            FsOpenMode_Write | FsOpenMode_Append, &opened))) {
    return;
  }
  const support::UniqueHandle<FileRelease> file{opened};
  CrashReport composer{text_};
  const std::string_view body =
      composer.compose(snapshot, modules_, backtrace, memory);
  FsFile target = file.get();
  static_cast<void>(
      fsFileWrite(&target, 0, body.data(), body.size(), FsWriteOption_Flush));
  static_cast<void>(fsFileSetSize(&target, static_cast<s64>(body.size())));
  static_cast<void>(fsFileFlush(&target));
#else
  static_cast<void>(snapshot);
  static_cast<void>(backtrace);
  static_cast<void>(memory);
#endif
}

void CrashReporter::notify_system(const CrashSnapshot& snapshot,
                                  const Backtrace& backtrace) noexcept {
#if defined(__SWITCH__)
  static constexpr std::uint64_t fp_bit = 1ULL << 29U;
  static constexpr std::uint64_t lr_bit = 1ULL << 30U;
  static constexpr std::uint64_t sp_bit = 1ULL << 31U;
  static constexpr std::uint64_t pc_bit = 1ULL << 32U;
  FatalCpuContext context{};
  FatalAarch64Context& cpu = context.aarch64_ctx;
  std::uint64_t present = fp_bit | sp_bit | pc_bit;
  if (snapshot.general.has_value()) {
    for (std::size_t index = 0; index < snapshot.general->size(); ++index) {
      cpu.x[index] = (*snapshot.general)[index];
      present |= 1ULL << index;
    }
  }
  if (snapshot.crash_class == CrashClass::cpu_exception || snapshot.lr != 0) {
    present |= lr_bit;
  }
  cpu.fp = snapshot.fp;
  cpu.lr = snapshot.lr;
  cpu.sp = snapshot.sp;
  cpu.pc = snapshot.pc;
  cpu.pstate = snapshot.pstate;
  cpu.afsr0 = snapshot.afsr0;
  cpu.afsr1 = snapshot.afsr1;
  cpu.esr = snapshot.esr;
  cpu.far = snapshot.far;
  const auto frames = backtrace.frames();
  const std::size_t shown = std::min(frames.size(), std::size(cpu.stack_trace));
  std::ranges::copy(frames.first(shown), std::begin(cpu.stack_trace));
  cpu.stack_trace_size = static_cast<u32>(shown);
  cpu.start_address = main_base();
  cpu.register_set_flags = present;
  context.is_aarch32 = false;
  context.type = snapshot.exception_type;
  fatalThrowWithContext(snapshot.result.value_or(crash_result),
                        FatalPolicy_ErrorReport, &context);
#else
  static_cast<void>(snapshot);
  static_cast<void>(backtrace);
#endif
}

auto CrashReporter::prepare() noexcept -> bool {
#if defined(__SWITCH__)
  FsFileSystem* const device = fsdevGetDeviceFileSystem(device_.data());
  if (device == nullptr) {
    return false;
  }
  const std::string_view directory = view(directory_);
  for (std::size_t at = 1; at <= directory.size(); ++at) {
    if (at == directory.size() || directory[at] == '/') {
      if (copy(directory.substr(0, at), path_)) {
        static_cast<void>(fsFsCreateDirectory(device, path_.data()));
      }
    }
  }
  FsDirEntryType type{};
  return copy(directory, path_) &&
         R_SUCCEEDED(fsFsGetEntryType(device, path_.data(), &type)) &&
         type == FsDirEntryType_Dir;
#else
  return true;
#endif
}

auto CrashReporter::now() noexcept -> std::optional<std::int64_t> {
#if defined(__SWITCH__)
  if (installed_time_ == 0) {
    return std::nullopt;
  }
  const std::uint64_t elapsed =
      (armGetSystemTick() - installed_tick_) / armGetSystemTickFreq();
  return installed_time_ + static_cast<std::int64_t>(elapsed);
#else
  return std::nullopt;
#endif
}

auto CrashReporter::main_base() noexcept -> std::uint64_t {
#if defined(__SWITCH__)
  return reinterpret_cast<std::uint64_t>(__start__);
#else
  return 0;
#endif
}

auto CrashReporter::copy(std::string_view from, std::span<char> into) noexcept
    -> bool {
  if (from.size() >= into.size()) {
    return false;
  }
  std::ranges::copy(from, into.begin());
  into[from.size()] = '\0';
  return true;
}

auto CrashReporter::view(std::span<const char> from) noexcept
    -> std::string_view {
  const auto end = std::ranges::find(from, '\0');
  return {from.data(), static_cast<std::size_t>(end - from.begin())};
}

auto CrashReporter::assertion(const char* file, int line, const char* function,
                              const char* expression) noexcept
    -> std::string_view {
  std::size_t used = 0;
  const auto append = [&used](std::string_view part) {
    const std::size_t taken =
        std::min(part.size(), message_capacity - 1 - used);
    std::ranges::copy(part.substr(0, taken),
                      message_.begin() + static_cast<std::ptrdiff_t>(used));
    used += taken;
  };
  append("assertion \"");
  append(expression != nullptr ? expression : "?");
  append("\" failed at ");
  append(file != nullptr ? file : "?");
  append(":");
  std::array<char, 12> number{};
  const auto written =
      std::to_chars(number.data(), number.data() + number.size(), line);
  append(
      {number.data(), static_cast<std::size_t>(written.ptr - number.data())});
  append(" in ");
  append(function != nullptr ? function : "?");
  message_[used] = '\0';
  return {message_.data(), used};
}

}
