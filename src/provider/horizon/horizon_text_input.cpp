#include <arisa/provider/horizon/horizon_text_input.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <switch.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace arisa::provider::horizon {

class HorizonTextInput::Exchange {
 public:
  explicit Exchange(TextInputRequest request) noexcept
      : request_(std::move(request)) {}

  auto request() const noexcept -> const TextInputRequest& { return request_; }

  void cancel() noexcept {
    const std::scoped_lock lock{mutex_};
    cancelled_ = true;
  }

  auto cancelled() const -> bool {
    const std::scoped_lock lock{mutex_};
    return cancelled_;
  }

  void complete(TextInputOutcome outcome) {
    const std::scoped_lock lock{mutex_};
    outcome_ = std::move(outcome);
  }

  auto outcome() const -> std::optional<TextInputOutcome> {
    const std::scoped_lock lock{mutex_};
    return outcome_;
  }

  auto finished() const -> bool {
    const std::scoped_lock lock{mutex_};
    return outcome_.has_value() || cancelled_;
  }

  void draft(std::string text) {
    const std::scoped_lock lock{mutex_};
    draft_ = std::move(text);
  }

  auto draft() const -> std::optional<std::string> {
    const std::scoped_lock lock{mutex_};
    return draft_;
  }

 private:
  TextInputRequest request_;
  mutable std::mutex mutex_;
  std::optional<TextInputOutcome> outcome_;
  std::optional<std::string> draft_;
  bool cancelled_{false};
};

class HorizonTextInput::Keyboard {
 public:
  explicit Keyboard(std::shared_ptr<std::atomic<bool>> shown) noexcept
      : shown_(std::move(shown)) {}

  auto submit(const std::shared_ptr<Exchange>& exchange) -> bool {
    {
      const std::scoped_lock lock{mutex_};
      const bool replaceable = current_ == nullptr ||
                               current_->outcome().has_value() ||
                               (queued_ && current_->cancelled());
      if (!replaceable) {
        return false;
      }
      current_ = exchange;
      queued_ = true;
    }
    wake_.notify_one();
    return true;
  }

  void run(const std::stop_token& token) {
    while (true) {
      std::shared_ptr<Exchange> exchange;
      {
        std::unique_lock lock{mutex_};
        if (!wake_.wait(lock, token, [this] { return queued_; })) {
          return;
        }
        queued_ = false;
        exchange = current_;
      }
      if (exchange->cancelled()) {
        exchange->complete(
            std::unexpected(ProviderError{ProviderErrorCode::cancelled}));
        continue;
      }
      if (exchange->request().live) {
        if (std::optional<TextInputOutcome> typed = type(*exchange)) {
          exchange->complete(std::move(*typed));
          continue;
        }
      }
      exchange->complete(show(exchange->request()));
    }
  }

 private:
  static constexpr std::uint64_t update_interval_ns = 16'000'000;

  class Typing {
   public:
    explicit Typing(Exchange& exchange) noexcept : exchange_(exchange) {}

    void changed(const char* text) const { exchange_.get().draft(text); }

    void entered(const char* text) { decided_ = std::string{text}; }

    void cancelled() {
      decided_ = std::unexpected(ProviderError{ProviderErrorCode::cancelled});
    }

    auto decided() const -> const std::optional<TextInputOutcome>& {
      return decided_;
    }

    auto exchange() const noexcept -> Exchange& { return exchange_.get(); }

   private:
    std::reference_wrapper<Exchange> exchange_;
    std::optional<TextInputOutcome> decided_;
  };

  static void on_changed(const char* text, SwkbdChangedStringArg*) {
    if (typing_ != nullptr && text != nullptr) {
      typing_->changed(text);
    }
  }

  static void on_entered(const char* text, SwkbdDecidedEnterArg*) {
    if (typing_ != nullptr) {
      typing_->entered(text != nullptr ? text : "");
    }
  }

  static void on_cancelled() {
    if (typing_ != nullptr) {
      typing_->cancelled();
    }
  }

