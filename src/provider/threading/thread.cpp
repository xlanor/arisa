#include <arisa/provider/threading/thread.hpp>

#include <utility>

namespace arisa::provider {

Thread::Thread(std::unique_ptr<Implementation> implementation) noexcept
    : implementation_(std::move(implementation)) {}

void Thread::request_stop() noexcept {
  if (implementation_ != nullptr) {
    implementation_->request_stop();
  }
}

auto Thread::joinable() const noexcept -> bool {
  return implementation_ != nullptr;
}

auto Thread::id() const noexcept -> ThreadId {
  return implementation_ != nullptr ? implementation_->id() : ThreadId{0};
}

}
