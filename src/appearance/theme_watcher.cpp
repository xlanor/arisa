#include <arisa/appearance/theme_watcher.hpp>

#include <optional>
#include <string>
#include <utility>

namespace arisa::appearance {

class ThemeWatcher::State : public std::enable_shared_from_this<State> {
 public:
  using Stamps = std::vector<std::optional<provider::FileStamp>>;

  State(AppearanceModel model, async::FrameScheduler& scheduler,
        async::Executor& background, Post post,
        std::chrono::milliseconds interval)
      : model_(std::move(model)),
        scheduler_(scheduler),
        background_(background),
        post_(std::move(post)),
        interval_(interval) {}

  void arm() {
    timer_ = scheduler_.get().after(
        interval_, [weak = weak_from_this()](std::stop_token) {
          if (const std::shared_ptr<State> state = weak.lock()) {
            state->probe();
          }
        });
  }

  auto interval() const noexcept -> std::chrono::milliseconds {
    return interval_;
  }

  auto polls() const noexcept -> std::uint64_t { return polls_; }

  auto reloads() const noexcept -> std::uint64_t { return reloads_; }

  auto failures() const noexcept -> std::uint64_t { return failures_; }

  auto watching() const noexcept -> const std::vector<ThemeLibrary::Location>& {
    return watching_;
  }

 private:
  void probe() {
    probe_ = background_.get().submit(
        [weak = weak_from_this(), post = post_, library = model_.library(),
         name = model_.settings().get().theme](std::stop_token stop) {
          if (stop.stop_requested()) {
            return;
          }
          std::vector<ThemeLibrary::Location> locations =
              library.locations(name);
          Stamps stamps;
          stamps.reserve(locations.size());
          for (const ThemeLibrary::Location& location : locations) {
            stamps.push_back(library.stamp(location));
          }
          post([weak, name, locations = std::move(locations),
                stamps = std::move(stamps)]() mutable {
            if (const std::shared_ptr<State> state = weak.lock()) {
              state->compare(name, std::move(locations), std::move(stamps));
            }
          });
        });
  }

  void compare(const std::string& name,
               std::vector<ThemeLibrary::Location> locations, Stamps stamps) {
    ++polls_;
    if (name != model_.settings().get().theme || locations != watching_ ||
        !last_.has_value()) {
      watching_ = std::move(locations);
      last_ = std::move(stamps);
      arm();
      return;
    }
    if (*last_ != stamps) {
      last_ = std::move(stamps);
      if (model_.reload().has_value()) {
        ++reloads_;
      } else {
        ++failures_;
      }
    }
    arm();
  }

  AppearanceModel model_;
  std::reference_wrapper<async::FrameScheduler> scheduler_;
  std::reference_wrapper<async::Executor> background_;
  Post post_;
  std::chrono::milliseconds interval_;
  std::vector<ThemeLibrary::Location> watching_;
  std::optional<Stamps> last_;
  std::uint64_t polls_{0};
  std::uint64_t reloads_{0};
  std::uint64_t failures_{0};
  std::optional<async::Job> timer_;
  std::optional<async::Job> probe_;
};

ThemeWatcher::ThemeWatcher(AppearanceModel model,
                           async::FrameScheduler& scheduler,
                           async::Executor& background, Post post,
                           std::chrono::milliseconds interval)
    : state_(std::make_shared<State>(std::move(model), scheduler, background,
                                     std::move(post), interval)) {
  state_->arm();
}

auto ThemeWatcher::interval() const noexcept -> std::chrono::milliseconds {
  return state_->interval();
}

auto ThemeWatcher::polls() const noexcept -> std::uint64_t {
  return state_->polls();
}

auto ThemeWatcher::reloads() const noexcept -> std::uint64_t {
  return state_->reloads();
}

auto ThemeWatcher::failures() const noexcept -> std::uint64_t {
  return state_->failures();
}

auto ThemeWatcher::watching() const
    -> const std::vector<ThemeLibrary::Location>& {
  return state_->watching();
}

}
