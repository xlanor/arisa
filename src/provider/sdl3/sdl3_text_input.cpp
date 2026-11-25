#include <arisa/provider/sdl3/sdl3_text_input.hpp>

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_properties.h>

#include <optional>
#include <utility>

namespace arisa::provider::sdl3 {

class Sdl3TextInput::Session final : public TextInputSession::Implementation {
 public:
  Session(Sdl3EventPump& pump, SDL_Window* window, TextInputRequest request,
          std::shared_ptr<const Active> active)
      : pump_(pump),
        window_(window),
        text_events_(pump.subscribe(Sdl3EventCategory::text)),
        key_events_(pump.subscribe(Sdl3EventCategory::input)),
        text_(std::move(request.initial)),
        limit_(request.max_length.value()),
        multiline_(request.traits.multiline),
        active_(std::move(active)) {
    while (code_points(text_) > limit_) {
      erase_last_code_point(text_);
    }
  }

  Session(const Session&) = delete;
  auto operator=(const Session&) -> Session& = delete;
  Session(Session&&) = delete;
  auto operator=(Session&&) -> Session& = delete;

  ~Session() override {
    if (window_ != nullptr) {
      static_cast<void>(SDL_StartTextInput(window_));
    }
  }

  auto poll() -> std::optional<TextInputOutcome> override {
    if (outcome_.has_value()) {
      return outcome_;
    }
    pump_.get().pump();
    while (const std::optional<SDL_Event> event = text_events_.next()) {
      if (event->type == SDL_EVENT_TEXT_INPUT && event->text.text != nullptr) {
        append(event->text.text);
      }
    }
    while (const std::optional<SDL_Event> event = key_events_.next()) {
      if (event->type != SDL_EVENT_KEY_DOWN || outcome_.has_value()) {
        continue;
      }
      switch (event->key.key) {
        case SDLK_RETURN:
          if (multiline_ &&
              (event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0U) {
            append("\n");
            break;
          }
          outcome_ = text_;
          break;
        case SDLK_KP_ENTER:
          outcome_ = text_;
          break;
        case SDLK_ESCAPE:
          outcome_ =
              std::unexpected(ProviderError{ProviderErrorCode::cancelled});
          break;
        case SDLK_BACKSPACE:
          erase_last_code_point(text_);
          break;
        default:
          break;
      }
    }
    return outcome_;
  }

  auto draft() const -> std::optional<std::string> override { return text_; }

 private:
  void append(std::string_view typed) {
    std::size_t length = code_points(text_);
    for (std::size_t index = 0; index < typed.size();) {
      std::size_t width = 1;
      while (index + width < typed.size() &&
             (static_cast<unsigned char>(typed[index + width]) & 0xC0U) ==
                 0x80U) {
        ++width;
      }
      if (length == limit_) {
        return;
      }
      text_.append(typed.substr(index, width));
      ++length;
      index += width;
    }
  }

  std::reference_wrapper<Sdl3EventPump> pump_;
  SDL_Window* window_;
  Sdl3EventPump::Subscription text_events_;
  Sdl3EventPump::Subscription key_events_;
  std::string text_;
  std::size_t limit_;
  bool multiline_;
  std::shared_ptr<const Active> active_;
  std::optional<TextInputOutcome> outcome_;
};

Sdl3TextInput::Sdl3TextInput(Sdl3EventPump& pump, SDL_Window* window) noexcept
    : pump_(pump), window_(window) {}

auto Sdl3TextInput::begin(TextInputRequest request)
    -> std::expected<TextInputSession, ProviderError> {
  if (!active_.expired()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  auto active = std::make_shared<const Active>();
  active_ = active;
  start(request);
  return TextInputSession{std::make_unique<Session>(
      pump_.get(), window_, std::move(request), std::move(active))};
}

auto Sdl3TextInput::unhonoured(const TextInputRequest& request) const
    -> TextInputTraitSet {
  const TextInputTraits& traits = request.traits;
  TextInputTraitSet refused;
  if (traits.submit_label != SubmitLabel::return_key) {
    refused.insert(TextInputTrait::submit_label);
  }
  const bool keyboard_known = traits.keyboard == KeyboardType::automatic ||
                              keyboard_type(traits.keyboard).has_value();
  const bool content_known =
      !traits.content.has_value() || content_type(*traits.content).has_value();
  if (traits.keyboard != KeyboardType::automatic &&
      (window_ == nullptr || !keyboard_known)) {
    refused.insert(TextInputTrait::keyboard_type);
  }
  if (traits.content.has_value() && (window_ == nullptr || !content_known)) {
    refused.insert(TextInputTrait::text_content_type);
  }
  if (window_ == nullptr) {
    if (traits.capitalization.has_value()) {
      refused.insert(TextInputTrait::autocapitalization);
    }
    if (traits.autocorrection_disabled.has_value()) {
      refused.insert(TextInputTrait::autocorrection);
    }
  }
  return refused;
}

void Sdl3TextInput::start(const TextInputRequest& request) const {
  if (window_ == nullptr) {
    return;
  }
  const TextInputTraits& traits = request.traits;
  const SDL_PropertiesID properties = SDL_CreateProperties();
  if (properties == 0) {
    return;
  }
  if (const std::optional<SDL_TextInputType> type = input_type(request)) {
    SDL_SetNumberProperty(properties, SDL_PROP_TEXTINPUT_TYPE_NUMBER, *type);
  }
  if (traits.capitalization.has_value()) {
    SDL_SetNumberProperty(properties, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER,
                          capitalization(*traits.capitalization));
  }
  if (traits.autocorrection_disabled.has_value()) {
    SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN,
                           !*traits.autocorrection_disabled);
  }
  SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN,
                         traits.multiline);
  static_cast<void>(SDL_StartTextInputWithProperties(window_, properties));
  SDL_DestroyProperties(properties);
}

auto Sdl3TextInput::input_type(const TextInputRequest& request) noexcept
    -> std::optional<SDL_TextInputType> {
  if (request.kind == TextInputKind::password) {
    return SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN;
  }
  if (request.traits.content.has_value()) {
    if (const auto type = content_type(*request.traits.content)) {
      return type;
    }
  }
  return keyboard_type(request.traits.keyboard);
}

auto Sdl3TextInput::capitalization(TextInputAutocapitalization style) noexcept
    -> SDL_Capitalization {
  switch (style) {
    case TextInputAutocapitalization::never:
      return SDL_CAPITALIZE_NONE;
    case TextInputAutocapitalization::words:
      return SDL_CAPITALIZE_WORDS;
    case TextInputAutocapitalization::sentences:
      return SDL_CAPITALIZE_SENTENCES;
    case TextInputAutocapitalization::characters:
      return SDL_CAPITALIZE_LETTERS;
  }
  return SDL_CAPITALIZE_SENTENCES;
}

auto Sdl3TextInput::keyboard_type(KeyboardType keyboard) noexcept
    -> std::optional<SDL_TextInputType> {
  switch (keyboard) {
    case KeyboardType::automatic:
      return SDL_TEXTINPUT_TYPE_TEXT;
    case KeyboardType::number_pad:
    case KeyboardType::decimal_pad:
    case KeyboardType::phone_pad:
    case KeyboardType::ascii_capable_number_pad:
      return SDL_TEXTINPUT_TYPE_NUMBER;
    case KeyboardType::email_address:
      return SDL_TEXTINPUT_TYPE_TEXT_EMAIL;
    case KeyboardType::ascii_capable:
    case KeyboardType::numbers_and_punctuation:
    case KeyboardType::url:
    case KeyboardType::name_phone_pad:
    case KeyboardType::web_search:
      return std::nullopt;
  }
  return std::nullopt;
}

auto Sdl3TextInput::content_type(TextContentType content) noexcept
    -> std::optional<SDL_TextInputType> {
  switch (content) {
    case TextContentType::name:
    case TextContentType::given_name:
    case TextContentType::family_name:
    case TextContentType::nickname:
      return SDL_TEXTINPUT_TYPE_TEXT_NAME;
    case TextContentType::username:
      return SDL_TEXTINPUT_TYPE_TEXT_USERNAME;
    case TextContentType::password:
    case TextContentType::new_password:
      return SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN;
    case TextContentType::one_time_code:
    case TextContentType::telephone_number:
      return SDL_TEXTINPUT_TYPE_NUMBER;
    case TextContentType::email_address:
      return SDL_TEXTINPUT_TYPE_TEXT_EMAIL;
    case TextContentType::url:
    case TextContentType::full_street_address:
    case TextContentType::postal_code:
    case TextContentType::organization_name:
    case TextContentType::job_title:
      return std::nullopt;
  }
  return std::nullopt;
}

auto Sdl3TextInput::code_points(std::string_view text) noexcept -> std::size_t {
  std::size_t count = 0;
  for (const char byte : text) {
    if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
      ++count;
    }
  }
  return count;
}

void Sdl3TextInput::erase_last_code_point(std::string& text) noexcept {
  while (!text.empty()) {
    const auto byte = static_cast<unsigned char>(text.back());
    text.pop_back();
    if ((byte & 0xC0U) != 0x80U) {
      return;
    }
  }
}

}
