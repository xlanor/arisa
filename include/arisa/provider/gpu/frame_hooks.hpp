#pragma once

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace arisa::provider {

template <class... Args>
class FrameHooks {
 public:
  using Function = std::move_only_function<void(Args..., const FrameInfo&)>;

  FrameHooks() : state_(std::make_shared<State>()) {}

  FrameHooks(const FrameHooks&) = delete;
  auto operator=(const FrameHooks&) -> FrameHooks& = delete;
  FrameHooks(FrameHooks&&) noexcept = default;
  auto operator=(FrameHooks&&) noexcept -> FrameHooks& = default;
  ~FrameHooks() = default;

  auto add(FrameStage stage, Function function, FrameHookOptions options)
      -> FrameHook {
    const FrameHookId id = state_->add(stage, std::move(function), options);
    return FrameHook{state_, id,
                     support::ScopedCallback{[state = state_, id]() noexcept {
                       state->remove(id);
                     }}};
  }

  auto installed() const noexcept -> bool { return state_->installed(); }

  auto run(FrameStage stage, Args... args, const FrameInfo& info) -> bool {
    return state_->run(stage, args..., info);
  }

  auto schedule() const noexcept -> FrameSchedule& { return *state_; }

 private:
  class Entry {
   public:
    Entry(FrameStage stage, Function function) noexcept
        : stage_(stage), function_(std::move(function)) {}

    auto stage() const noexcept -> FrameStage { return stage_; }

    auto call(Args... args, const FrameInfo& info) -> bool {
      const std::scoped_lock lock{calling_};
      if (!function_) {
        return false;
      }
      function_(args..., info);
      return true;
    }

    void retire() noexcept {
      Function released;
      {
        const std::scoped_lock lock{calling_};
        released = std::move(function_);
        function_ = nullptr;
      }
    }

   private:
    FrameStage stage_;
    Function function_;
    std::mutex calling_;
  };

  class State : public FrameSchedule {
   public:
    auto add(FrameStage stage, Function function, FrameHookOptions options)
        -> FrameHookId {
      auto entry = std::make_shared<Entry>(stage, std::move(function));
      const FrameHookId id = adopt(options);
      const std::scoped_lock lock{entries_mutex_};
      entries_.emplace_back(id, std::move(entry));
      return id;
    }

    auto run(FrameStage stage, Args... args, const FrameInfo& info) -> bool {
      if (!installed()) {
        return false;
      }
      const std::scoped_lock running{running_mutex_};
      {
        const std::scoped_lock lock{entries_mutex_};
        for (const auto& [id, entry] : entries_) {
          if (entry->stage() == stage) {
            running_.push_back(entry);
          }
        }
      }
      bool ran = false;
      for (const std::shared_ptr<Entry>& entry : running_) {
        ran = entry->call(args..., info) || ran;
      }
      running_.clear();
      return ran;
    }

    void remove(FrameHookId id) noexcept {
      std::shared_ptr<Entry> removed;
      {
        const std::scoped_lock lock{entries_mutex_};
        const auto found = std::ranges::find(
            entries_, id,
            &std::pair<FrameHookId, std::shared_ptr<Entry>>::first);
        if (found != entries_.end()) {
          removed = std::move(found->second);
          entries_.erase(found);
        }
      }
      retire(id);
      if (removed != nullptr) {
        removed->retire();
      }
    }

   private:
    std::mutex entries_mutex_;
    std::vector<std::pair<FrameHookId, std::shared_ptr<Entry>>> entries_;
    std::mutex running_mutex_;
    std::vector<std::shared_ptr<Entry>> running_;
  };

  std::shared_ptr<State> state_;
};

}
