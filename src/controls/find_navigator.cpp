#include <arisa/controls/find_navigator.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/text_field.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>

namespace arisa::controls {

const composition::StateKey<std::size_t> FindNavigator::Content::current{
    "find-navigator-current", 0};
const composition::StateKey<std::string> FindNavigator::Content::replacement{
    "find-navigator-replacement", ""};

FindNavigator::FindNavigator(std::shared_ptr<TextEditingSession> session,
                             composition::Binding<std::string> query,
                             composition::Binding<bool> presented,
                             bool replaces)
    : Element(std::make_unique<Content>(std::move(session), std::move(query),
                                        std::move(presented), replaces)) {}

auto FindNavigator::matches(std::string_view text, std::string_view query)
    -> std::vector<Range> {
  std::vector<Range> found;
  if (query.empty() || query.size() > text.size()) {
    return found;
  }
  const auto folded = [](char byte) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(byte)));
  };
  for (std::size_t at = 0; at + query.size() <= text.size();) {
    const bool same = std::ranges::equal(text.substr(at, query.size()), query,
                                         [&folded](char one, char other) {
                                           return folded(one) == folded(other);
                                         });
    if (same) {
      found.push_back(Range{.lower = at, .upper = at + query.size()});
      at += query.size();
    } else {
      ++at;
    }
  }
  return found;
}

auto FindNavigator::status(std::size_t current, std::size_t count)
    -> std::string {
  if (count == 0) {
    return "0";
  }
  return std::format("{} of {}", std::min(current, count - 1) + 1, count);
}

auto FindNavigator::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto FindNavigator::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const composition::StateHandle<std::size_t> chosen = context.state(current);
  const composition::StateHandle<std::string> replaced =
      context.state(replacement);
  const std::shared_ptr<TextEditingSession> session = session_;
  const composition::Binding<std::string> query = query_;
  const auto visit = [session, query, chosen](int step) {
    const TextEditing model = session->model();
    const std::vector<Range> found = matches(model.text(), query.get());
    if (found.empty()) {
      return;
    }
    const auto count = static_cast<long long>(found.size());
    const long long next =
        ((static_cast<long long>(chosen.get()) + step) % count + count) % count;
    chosen.set(static_cast<std::size_t>(next));
    TextEditing edited = model;
    edited.select(found[static_cast<std::size_t>(next)]);
    session->commit(edited);
  };
  const auto replace = [session, query, chosen, replaced](bool all) {
    TextEditing edited = session->model();
    std::vector<Range> found = matches(edited.text(), query.get());
    if (found.empty()) {
      return;
    }
    if (!all) {
      edited.select(found[std::min(chosen.get(), found.size() - 1)]);
      edited.insert(replaced.get());
      session->commit(edited);
      return;
    }
    std::ranges::reverse(found);
    for (const Range& range : found) {
      edited.select(range);
      edited.insert(replaced.get());
    }
    session->commit(edited);
  };
  const std::size_t count =
      matches(session_->model().text(), query_.get()).size();
  std::vector<composition::Element> rows;
  rows.push_back(components::HStack(
      TextField(std::string{find_label}, query_)
          .frame({.max_width = composition::infinity})
          .id("find-field"),
      components::Text(status(chosen.get(), count)).id("find-status"),
      components::Button(std::string{previous_label}, [visit] { visit(-1); })
          .id("find-previous"),
      components::Button(std::string{next_label}, [visit] { visit(1); })
          .id("find-next"),
      components::Button(std::string{done_label}, [presented = presented_] {
        presented.set(false);
      }).id("find-done")));
  if (replaces_) {
    rows.push_back(components::HStack(
        TextField(std::string{replace_field_label},
                  composition::Binding<std::string>{replaced})
            .frame({.max_width = composition::infinity})
            .id("replace-field"),
        components::Button(std::string{replace_label},
                           [replace] { replace(false); })
            .id("replace-one"),
        components::Button(std::string{replace_all_label}, [replace] {
          replace(true);
        }).id("replace-all")));
  }
  std::vector<composition::Element> produced;
  produced.push_back(
      components::VStack(components::VStackOptions{}, std::move(rows))
          .id("find-navigator"));
  return produced;
}

}
