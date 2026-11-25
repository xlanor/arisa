#include <arisa/components/open_url_action.hpp>

#include <utility>

namespace arisa::components {

OpenURLAction::OpenURLAction(Handler handler)
    : handler_(std::make_shared<const Handler>(std::move(handler))) {}

auto OpenURLAction::system(provider::BrowserProvider& browser)
    -> OpenURLAction {
  return OpenURLAction{[&browser](const provider::Url& url) {
    return browser.open(url).has_value() ? Result::handled : Result::discarded;
  }};
}

auto OpenURLAction::merged(const OpenURLAction& inherited,
                           const OpenURLAction& declared) -> OpenURLAction {
  OpenURLAction combined = declared;
  if (inherited.system_ != nullptr) {
    combined.system_ = inherited.system_;
  } else if (inherited.handler_ != nullptr) {
    combined.system_ = std::make_shared<const OpenURLAction>(inherited);
  }
  return combined;
}

auto OpenURLAction::operator()(const provider::Url& url) const -> bool {
  if (handler_ == nullptr || !*handler_) {
    return system_ != nullptr && (*system_)(url);
  }
  const Result result = (*handler_)(url);
  switch (result.kind()) {
    case Result::Kind::handled:
      return true;
    case Result::Kind::discarded:
      return false;
    case Result::Kind::system_action:
      return system_ != nullptr && (*system_)(result.url().value_or(url));
  }
  return false;
}

auto OpenURLAction::operator()(std::string_view url) const -> bool {
  const auto parsed = provider::Url::create(url);
  return parsed.has_value() && (*this)(*parsed);
}

}
