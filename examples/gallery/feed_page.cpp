#include <feed_page.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/refreshable.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <type_traits>
#include <utility>

namespace arisa::examples {

using composition::Element;
using composition::infinity;

const composition::StateKey<std::vector<FeedPage::Item>> FeedPage::items_key{
    "feed-items", FeedPage::fetch(0, 0)};

auto FeedPage::fetch(int page, int refreshes) -> std::vector<Item> {
  std::vector<Item> found;
  if (page < 0 || page >= page_count) {
    return found;
  }
  for (int index = 0; index < page_size; ++index) {
    const int id = (page * page_size) + index;
    found.push_back(Item{
        .id = id,
        .title = std::format("Post {}", id + 1),
        .detail = refreshes == 0 ? std::format("page {}", page + 1)
                                 : std::format("refreshed {}", refreshes)});
  }
  return found;
}

auto FeedPage::load(int page, int refreshes,
                    composition::StateHandle<std::vector<Item>> items)
    -> async::Task<void> {
  for (int frame = 0; frame < latency_frames; ++frame) {
    co_await async::next_frame();
  }
  const std::vector<Item> arrived = fetch(page, refreshes);
  items.update([&arrived](std::vector<Item>& held) {
    for (const Item& item : arrived) {
      if (std::ranges::none_of(held, [&item](const Item& existing) {
            return existing.id == item.id;
          })) {
        held.push_back(item);
      }
    }
  });
}

auto FeedPage::reload(composition::StateHandle<std::vector<Item>> items,
                      composition::StateHandle<int> requested,
                      composition::StateHandle<int> refreshes)
    -> async::Task<void> {
  for (int frame = 0; frame < latency_frames * 4; ++frame) {
    co_await async::next_frame();
  }
  const int next = refreshes.get() + 1;
  refreshes.set(next);
  items.set(fetch(0, next));
  requested.set(1);
}

auto FeedPage::row(const Item& item) -> Element {
  return components::HStack(
             components::Text(item.title), components::Spacer(),
             components::Text(item.detail)
                 .font(theme::Font::footnote)
                 .foreground_style(theme::ColourRole::text_secondary))
      .padding(theme::Insets{.top = theme::space::sm,
                             .leading = theme::space::md,
                             .bottom = theme::space::sm,
                             .trailing = theme::space::md})
      .frame({.max_width = infinity})
      .background(theme::ColourRole::secondary_system_grouped_background,
                  theme::RoundedRectangle{8.0F})
      .focusable();
}

auto FeedPage::body(composition::Context& context) const -> Element {
  const auto items = items_;
  const auto requested = context.state(requested_key);
  const auto refreshes = context.state(refreshes_key);
  const auto position = position_;
  const std::vector<Item>& shown = items.get();
  const int loaded = static_cast<int>(shown.size()) / page_size;
  const bool more = loaded < page_count;
  const auto ask = [requested, loaded, more] {
    if (more && requested.get() <= loaded) {
      requested.set(loaded + 1);
    }
  };
  std::vector<int> tail;
  if (more) {
    tail.push_back(0);
  }
  const int count = static_cast<int>(shown.size());
  Element list =
      components::ScrollView(
          components::LazyVStack(
              components::LazyVStackOptions{
                  .alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::space::xs},
              components::ForEach(
                  shown,
                  [](const Item& item) {
                    return composition::ElementKey{item.id};
                  },
                  [](const Item& item) { return row(item); }),
              components::ForEach(
                  tail,
                  [](int) {
                    return composition::ElementKey{std::string{loading_row}};
                  },
                  [ask](int) {
                    return Element{controls::ProgressView("Loading")}
                        .frame({.height = 44.0F, .max_width = infinity})
                        .on_scroll_visibility_change([ask](bool visible) {
                          if (visible) {
                            ask();
                          }
                        });
                  }))
              .scroll_target_layout()
              .frame({.max_width = infinity}))
          .scroll_position(position)
          .on_scroll_target_visibility_change(
              std::type_identity<int>{}, 0.5,
              [ask, count](const std::vector<int>& visible) {
                if (!visible.empty() &&
                    std::ranges::max(visible) >= count - prefetch_margin) {
                  ask();
                }
              })
          .frame({.max_width = infinity, .max_height = infinity})
          .id(feed_view)
          .refreshable([items, requested, refreshes] {
            return reload(items, requested, refreshes);
          })
          .task(composition::ElementKey{static_cast<std::int64_t>(
                    (refreshes.get() * 100) + requested.get())},
                [page = requested.get() - 1, refreshed = refreshes.get(),
                 items] { return load(page, refreshed, items); });
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             std::move(list))
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::leading});
}

}
