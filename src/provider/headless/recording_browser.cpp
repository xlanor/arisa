#include <arisa/provider/headless/recording_browser.hpp>

namespace arisa::provider::headless {

auto RecordingBrowser::open(const Url& url)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  opened_.push_back(url);
  return {};
}

auto RecordingBrowser::opened() const -> std::vector<Url> {
  const std::scoped_lock lock{mutex_};
  return opened_;
}

}
