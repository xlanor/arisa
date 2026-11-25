#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::presentation {

class Help : public composition::Element {
 public:
  class Content;

  static constexpr std::chrono::milliseconds focus_delay{1000};
  static constexpr std::chrono::milliseconds touch_linger{2000};
  static constexpr float titled_width = 280.0F;

  Help(composition::Element content, std::string text);

  Help(composition::Element content, std::string title, std::string body);

  static auto label(const composition::Element& element)
      -> std::optional<std::reference_wrapper<const composition::Element>>;

  static auto relabel(
      composition::Element help,
      const std::function<composition::Element(composition::Element)>& change)
      -> composition::Element;

 private:
  static auto dwell(composition::StateHandle<bool> dwelled, bool within)
      -> async::Task<void>;

  static auto linger(composition::StateHandle<bool> held) -> async::Task<void>;

  static auto tip(std::optional<std::string> title, std::string text)
      -> composition::Element;
};

class Help::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> dwelled{"arisa.help.dwelled",
                                                       false};
  static constexpr composition::StateKey<bool> held{"arisa.help.held", false};

  Content(Source label, std::optional<std::string> title,
          std::string text) noexcept
      : label_(std::move(label)),
        title_(std::move(title)),
        text_(std::move(text)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto title() const noexcept -> const std::optional<std::string>& {
    return title_;
  }

  auto text() const noexcept -> const std::string& { return text_; }

  void hold(Source label) noexcept { label_ = std::move(label); }

  auto pending_label() const noexcept
      -> std::optional<std::reference_wrapper<const composition::Element>> {
    if (label_ == nullptr || !label_->peek().has_value()) {
      return std::nullopt;
    }
    return std::cref(*label_->peek());
  }

  auto take_label() noexcept -> std::optional<composition::Element> {
    return label_ != nullptr ? label_->take() : std::nullopt;
  }

 private:
  Source label_;
  std::optional<std::string> title_;
  std::string text_;
};

}
