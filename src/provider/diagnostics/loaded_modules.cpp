#include <arisa/provider/diagnostics/loaded_modules.hpp>

#include <algorithm>
#include <string_view>

namespace arisa::provider {

auto LoadedModules::discover(const MemorySpace& memory,
                             std::uint64_t main_base) noexcept
    -> LoadedModules {
  LoadedModules found;
  std::uint64_t address = 0;
  while (found.count_ < capacity) {
    const std::optional<MemoryRegion> region = memory.region(address);
    if (!region.has_value() || region->size == 0) {
      break;
    }
    std::uint64_t next = region->end();
    if (region->module_image && region->executable) {
      const LoadedModule module = image(memory, *region, main_base);
      found.modules_[found.count_++] = module;
      next = module.base + module.size;
    }
    if (next <= address) {
      break;
    }
    address = next;
  }
  return found;
}

auto LoadedModules::modules() const noexcept -> std::span<const LoadedModule> {
  return std::span{modules_}.first(count_);
}

auto LoadedModules::find(std::uint64_t address) const noexcept
    -> std::optional<LoadedModule> {
  const auto listed = modules();
  const auto hit = std::ranges::find_if(
      listed,
      [address](const LoadedModule& each) { return each.contains(address); });
  if (hit == listed.end()) {
    return std::nullopt;
  }
  return *hit;
}

auto LoadedModules::main() const noexcept -> std::optional<LoadedModule> {
  const auto listed = modules();
  const auto hit = std::ranges::find_if(
      listed, [](const LoadedModule& each) { return each.main; });
  if (hit == listed.end()) {
    return std::nullopt;
  }
  return *hit;
}

auto LoadedModules::image(const MemorySpace& memory, const MemoryRegion& text,
                          std::uint64_t main_base) noexcept -> LoadedModule {
  LoadedModule module{.base = text.base,
                      .size = text.size,
                      .name = {},
                      .name_length = 0,
                      .build_id = {},
                      .build_id_length = 0,
                      .main = text.base == main_base};
  std::optional<MemoryRegion> rodata;
  std::uint64_t next = text.end();
  while (true) {
    const std::optional<MemoryRegion> part = memory.region(next);
    if (!part.has_value() || !part->module_image || part->executable ||
        part->size == 0 || part->end() <= next) {
      break;
    }
    if (!rodata.has_value() && part->readable && !part->writable) {
      rodata = part;
    }
    next = part->end();
  }
  module.size = next - text.base;
  if (rodata.has_value()) {
    name(memory, *rodata, module);
    build_id(memory, *rodata, module);
  }
  if (module.name_length == 0) {
    fallback_name(module);
  }
  return module;
}

void LoadedModules::name(const MemorySpace& memory, const MemoryRegion& rodata,
                         LoadedModule& module) noexcept {
  static constexpr std::uint64_t longest = 0x200;
  const std::optional<std::uint64_t> header = memory.word(rodata.base);
  if (!header.has_value() || (*header & 0xFFFF'FFFFU) != 0) {
    return;
  }
  const std::uint64_t length = *header >> 32U;
  if (length == 0 || length > longest) {
    return;
  }
  const std::uint64_t text = rodata.base + sizeof(std::uint64_t);
  std::uint64_t start = 0;
  std::uint64_t end = 0;
  for (; end < length; ++end) {
    const std::optional<std::uint8_t> each = memory.byte(text + end);
    if (!each.has_value() || *each == 0) {
      break;
    }
    if (*each == '/' || *each == '\\') {
      start = end + 1;
    }
  }
  for (std::uint64_t at = start;
       at < end && module.name_length < LoadedModule::name_capacity; ++at) {
    module.name[module.name_length++] =
        static_cast<char>(memory.byte(text + at).value_or('?'));
  }
}

void LoadedModules::build_id(const MemorySpace& memory,
                             const MemoryRegion& rodata,
                             LoadedModule& module) noexcept {
  static constexpr std::uint64_t header = 16;
  if (rodata.size < header) {
    return;
  }
  const std::uint64_t floor = rodata.size > build_id_search
                                  ? rodata.end() - build_id_search
                                  : rodata.base;
  for (std::uint64_t at = rodata.end() - header; at >= floor; at -= 4) {
    const std::optional<std::uint32_t> name_size = memory.half(at);
    const std::optional<std::uint32_t> length = memory.half(at + 4);
    const std::optional<std::uint32_t> type = memory.half(at + 8);
    const std::optional<std::uint32_t> owner = memory.half(at + 12);
    if (name_size == 4U && type == build_id_note && owner == gnu_name &&
        length.has_value() && *length > 0 &&
        *length <= LoadedModule::build_id_capacity &&
        at + header + *length <= rodata.end()) {
      for (std::uint32_t index = 0; index < *length; ++index) {
        module.build_id[index] =
            memory.byte(at + header + index).value_or(std::uint8_t{0});
      }
      module.build_id_length = *length;
      return;
    }
    if (at < floor + 4) {
      return;
    }
  }
}

void LoadedModules::fallback_name(LoadedModule& module) noexcept {
  const std::string_view fallback = module.main ? "main" : "module";
  std::ranges::copy(fallback, module.name.begin());
  module.name_length = fallback.size();
}

}
