#include <arisa/async/awaitables.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/detail_presenter.hpp>

#include <algorithm>
#include <format>
#include <ranges>

namespace arisa::structure {

const composition::EnvironmentKey<std::chrono::milliseconds>
    DetailPresenter::delay{"detail_settle_delay",
                           DetailPresenter::default_delay};

const composition::StateKey<std::optional<composition::TagKey>>
    DetailPresenter::presented{"detail-presenter-presented", std::nullopt};

const composition::StateKey<DetailPresenter::Recent> DetailPresenter::kept{
    "detail-presenter-kept", DetailPresenter::Recent{}};

DetailPresenter::DetailPresenter(composition::Context& context, Request request)
    : target_(context.state(presented)),
      shown_(std::move(request.shown)),
      delay_(context.environment(delay)),
      space_(request.space) {
  Recent& recent = context.retained(kept);
  if (!request.delivered && !recent.contains(shown_)) {
    recent.claim(shown_);
  }
  const composition::TagKey resting =
      target_.get().value_or(recent.front().value_or(shown_));
  const bool deferred = request.following &&
                        delay_ > std::chrono::milliseconds::zero() &&
                        recent.contains(resting);
  displayed_ = deferred ? resting : shown_;
  settled_ = resting == shown_;
  recent.remember(displayed_);
  alive_ = recent.entries();
  std::ranges::sort(alive_, {}, &Entry::id);
  generation_ = recent.generation(shown_);
}

auto DetailPresenter::commit() const -> Commit { return Commit{target_}; }

auto DetailPresenter::settle(
    composition::StateHandle<std::optional<composition::TagKey>> target,
    composition::TagKey key, std::chrono::milliseconds wait)
    -> async::Task<void> {
  co_await async::sleep(
      std::chrono::duration_cast<std::chrono::nanoseconds>(wait));
  target.set(std::move(key));
}

auto DetailPresenter::stack(const Build& build) const -> composition::Element {
  std::vector<composition::Element> built;
  built.reserve(alive_.size());
  for (const Entry& entry : alive_) {
    const bool hidden = entry.key != displayed_;
    built.push_back(composition::Element{
        Page{build(Slot{.key = entry.key,
                        .frozen = hidden || displayed_ != shown_}),
             hidden}}
                        .frame({.max_width = composition::infinity,
                                .min_height = 0.0F,
                                .max_height = composition::infinity})
                        .opacity(hidden ? 0.0F : 1.0F)
                        .animation(std::nullopt, hidden)
                        .allows_hit_testing(!hidden)
                        .disabled(hidden)
                        .id(identify(entry.id)));
  }
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           components::Alignment::top_leading},
             std::move(built))
      .frame({.max_width = composition::infinity,
              .min_height = 0.0F,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading})
      .flex_shrink(1.0F);
}

auto DetailPresenter::identify(std::int64_t id) const
    -> composition::ElementKey {
  if (space_.empty()) {
    return composition::ElementKey{id};
  }
  return composition::ElementKey{std::format("{}.{}", space_, id)};
}

auto DetailPresenter::settling(composition::Element element) const
    -> composition::Element {
  if (settled_) {
    return element;
  }
  return std::move(element).task(
      composition::ElementKey{generation_},
      [target = target_, shown = shown_, wait = delay_] {
        return settle(target, shown, wait);
      });
}

DetailPresenter::Page::Page(composition::Element content, bool hidden)
    : Element(std::make_unique<components::ZStack::Content>(
                  components::Alignment::top_leading),
              [&content] {
                std::vector<composition::Element> children;
                children.push_back(std::move(content));
                return children;
              }()) {
  declared().covered = hidden;
}

auto DetailPresenter::Recent::contains(const composition::TagKey& key) const
    -> bool {
  return std::ranges::contains(entries_, key, &Entry::key);
}

auto DetailPresenter::Recent::front() const
    -> std::optional<composition::TagKey> {
  if (entries_.empty()) {
    return std::nullopt;
  }
  return entries_.front().key;
}

void DetailPresenter::Recent::claim(const composition::TagKey& key) {
  if (!entries_.empty()) {
    entries_.front().key = key;
  }
}

void DetailPresenter::Recent::remember(const composition::TagKey& key) {
  const auto found = std::ranges::find(entries_, key, &Entry::key);
  Entry entry =
      found != entries_.end() ? *found : Entry{.key = key, .id = next_++};
  if (found != entries_.end()) {
    entries_.erase(found);
  }
  entries_.insert(entries_.begin(), std::move(entry));
  if (entries_.size() > kept_pages) {
    entries_.resize(kept_pages);
  }
}

auto DetailPresenter::Recent::generation(const composition::TagKey& shown)
    -> std::int64_t {
  if (last_shown_ != shown) {
    last_shown_ = shown;
    ++generation_;
  }
  return generation_;
}

}
