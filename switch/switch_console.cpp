#include <switch_console.hpp>

#include <arisa/support/unique_handle.hpp>

#include <print>

namespace arisa::nx {

void SwitchConsole::show(const std::string& failure) {
  const support::UniqueHandle<Release> console{consoleInit(nullptr)};
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad{};
  padInitializeDefault(&pad);
  std::println("{}", failure);
  std::println("press + to exit");
  while (appletMainLoop()) {
    padUpdate(&pad);
    if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0) {
      break;
    }
    consoleUpdate(console.get());
  }
}

}
