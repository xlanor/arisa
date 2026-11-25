#include <arisa/async/awaitables.hpp>
#include <arisa/runtime/text_input.hpp>

#include <optional>
#include <utility>

namespace arisa::runtime {

auto TextInput::request(provider::TextInputRequest request) const
    -> async::Task<Result> {
  return run(provider_, dismissals_, std::move(request), Draft{});
}

auto TextInput::request(provider::TextInputRequest request, Draft drafted) const
    -> async::Task<Result> {
  return run(provider_, dismissals_, std::move(request), std::move(drafted));
}

auto TextInput::run(
    std::reference_wrapper<provider::TextInputProvider> provider,
    std::shared_ptr<const std::uint64_t> dismissals,
    provider::TextInputRequest request, Draft drafted) -> async::Task<Result> {
  co_await async::next_frame();
  const std::uint64_t opened = *dismissals;
  std::string shown = request.initial;
  auto session = provider.get().begin(std::move(request));
  if (!session.has_value()) {
    co_return std::unexpected(TextInputError::from(session.error()));
  }
  while (true) {
    if (*dismissals != opened) {
      co_return std::unexpected(TextInputError{TextInputErrorCode::cancelled});
    }
    if (drafted) {
      if (std::optional<std::string> draft = session->draft();
          draft.has_value() && *draft != shown) {
        shown = std::move(*draft);
        drafted(shown);
      }
    }
    if (std::optional<provider::TextInputOutcome> outcome = session->poll()) {
      if (outcome->has_value()) {
        co_return std::move(**outcome);
      }
      co_return std::unexpected(TextInputError::from(outcome->error()));
    }
    co_await async::next_frame();
  }
}

}
