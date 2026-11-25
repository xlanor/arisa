#pragma once

#include <switch.h>

#include <string>

namespace arisa::nx {

class SwitchConsole {
 public:
  static void show(const std::string& failure);

 private:
  struct Release {
    using handle_type = PrintConsole*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type console) noexcept -> bool {
      return console != nullptr;
    }

    static void release(handle_type& console) noexcept { consoleExit(console); }
  };
};

}