  auto type(Exchange& exchange) -> std::optional<TextInputOutcome> {
    const TextInputRequest& request = exchange.request();
    SwkbdInline keyboard{};
    if (R_FAILED(swkbdInlineCreate(&keyboard))) {
      return std::nullopt;
    }
    const support::ScopedCallback close{
        [&keyboard]() noexcept { swkbdInlineClose(&keyboard); }};
    if (R_FAILED(swkbdInlineLaunchForLibraryApplet(
            &keyboard, SwkbdInlineMode_AppletDisplay, 0))) {
      return std::nullopt;
    }
    Typing typing{exchange};
    typing_ = &typing;
    const support::ScopedCallback forget{[this]() noexcept {
      typing_ = nullptr;
      shown_->store(false, std::memory_order_release);
    }};
    swkbdInlineSetChangedStringCallback(&keyboard, &Keyboard::on_changed);
    swkbdInlineSetDecidedEnterCallback(&keyboard, &Keyboard::on_entered);
    swkbdInlineSetDecidedCancelCallback(&keyboard, &Keyboard::on_cancelled);
    SwkbdAppearArg appear{};
    swkbdInlineMakeAppearArg(&appear, SwkbdType_Normal);
    swkbdInlineAppearArgSetStringLenMax(
        &appear, static_cast<s32>(request.max_length.value()));
    if (request.traits.submit_label != SubmitLabel::return_key) {
      const std::string label{
          TextInputTraits::submit_text(request.traits.submit_label)};
      swkbdInlineAppearArgSetOkButtonText(&appear, label.c_str());
    }
    swkbdInlineSetInputText(&keyboard, request.initial.c_str());
    swkbdInlineSetCursorPos(&keyboard,
                            static_cast<s32>(request.initial.size()));
    shown_->store(true, std::memory_order_release);
    swkbdInlineAppear(&keyboard, &appear);
    SwkbdState state = SwkbdState_Inactive;
    while (!typing.decided().has_value()) {
      if (exchange.cancelled()) {
        swkbdInlineDisappear(&keyboard);
        return TextInputOutcome{
            std::unexpected(ProviderError{ProviderErrorCode::cancelled})};
      }
      if (R_FAILED(swkbdInlineUpdate(&keyboard, &state))) {
        return TextInputOutcome{std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure})};
      }
      svcSleepThread(update_interval_ns);
    }
    return typing.decided();
  }

  static auto show(const TextInputRequest& request) -> TextInputOutcome {
    SwkbdConfig config{};
    const Result created = swkbdCreate(&config, 0);
    if (R_FAILED(created)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure, created});
    }
    const support::ScopedCallback close{
        [&config]() noexcept { swkbdClose(&config); }};
    configure(config, request);
    swkbdConfigSetInitialText(&config, request.initial.c_str());
    if (!request.hint.empty()) {
      swkbdConfigSetGuideText(&config, request.hint.c_str());
    }
    swkbdConfigSetStringLenMax(&config, request.max_length.value());
    std::vector<char> text(
        (static_cast<std::size_t>(request.max_length.value()) * 4) + 1, '\0');
    const Result shown = swkbdShow(&config, text.data(), text.size());
    if (R_SUCCEEDED(shown)) {
      return std::string{text.data()};
    }
    if (shown == MAKERESULT(Module_Libnx, LibnxError_LibAppletBadExit)) {
      return std::unexpected(ProviderError{ProviderErrorCode::cancelled});
    }
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, shown});
  }

  static auto secret(const TextInputRequest& request) noexcept -> bool {
    const std::optional<TextContentType> content = request.traits.content;
    return request.kind == TextInputKind::password ||
           content == TextContentType::password ||
           content == TextContentType::new_password;
  }

  static void configure(SwkbdConfig& config, const TextInputRequest& request) {
    const TextInputTraits& traits = request.traits;
    if (secret(request)) {
      swkbdConfigMakePresetPassword(&config);
    } else if (traits.content == TextContentType::username) {
      swkbdConfigMakePresetUserName(&config);
    } else {
      swkbdConfigMakePresetDefault(&config);
    }
    if (traits.content == TextContentType::one_time_code) {
      swkbdConfigSetType(&config, SwkbdType_NumPad);
    }
    switch (traits.keyboard) {
      case KeyboardType::ascii_capable:
        swkbdConfigSetType(&config, SwkbdType_Latin);
        break;
      case KeyboardType::numbers_and_punctuation:
        swkbdConfigSetType(&config, SwkbdType_NumPad);
        swkbdConfigSetLeftOptionalSymbolKey(&config, "-");
        swkbdConfigSetRightOptionalSymbolKey(&config, ".");
        break;
      case KeyboardType::url:
      case KeyboardType::email_address:
        swkbdConfigSetType(&config, SwkbdType_QWERTY);
        swkbdConfigSetKeySetDisableBitmask(&config,
                                           SwkbdKeyDisableBitmask_Space);
        break;
      case KeyboardType::web_search:
        swkbdConfigSetType(&config, SwkbdType_QWERTY);
        break;
      case KeyboardType::number_pad:
      case KeyboardType::ascii_capable_number_pad:
        swkbdConfigSetType(&config, SwkbdType_NumPad);
        break;
      case KeyboardType::phone_pad:
        swkbdConfigSetType(&config, SwkbdType_NumPad);
        swkbdConfigSetLeftOptionalSymbolKey(&config, "+");
        swkbdConfigSetRightOptionalSymbolKey(&config, "#");
        break;
      case KeyboardType::decimal_pad:
        swkbdConfigSetType(&config, SwkbdType_NumPad);
        swkbdConfigSetRightOptionalSymbolKey(&config, ".");
        break;
      case KeyboardType::automatic:
      case KeyboardType::name_phone_pad:
        break;
    }
    if (traits.autocorrection_disabled.has_value()) {
      swkbdConfigSetDicFlag(&config, *traits.autocorrection_disabled ? 0 : 1);
    }
    if (traits.submit_label != SubmitLabel::return_key) {
      const std::string label{
          TextInputTraits::submit_text(traits.submit_label)};
      swkbdConfigSetOkButtonText(&config, label.c_str());
    }
    swkbdConfigSetReturnButtonFlag(&config, traits.multiline ? 1 : 0);
    if (traits.multiline) {
      swkbdConfigSetTextDrawType(&config, SwkbdTextDrawType_Box);
    }
  }

  static inline Typing* typing_{nullptr};

  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::shared_ptr<Exchange> current_;
  std::shared_ptr<std::atomic<bool>> shown_;
  bool queued_{false};
};

