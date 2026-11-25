#include <arisa/provider/headless/scripted_text_input.hpp>

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace arisa::provider::headless {

class ScriptedTextInput::Ledger {
 public:
  struct Reply {
    TextInputOutcome outcome;
    std::uint32_t polls_before;
    std::vector<std::string> drafts;
  };

  auto open(TextInputRequest request) -> std::optional<Reply> {
    const std::scoped_lock lock{mutex_};
    requests_.push_back(std::move(request));
    ++live_;
    if (replies_.empty()) {
      return std::nullopt;
    }
    Reply reply = std::move(replies_.front());
    replies_.pop_front();
    return reply;
  }

  void close(bool finished) noexcept {
    const std::scoped_lock lock{mutex_};
    --live_;
    if (!finished) {
      ++cancelled_;
    }
  }

  void push(Reply reply) {
    const std::scoped_lock lock{mutex_};
    replies_.push_back(std::move(reply));
  }

  auto requests() const -> std::vector<TextInputRequest> {
    const std::scoped_lock lock{mutex_};
    return requests_;
  }

  auto live() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return live_;
  }

  auto cancelled() const -> std::size_t {
    const std::scoped_lock lock{mutex_};
    return cancelled_;
  }

  void refuse(TextInputTraitSet traits) {
    const std::scoped_lock lock{mutex_};
    refused_ = traits;
  }

  auto refused() const -> TextInputTraitSet {
    const std::scoped_lock lock{mutex_};
    return refused_;
  }

 private:
  mutable std::mutex mutex_;
  std::deque<Reply> replies_;
  std::vector<TextInputRequest> requests_;
  std::size_t live_{0};
  std::size_t cancelled_{0};
  TextInputTraitSet refused_;
};

class ScriptedTextInput::Session final
    : public TextInputSession::Implementation {
 public:
  Session(std::shared_ptr<Ledger> ledger,
          std::optional<Ledger::Reply> reply) noexcept
      : ledger_(std::move(ledger)), reply_(std::move(reply)) {}

  Session(const Session&) = delete;
  auto operator=(const Session&) -> Session& = delete;
  Session(Session&&) = delete;
  auto operator=(Session&&) -> Session& = delete;

  ~Session() override { ledger_->close(finished_); }

  auto poll() -> std::optional<TextInputOutcome> override {
    if (!reply_.has_value()) {
      return std::nullopt;
    }
    if (polls_ < reply_->polls_before) {
      ++polls_;
      return std::nullopt;
    }
    if (shown_ < reply_->drafts.size()) {
      ++shown_;
      return std::nullopt;
    }
    if (!reply_->drafts.empty() && !settled_) {
      settled_ = true;
      return std::nullopt;
    }
    finished_ = true;
    return reply_->outcome;
  }

  auto draft() const -> std::optional<std::string> override {
    if (!reply_.has_value() || shown_ == 0) {
      return std::nullopt;
    }
    return reply_->drafts[shown_ - 1];
  }

 private:
  std::shared_ptr<Ledger> ledger_;
  std::optional<Ledger::Reply> reply_;
  std::uint32_t polls_{0};
  std::size_t shown_{0};
  bool settled_{false};
  bool finished_{false};
};

ScriptedTextInput::ScriptedTextInput() : ledger_(std::make_shared<Ledger>()) {}

auto ScriptedTextInput::begin(TextInputRequest request)
    -> std::expected<TextInputSession, ProviderError> {
  auto reply = ledger_->open(std::move(request));
  return TextInputSession{std::make_unique<Session>(ledger_, std::move(reply))};
}

auto ScriptedTextInput::unhonoured(const TextInputRequest& request) const
    -> TextInputTraitSet {
  return request.traits.requested() & ledger_->refused();
}

void ScriptedTextInput::refuse(TextInputTraitSet traits) {
  ledger_->refuse(traits);
}

void ScriptedTextInput::push(TextInputOutcome outcome,
                             std::uint32_t polls_before) {
  ledger_->push(Ledger::Reply{.outcome = std::move(outcome),
                              .polls_before = polls_before,
                              .drafts = {}});
}

void ScriptedTextInput::push(TextInputOutcome outcome,
                             std::vector<std::string> drafts) {
  ledger_->push(Ledger::Reply{.outcome = std::move(outcome),
                              .polls_before = 0,
                              .drafts = std::move(drafts)});
}

auto ScriptedTextInput::requests() const -> std::vector<TextInputRequest> {
  return ledger_->requests();
}

auto ScriptedTextInput::live_sessions() const -> std::size_t {
  return ledger_->live();
}

auto ScriptedTextInput::cancelled_sessions() const -> std::size_t {
  return ledger_->cancelled();
}

}
