#pragma once

#include <arisa/async/task.hpp>
#include <arisa/provider/text_input/provider.hpp>
#include <arisa/runtime/text_input_error.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace arisa::runtime {

class TextInput {
 public:
  using Result = std::expected<std::string, TextInputError>;
  using Draft = std::move_only_function<void(const std::string&)>;

  explicit TextInput(provider::TextInputProvider& provider)
      : provider_(provider), dismissals_(std::make_shared<std::uint64_t>(0U)) {}

  auto request(provider::TextInputRequest request) const -> async::Task<Result>;

  auto request(provider::TextInputRequest request, Draft drafted) const
      -> async::Task<Result>;

  void dismiss() const { ++*dismissals_; }

  auto unhonoured(const provider::TextInputRequest& request) const
      -> provider::TextInputTraitSet {
    return provider_.get().unhonoured(request);
  }

  friend auto operator==(const TextInput& left, const TextInput& right) noexcept
      -> bool {
    return &left.provider_.get() == &right.provider_.get();
  }

 private:
  static auto run(std::reference_wrapper<provider::TextInputProvider> provider,
                  std::shared_ptr<const std::uint64_t> dismissals,
                  provider::TextInputRequest request, Draft drafted)
      -> async::Task<Result>;

  std::reference_wrapper<provider::TextInputProvider> provider_;
  std::shared_ptr<std::uint64_t> dismissals_;
};

}