class HorizonTextInput::Session final
    : public TextInputSession::Implementation {
 public:
  explicit Session(std::shared_ptr<Exchange> exchange) noexcept
      : exchange_(std::move(exchange)) {}

  Session(const Session&) = delete;
  auto operator=(const Session&) -> Session& = delete;
  Session(Session&&) = delete;
  auto operator=(Session&&) -> Session& = delete;

  ~Session() override { exchange_->cancel(); }

  auto poll() -> std::optional<TextInputOutcome> override {
    return exchange_->outcome();
  }

  auto draft() const -> std::optional<std::string> override {
    return exchange_->draft();
  }

 private:
  std::shared_ptr<Exchange> exchange_;
};

HorizonTextInput::HorizonTextInput(std::unique_ptr<Keyboard> keyboard,
                                   Thread thread) noexcept
    : keyboard_(std::move(keyboard)), thread_(std::move(thread)) {}

HorizonTextInput::HorizonTextInput(HorizonTextInput&& other) noexcept = default;

HorizonTextInput::~HorizonTextInput() = default;

auto HorizonTextInput::create(ThreadingProvider& threading)
    -> std::expected<HorizonTextInput, ProviderError> {
  return create(threading, std::make_shared<std::atomic<bool>>(false));
}

auto HorizonTextInput::create(ThreadingProvider& threading,
                              std::shared_ptr<std::atomic<bool>> shown)
    -> std::expected<HorizonTextInput, ProviderError> {
  auto keyboard = std::make_unique<Keyboard>(std::move(shown));
  auto thread = threading.start(
      ThreadSpec{.name = "arisa-swkbd", .role = ThreadRole::blocking},
      [&served = *keyboard](const std::stop_token& token) {
        served.run(token);
      });
  if (!thread.has_value()) {
    return std::unexpected(thread.error());
  }
  return HorizonTextInput{std::move(keyboard), std::move(*thread)};
}

auto HorizonTextInput::unhonoured(const TextInputRequest& request) const
    -> TextInputTraitSet {
  const TextInputTraits& traits = request.traits;
  TextInputTraitSet refused;
  if (traits.keyboard == KeyboardType::name_phone_pad) {
    refused.insert(TextInputTrait::keyboard_type);
  }
  if (traits.content.has_value()) {
    switch (*traits.content) {
      case TextContentType::username:
      case TextContentType::password:
      case TextContentType::new_password:
      case TextContentType::one_time_code:
        break;
      default:
        refused.insert(TextInputTrait::text_content_type);
        break;
    }
  }
  if (traits.capitalization.has_value()) {
    refused.insert(TextInputTrait::autocapitalization);
  }
  return refused;
}

auto HorizonTextInput::begin(TextInputRequest request)
    -> std::expected<TextInputSession, ProviderError> {
  auto exchange = std::make_shared<Exchange>(std::move(request));
  if (!keyboard_->submit(exchange)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  return TextInputSession{std::make_unique<Session>(std::move(exchange))};
}

}
