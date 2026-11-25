#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/dismiss_search_action.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/search_field.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <utility>

namespace arisa::controls {

Searchable::Searchable(composition::Element content,
                       composition::Binding<std::string> text,
                       composition::SearchFieldPlacement placement,
                       std::optional<std::string> prompt,
                       std::optional<Tokens> tokens)
    : Element([&] {
        const bool hosts = content.content().hosts_sidebar();
        return std::make_unique<Content>(
            std::make_shared<composition::PlaceholderContentView::Source>(
                std::move(content)),
            std::move(text), placement, std::move(prompt), std::move(tokens),
            hosts);
      }()) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto Searchable::presented(
    composition::Binding<bool> is_presented) && -> Searchable {
  edit<Content>().present_with(std::move(is_presented));
  return std::move(*this);
}

Searchable::Tracked::Tracked(
    composition::Element content,
    std::shared_ptr<composition::ScrollGeometryObserver> observer)
    : Element(std::make_unique<components::ZStack::Content>(
                  components::Alignment::top_leading),
              [&content] {
                std::vector<composition::Element> held;
                held.push_back(std::move(content));
                return held;
              }()) {
  declared().scrolling.nested_geometry = std::move(observer);
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto Searchable::Drawer::concealed(
    const composition::ScrollGeometry& geometry) noexcept -> bool {
  return geometry.content_offset.y + geometry.content_insets.top >
         drawer_reveal;
}

auto Searchable::Drawer::differs(const composition::ScrollGeometry&,
                                 const composition::ScrollGeometry& after) const
    -> bool {
  return concealed(after) != hidden_.get();
}

void Searchable::Drawer::notify(const composition::ScrollGeometry&,
                                const composition::ScrollGeometry& after) {
  const bool hide = concealed(after);
  if (hide != hidden_.get()) {
    motion::with_animation(motion::Animation::snappy(0.25),
                           [this, hide] { hidden_.set(hide); });
  }
}

Searchable::Searchable(std::unique_ptr<Completer> completer)
    : Element(std::move(completer)) {}

auto Searchable::complete_text(composition::Element label, std::string text)
    -> composition::Element {
  return Searchable{std::make_unique<Completer>(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(label)),
      std::move(text), nullptr)};
}

auto Searchable::complete_token(
    composition::Element label,
    std::shared_ptr<const composition::TagValue> token)
    -> composition::Element {
  return Searchable{std::make_unique<Completer>(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(label)),
      std::nullopt, std::move(token))};
}

auto Searchable::Completer::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Completer>();
}

auto Searchable::Completer::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const composition::SearchEnvironment::Completion& completion =
      context.environment(composition::SearchEnvironment::completion);
  std::vector<composition::Element> produced;
  produced.push_back(components::Button(
      [completion, text = text_, token = token_] {
        if (text.has_value()) {
          completion.complete(*text);
        } else if (token != nullptr) {
          completion.complete(*token);
        }
      },
      composition::PlaceholderContentView(label_)));
  return produced;
}

auto Searchable::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Searchable::Content::scoped(composition::SearchScopeActivation activation,
                                 bool active, bool typed) noexcept -> bool {
  switch (activation) {
    case composition::SearchScopeActivation::on_text_entry:
      return typed;
    case composition::SearchScopeActivation::automatic:
    case composition::SearchScopeActivation::on_search_presentation:
      return active;
  }
  return active;
}

auto Searchable::Content::offered() const
    -> std::optional<composition::Element> {
  if (!tokens_.has_value() || !tokens_->suggested_count) {
    return std::nullopt;
  }
  const std::size_t count = tokens_->suggested_count();
  if (count == 0) {
    return std::nullopt;
  }
  std::vector<composition::Element> chips;
  chips.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    chips.push_back(
        components::Button([choose = tokens_->choose, index] { choose(index); },
                           tokens_->suggested_label(index))
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  return composition::Element{
      components::HStack(components::HStackOptions{.spacing = theme::space::sm},
                         std::move(chips))
          .id("arisa.search.tokens")};
}

auto Searchable::Content::suggestions(
    composition::Context& context, bool active,
    composition::SearchSuggestionsPlacement mode) const
    -> std::optional<composition::Element> {
  if (!active ||
      !context
           .environment(composition::SearchEnvironment::suggestion_visibility)
           .allows(mode)) {
    return std::nullopt;
  }
  const std::shared_ptr<const composition::SearchEnvironment::Builder>&
      builder =
          context.environment(composition::SearchEnvironment::suggestions);
  std::optional<composition::Element> tokens = offered();
  if (builder == nullptr && !tokens.has_value()) {
    return std::nullopt;
  }
  std::vector<composition::Element> rows;
  if (tokens.has_value()) {
    rows.push_back(std::move(*tokens));
  }
  if (builder != nullptr && *builder) {
    rows.push_back((*builder)());
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(rows))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .environment(composition::SearchEnvironment::suggestions_placement,
                       mode)
          .id("arisa.search.suggestions")};
}

