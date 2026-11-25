#pragma once

#include <arisa/provider/diagnostics/crash_snapshot.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arisa::provider {

struct alignas(16) ExceptionDump {
  std::uint32_t error_desc{0};
  std::array<std::uint32_t, 3> pad{};
  std::array<std::uint64_t, CrashSnapshot::general_registers> gprs{};
  std::uint64_t fp{0};
  std::uint64_t lr{0};
  std::uint64_t sp{0};
  std::uint64_t pc{0};
  std::uint64_t padding{0};
  std::array<std::array<std::uint64_t, 2>, 32> fpu{};
  std::uint32_t pstate{0};
  std::uint32_t afsr0{0};
  std::uint32_t afsr1{0};
  std::uint32_t esr{0};
  std::uint64_t far{0};

  auto snapshot() const noexcept -> CrashSnapshot {
    return CrashSnapshot{.app = {},
                         .unix_time = {},
                         .crash_class = CrashClass::cpu_exception,
                         .exception_type = error_desc,
                         .result = {},
                         .message = {},
                         .thread_id = 0,
                         .general = gprs,
                         .fp = fp,
                         .lr = lr,
                         .sp = sp,
                         .pc = pc,
                         .pstate = pstate,
                         .afsr0 = afsr0,
                         .afsr1 = afsr1,
                         .esr = esr,
                         .far = far};
  }
};

static_assert(sizeof(ExceptionDump) == 0x340);
static_assert(offsetof(ExceptionDump, gprs) == 0x10);
static_assert(offsetof(ExceptionDump, fp) == 0xF8);
static_assert(offsetof(ExceptionDump, pc) == 0x110);
static_assert(offsetof(ExceptionDump, fpu) == 0x120);
static_assert(offsetof(ExceptionDump, pstate) == 0x320);
static_assert(offsetof(ExceptionDump, far) == 0x330);

}
