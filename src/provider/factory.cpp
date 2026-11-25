#include <arisa/provider/applets/absent.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/speech/absent.hpp>

namespace arisa::provider {

auto ProviderFactory::supplies(Capability capability) const -> bool {
  return contains(info().capabilities, capability);
}

auto ProviderFactory::applets() noexcept -> AppletProvider& {
  static AbsentAppletProvider absent;
  return absent;
}

auto ProviderFactory::speech() noexcept -> SpeechProvider& {
  static AbsentSpeechProvider absent;
  return absent;
}

}