auto Searchable::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto state = context.state(searching);
  const std::optional<composition::Binding<bool>> presented = presented_;
  const bool active = presented.has_value() ? presented->get() : state.get();
  const auto activate = [state, presented](bool now) {
    if (presented.has_value() && presented->get() != now) {
      presented->set(now);
    }
    if (state.get() != now) {
      state.set(now);
    }
  };
  const bool typed = !text_.get().empty();
  const composition::SearchEnvironment::Activation& tab =
      context.environment(composition::SearchEnvironment::activation);
  const std::function<void()> clear_tokens =
      tokens_.has_value() ? tokens_->clear : std::function<void()>{};
  const composition::DismissSearchAction dismiss{
      state.identity(), [activate, text = text_, clear_tokens, tab] {
        text.set(std::string{});
        if (clear_tokens) {
          clear_tokens();
        }
        activate(false);
        tab.dismissed();
      }};
  std::optional<SearchField::Chips> chips;
  if (tokens_.has_value()) {
    chips = SearchField::Chips{.count = tokens_->count,
                               .label = tokens_->label,
                               .remove = tokens_->remove};
  }
  composition::Element field = SearchField(
      text_, SearchField::Options{.prompt = prompt_.value_or(
                                      std::string{SearchField::default_prompt}),
                                  .began = [activate] { activate(true); },
                                  .cleared = clear_tokens,
                                  .chips = std::move(chips)});
  if (const auto& focused =
          context.environment(composition::SearchEnvironment::focused)) {
    field = std::move(field).focused(*focused);
  }
  if (const auto& target =
          context.environment(composition::SearchEnvironment::focus_target);
      target != nullptr && *target) {
    field = std::move(field).focused((*target)());
  }
  if (typed) {
    field = std::move(field).on_command(
        components::CommandMapping::clear,
        std::string{components::CommandMapping::clear_label},
        [text = text_, clear_tokens] {
          text.set(std::string{});
          if (clear_tokens) {
            clear_tokens();
          }
        });
  }
  if (tokens_.has_value() && tokens_->count && tokens_->count() > 0) {
    field = std::move(field).on_command(
        components::CommandMapping::remove,
        std::string{components::CommandMapping::remove_label},
        [count = tokens_->count, remove = tokens_->remove] {
          const std::size_t held = count();
          if (held > 0 && remove) {
            remove(held - 1);
          }
        });
  }
  field = std::move(field).id("arisa.search.field");
  std::optional<composition::Element> cancel;
  if (active) {
    cancel = components::Button(std::string{cancel_label}, [dismiss] {
               dismiss();
             }).id("arisa.search.cancel");
  }
  using Kind = composition::SearchFieldPlacement::Kind;
  const bool in_toolbar = placement_.in_toolbar();
  const bool in_sidebar = placement_.kind() == Kind::sidebar && hosts_sidebar_;
  const bool drawer = placement_.kind() == Kind::navigation_bar_drawer &&
                      placement_.display_mode() ==
                          composition::SearchFieldPlacement::
                              NavigationBarDrawerDisplayMode::automatic;
  const auto concealed = context.state(drawer_hidden);
  const composition::SearchSuggestionsPlacement mode =
      in_toolbar ? composition::SearchSuggestionsPlacement::menu
                 : composition::SearchSuggestionsPlacement::content;
  const composition::SearchEnvironment::Completion completion{
      state.identity(),
      [text = text_, submit = composition::Submission::collect(
                         context, composition::SubmitTriggers::search)](
          const std::string& chosen) {
        text.set(chosen);
        submit();
      },
      [add = tokens_.has_value()
                 ? tokens_->add
                 : std::function<bool(const composition::TagValue&)>{},
       text = text_](const composition::TagValue& token) {
        if (add && add(token)) {
          text.set(std::string{});
        }
      }};
  std::vector<composition::Element> column;
  std::optional<composition::Element> toolbar_field;
  std::optional<composition::Element> sidebar_field;
  composition::Element bar =
      components::HStack(components::HStackOptions{.spacing = theme::space::sm},
                         std::move(field), std::move(cancel))
          .id("arisa.search.bar");
  if (in_toolbar) {
    const bool minimized =
        !active &&
        context.environment(composition::SearchEnvironment::toolbar_behavior) ==
            composition::SearchToolbarBehavior::minimize;
    if (minimized) {
      toolbar_field =
          components::Button([activate] { activate(true); },
                             components::Image::system_name("search").font(
                                 theme::Font::title3))
              .id("arisa.search.button");
    } else {
      toolbar_field = std::move(bar).frame({.width = toolbar_width});
      if (std::optional<composition::Element> offered =
              suggestions(context, active, mode)) {
        toolbar_field =
            std::move(*toolbar_field)
                .popover(composition::Binding<bool>{[] { return true; },
                                                    [dismiss](bool shown) {
                                                      if (!shown) {
                                                        dismiss();
                                                      }
                                                    }},
                         composition::PopoverAttachmentAnchor::rect(),
                         theme::Edge::top,
                         [held = std::make_shared<
                              composition::PlaceholderContentView::Source>(
                              std::move(*offered).environment(
                                  composition::SearchEnvironment::completion,
                                  completion))] {
                           return composition::Element{
                               composition::PlaceholderContentView(held)};
                         });
      }
    }
  } else if (in_sidebar) {
    sidebar_field = std::move(bar).frame({.max_width = composition::infinity});
  } else if (!(drawer && concealed.get() && !active)) {
    column.push_back(
        std::move(bar).frame({.max_width = composition::infinity}));
  }
  if (const auto& scopes =
          context.environment(composition::SearchEnvironment::scopes);
      scopes != nullptr && scopes->picker &&
      scoped(scopes->activation, active, typed)) {
    column.push_back(scopes->picker().id("arisa.search.scopes"));
  }
  if (!in_toolbar) {
    if (std::optional<composition::Element> offered =
            suggestions(context, active, mode)) {
      column.push_back(std::move(*offered).environment(
          composition::SearchEnvironment::completion, completion));
    }
  }
  composition::Element shown_content =
      composition::PlaceholderContentView(content_)
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .flex_shrink(1.0F)
          .environment(composition::SearchEnvironment::is_searching, active)
          .environment(composition::SearchEnvironment::dismiss_search, dismiss)
          .environment(composition::SearchEnvironment::completion, completion);
  if (sidebar_field.has_value()) {
    shown_content =
        std::move(shown_content)
            .environment(
                composition::SearchEnvironment::sidebar_field,
                std::make_shared<const composition::SearchEnvironment::Builder>(
                    [held = std::make_shared<
                         composition::PlaceholderContentView::Source>(
                         std::move(*sidebar_field))] {
                      return composition::Element{
                          composition::PlaceholderContentView(held)};
                    }));
  }
  if (drawer) {
    shown_content =
        Tracked(std::move(shown_content), std::make_shared<Drawer>(concealed));
  }
  column.push_back(std::move(shown_content).id("arisa.search.content"));
  composition::Element body =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          std::move(column))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  if (active) {
    body = std::move(body).on_exit_command(std::string{cancel_label},
                                           [dismiss] { dismiss(); });
    if (!in_toolbar &&
        context.environment(
            composition::SearchEnvironment::presentation_toolbar_behavior) ==
            composition::SearchPresentationToolbarBehavior::automatic) {
      body = std::move(body).toolbar_visibility(
          composition::Visibility::hidden,
          composition::ToolbarPlacement::navigation_bar);
    }
  }
  if (tab.active() && !active) {
    body = std::move(body).on_appear([activate] { activate(true); });
  }
  if (toolbar_field.has_value() &&
      !context.environment(composition::ToolbarRemoval::search_removed)) {
    body = std::move(body).toolbar(
        composition::ToolbarItem(
            placement_.kind() == Kind::toolbar_principal
                ? composition::ToolbarItemPlacement::principal
                : composition::ToolbarItemPlacement::primary_action,
            std::move(*toolbar_field))
            .standing_in(composition::ToolbarDefaultItemKind::search));
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

}
