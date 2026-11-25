#include <arisa/provider/diagnostics/backtrace.hpp>

namespace arisa::provider {

auto Backtrace::walk(const MemorySpace& memory, std::optional<std::uint64_t> pc,
                     std::uint64_t fp) noexcept -> Backtrace {
  Backtrace trace;
  if (pc.has_value()) {
    trace.push(*pc);
  }
  while (trace.count_ < capacity && record(memory, fp)) {
    const std::optional<std::uint64_t> caller = memory.word(fp);
    const std::optional<std::uint64_t> returned =
        memory.word(fp + sizeof(std::uint64_t));
    if (!caller.has_value() || !returned.has_value() || *returned == 0) {
      break;
    }
    const std::optional<MemoryRegion> code = memory.region(*returned);
    if (!code.has_value() || !code->executable) {
      break;
    }
    trace.push(*returned);
    if (*caller <= fp) {
      break;
    }
    fp = *caller;
  }
  return trace;
}

auto Backtrace::frames() const noexcept -> std::span<const std::uint64_t> {
  return std::span{frames_}.first(count_);
}

auto Backtrace::record(const MemorySpace& memory, std::uint64_t fp) noexcept
    -> bool {
  if (fp == 0 || fp % sizeof(std::uint64_t) != 0) {
    return false;
  }
  const std::optional<MemoryRegion> found = memory.region(fp);
  return found.has_value() && found->readable && found->writable &&
         found->holds(fp, 2 * sizeof(std::uint64_t));
}

void Backtrace::push(std::uint64_t address) noexcept {
  if (count_ < capacity) {
    frames_[count_++] = address;
  }
}

}
