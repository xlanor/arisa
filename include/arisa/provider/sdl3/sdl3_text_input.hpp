#pragma once

#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/text_input/provider.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::provider::sdl3 {

class Sdl3TextInput final : public TextInputProvider {
 public:
  explicit Sdl3TextInput(Sdl3EventPump& pump,
                         SDL_Window* window = nullptr) noexcept;

  [[nodiscard]] auto begin(TextInputRequest request)
      -> std::expected<TextInputSession, ProviderError> override;

  auto unhonoured(const TextInputRequest& request) const
      -> TextInputTraitSet override;

  static auto code_points(std::string_view text) noexcept -> std::size_t;

  static void erase_last_code_point(std::string& text) noexcept;

 private:
  class Session;

  struct Active {};

  static auto input_type(const TextInputRequest& request) noexcept
      -> std::optional<SDL_TextInputType>;

  static auto capitalization(TextInputAutocapitalization style) noexcept
      -> SDL_Capitalization;

  static auto keyboard_type(KeyboardType keyboard) noexcept
      -> std::optional<SDL_TextInputType>;

  static auto content_type(TextContentType content) noexcept
      -> std::optional<SDL_TextInputType>;

  void start(const TextInputRequest& request) const;

  std::reference_wrapper<Sdl3EventPump> pump_;
  SDL_Window* window_;
  std::weak_ptr<const Active> active_;
};

}
