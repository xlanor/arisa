#include <arisa/components/accessibility_tree.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/move_command_direction.hpp>

#include <algorithm>
#include <format>
#include <numeric>
#include <ranges>
#include <utility>
#include <variant>

namespace arisa::components {

auto AccessibilityElement::Action::perform(composition::Tree& tree) const
    -> bool {
  if (handler_.has_value()) {
    handler_->perform();
    return true;
  }
  if (target_.has_value()) {
    return tree.activate(*target_);
  }
  return false;
}

auto AccessibilityElement::large_content() const
    -> std::optional<composition::Element> {
  if (details_.large_content == nullptr || !*details_.large_content) {
    return std::nullopt;
  }
  return (*details_.large_content)();
}

auto AccessibilityElement::activate(composition::Tree& tree) const -> bool {
  for (const Action& action : details_.actions) {
    if (action.kind() == composition::AccessibilityActionKind::Kind::standard) {
      return action.perform(tree);
    }
  }
  return tree.activate(details_.anchor, details_.activation_point);
}

auto AccessibilityElement::adjust(
    composition::Tree& tree,
    composition::AccessibilityAdjustmentDirection direction) const -> bool {
  if (details_.adjust != nullptr && *details_.adjust) {
    (*details_.adjust)(direction);
    return true;
  }
  if (!details_.adjustable) {
    return false;
  }
  const composition::MoveCommandDirection toward =
      direction == composition::AccessibilityAdjustmentDirection::increment
          ? composition::MoveCommandDirection::right
          : composition::MoveCommandDirection::left;
  if (const auto found = tree.node(node_)) {
    for (const std::shared_ptr<composition::Command>& command :
         found->get().interactions().commands) {
      if (command->kind() == composition::Command::Kind::move &&
          command->handles(toward)) {
        command->perform(toward);
        return true;
      }
    }
  }
  return tree.steer(node_, toward);
}

auto AccessibilityElement::scroll(theme::Edge edge) const -> bool {
  if (details_.scroll == nullptr || !*details_.scroll) {
    return false;
  }
  (*details_.scroll)(edge);
  return true;
}

auto AccessibilityElement::zoom(
    composition::AccessibilityZoomGestureAction action) const -> bool {
  if (details_.zoom == nullptr || !*details_.zoom) {
    return false;
  }
  (*details_.zoom)(action);
  return true;
}

auto AccessibilityElement::description() const -> std::string {
  std::string text{label_};
  if (value_.has_value() && !value_->empty()) {
    text += std::format(", {}", *value_);
  }
  for (const std::string_view trait : traits_.names()) {
    text += std::format(", {}", trait);
  }
  if (details_.chart.has_value()) {
    text += std::format(", {}", details_.chart->description());
  }
  if (hint_.has_value() && !hint_->empty()) {
    text += std::format(". {}", *hint_);
  }
  if (details_.scroll_status.has_value() && !details_.scroll_status->empty()) {
    text += std::format(". {}", *details_.scroll_status);
  }
  return text;
}

AccessibilityTree::AccessibilityTree(const composition::Tree& tree)
    : tree_(tree), fallback_locale_(text::Locale::undetermined()) {}

auto AccessibilityTree::attachment(const composition::Node& node)
    -> const composition::AccessibilityAttachment* {
  return node.interactions().accessibility.get();
}

auto AccessibilityTree::hidden(const composition::Node& node) -> bool {
  const composition::AccessibilityAttachment* declared = attachment(node);
  if (declared != nullptr && declared->hidden) {
    return true;
  }
  return node.modifiers().opacity <= 0.0F &&
         (declared == nullptr || !declared->layer_content);
}

auto AccessibilityTree::wraps(const composition::Node& node) -> bool {
  const composition::AccessibilityAttachment* declared = attachment(node);
  return declared != nullptr && declared->layer.has_value() &&
         node.child_count() == 2;
}

auto AccessibilityTree::stands_alone(
    const composition::Node& node,
    const composition::AccessibilityDefaults& defaults) -> bool {
  if (const composition::AccessibilityAttachment* declared = attachment(node)) {
    if (declared->children.has_value()) {
      return *declared->children !=
             composition::AccessibilityChildBehavior::contain;
    }
    if (declared->label.has_value() || declared->heading.has_value()) {
      return true;
    }
  }
  return defaults.label.has_value() || !defaults.traits.empty() ||
         defaults.adjustable || node.content().activatable();
}

auto AccessibilityTree::language_of(const composition::Node& node,
                                    const Language& above) const -> Language {
  Language next = above;
  if (const auto locale = node.override_of(Environment::locale)) {
    next.locale = &locale->get();
  }
  if (const auto catalog = node.override_of(Environment::string_catalog);
      catalog.has_value() && catalog->get().has_value()) {
    next.catalog = &*catalog->get();
  }
  if (const composition::AccessibilityAttachment* declared = attachment(node);
      declared != nullptr && declared->speech.has_value()) {
    next.speech = declared->speech->over(next.speech);
  }
  return next;
}

auto AccessibilityTree::language_at(const composition::Node& node) const
    -> Language {
  std::vector<const composition::Node*> chain;
  std::optional<composition::NodeId> current = node.id();
  while (current.has_value()) {
    const auto found = tree_.get().node(*current);
    if (!found.has_value()) {
      break;
    }
    chain.push_back(&found->get());
    current = found->get().parent();
  }
  Language language{};
  for (const composition::Node* each : chain | std::views::reverse) {
    language = language_of(*each, language);
  }
  return language;
}

auto AccessibilityTree::resolve(const composition::AccessibilityText& text,
                                const Language& language) const -> std::string {
  return text.resolve(
      language.locale != nullptr ? *language.locale : fallback_locale_,
      language.catalog != nullptr ? *language.catalog : fallback_catalog_);
}

auto AccessibilityTree::resolve(
    const std::optional<composition::AccessibilityText>& text,
    const Language& language) const -> std::optional<std::string> {
  if (!text.has_value()) {
    return std::nullopt;
  }
  return resolve(*text, language);
}

auto AccessibilityTree::defaults(const composition::Node& node,
                                 const Language& language) const
    -> composition::AccessibilityDefaults {
  return node.content().localized_accessibility(
      language.locale != nullptr ? *language.locale : fallback_locale_,
      language.catalog != nullptr ? *language.catalog : fallback_catalog_);
}

auto AccessibilityTree::located(
    const composition::AccessibilityAttachment::Location& at,
    const composition::Rect& frame) -> composition::Point {
  if (const auto* unit = std::get_if<motion::UnitPoint>(&at.at)) {
    return composition::Point{.x = frame.x + (frame.width * unit->x),
                              .y = frame.y + (frame.height * unit->y)};
  }
  const auto& point = std::get<composition::Point>(at.at);
  return composition::Point{.x = frame.x + point.x, .y = frame.y + point.y};
}

auto AccessibilityTree::action_name(
    const composition::AccessibilityActionKind& kind,
    const std::string& resolved) -> std::string {
  using Kind = composition::AccessibilityActionKind::Kind;
  switch (kind.kind()) {
    case Kind::standard:
      return "Activate";
    case Kind::escape:
      return "Escape";
    case Kind::magic_tap:
      return "Magic tap";
    case Kind::remove:
      return "Delete";
    case Kind::show_menu:
      return "Show menu";
    case Kind::named:
      return resolved;
  }
  return resolved;
}

auto AccessibilityTree::Speaker::enter(const composition::Node& node,
                                       const Spoken& above)
    -> SpokenWalk::Step {
  const AccessibilityTree& owner = owner_;
  if (&node == &top_.get()) {
    return SpokenWalk::Step{.state = Spoken{.language = above.language},
                            .descend = true};
  }
  if (hidden(node)) {
    return SpokenWalk::Step{.state = Spoken{}, .descend = false};
  }
  const Language language = owner.language_of(node, above.language);
  const composition::AccessibilityAttachment* declared = attachment(node);
  if (declared != nullptr && declared->label_placeholder) {
    if (placeholder_.has_value() && !placeholder_->empty()) {
      words_.push_back(*placeholder_);
    }
    return SpokenWalk::Step{.state = Spoken{}, .descend = false};
  }
  if (declared != nullptr && declared->label.has_value()) {
    words_.push_back(owner.resolve(*declared->label, language));
    return SpokenWalk::Step{.state = Spoken{}, .descend = false};
  }
  const composition::AccessibilityDefaults made =
      owner.defaults(node, language);
  if (made.label.has_value() && !made.label->empty()) {
    words_.push_back(*made.label);
    return SpokenWalk::Step{.state = Spoken{}, .descend = false};
  }
  return SpokenWalk::Step{.state = Spoken{.language = language},
                          .descend = true};
}

auto AccessibilityTree::spoken(const composition::Node& node,
                               const Language& language,
                               std::optional<std::string> placeholder) const
    -> std::vector<std::string> {
  Speaker speaker{*this, node, std::move(placeholder)};
  SpokenWalk walk;
  walk.run(node, Spoken{.language = language}, speaker);
  return speaker.take();
}

auto AccessibilityTree::make(const composition::Node& node,
                             const Language& language) const
    -> AccessibilityElement {
  using composition::AccessibilityTrait;
  const composition::AccessibilityDefaults made = defaults(node, language);
  std::string label;
  if (made.label.has_value() && !made.label->empty()) {
    label = *made.label;
  } else {
    for (const std::string& word : spoken(node, language)) {
      if (!label.empty()) {
        label += ", ";
      }
      label += word;
    }
  }
  composition::AccessibilityTraits traits = made.traits;
  if (node.content().activatable() && traits.empty()) {
    traits = traits.with(AccessibilityTrait::is_button);
  }
  std::optional<std::string> identifier;
  if (node.key().has_value()) {
    identifier = node.key()->description();
  }
  std::optional<graphics::Path> outline;
  if (const std::optional<graphics::AnyShape>& shape =
          node.interactions().accessibility_shape;
      shape.has_value()) {
    outline = shape->path(node.frame());
  }
  AccessibilityElement::Details details;
  details.anchor = node.id();
  details.adjustable = made.adjustable;
  details.responds_to_user_interaction = node.content().activatable();
  details.speech = made.speech.has_value() ? made.speech->over(language.speech)
                                           : language.speech;
  AccessibilityElement element{
      node.id(),          node.frame(),          std::move(label), made.value,
      std::nullopt,       std::move(identifier), traits,           std::nullopt,
      std::move(outline), std::move(details)};
  if (const composition::AccessibilityAttachment* declared = attachment(node)) {
    overlay(element, *declared, language);
  }
  return element;
}

void AccessibilityTree::overlay(
    AccessibilityElement& element,
    const composition::AccessibilityAttachment& declared,
    const Language& language) const {
  using composition::AccessibilityTrait;
  AccessibilityElement::Details& details = element.details_;
  if (declared.label.has_value()) {
    element.label_ = resolve(*declared.label, language);
  }
  if (declared.value.has_value()) {
    element.value_ = resolve(*declared.value, language);
  }
  if (declared.hint.has_value()) {
    element.hint_ = resolve(*declared.hint, language);
  }
  if (declared.identifier.has_value()) {
    element.identifier_ = declared.identifier;
  }
  if (declared.heading.has_value()) {
    element.heading_ = declared.heading;
    element.traits_ = element.traits_.with(AccessibilityTrait::is_header);
  }
  element.traits_ =
      element.traits_.with(declared.added).without(declared.removed);
  if (!declared.input_labels.empty()) {
    details.input_labels.clear();
    for (const composition::AccessibilityText& each : declared.input_labels) {
      details.input_labels.push_back(resolve(each, language));
    }
  }
  if (declared.sort_priority.has_value()) {
    details.sort_priority = *declared.sort_priority;
  }
  for (const composition::AccessibilityAction& action : declared.actions) {
    const std::string named = action.kind().name().has_value()
                                  ? resolve(*action.kind().name(), language)
                                  : std::string{};
    details.actions.emplace_back(action_name(action.kind(), named),
                                 action.kind().kind(), std::nullopt, action,
                                 std::nullopt);
  }
  if (declared.adjustable != nullptr) {
    details.adjust = declared.adjustable;
  }
  if (declared.scroll != nullptr) {
    details.scroll = declared.scroll;
  }
  if (declared.zoom != nullptr) {
    details.zoom = declared.zoom;
  }
  if (declared.activation_point.has_value()) {
    details.activation_point =
        located(*declared.activation_point, element.frame_);
  }
  for (const composition::AccessibilityAttachment::Location& point :
       declared.drag_points) {
    details.drag_points.push_back(AccessibilityElement::Location{
        .point = located(point, element.frame_),
        .description = resolve(point.description, language).value_or("")});
  }
  for (const composition::AccessibilityAttachment::Location& point :
       declared.drop_points) {
    details.drop_points.push_back(AccessibilityElement::Location{
        .point = located(point, element.frame_),
        .description = resolve(point.description, language).value_or("")});
  }
  if (declared.responds_to_user_interaction.has_value()) {
    details.responds_to_user_interaction =
        *declared.responds_to_user_interaction;
  }
  details.ignores_invert_colors =
      details.ignores_invert_colors || declared.ignores_invert_colors;
  if (declared.direct_touch.has_value()) {
    details.direct_touch = declared.direct_touch;
  }
  if (declared.shows_large_content_viewer) {
    details.shows_large_content_viewer = true;
  }
  if (declared.large_content != nullptr) {
    details.large_content = declared.large_content;
  }
  details.rotors.insert(details.rotors.end(), declared.rotors.begin(),
                        declared.rotors.end());
  if (declared.rotor_entry.has_value()) {
    details.rotor_entry = declared.rotor_entry;
  }
  if (declared.labeled_pair.has_value()) {
    details.labeled_pair = declared.labeled_pair;
  }
  if (declared.linked_group.has_value()) {
    details.linked_group = declared.linked_group;
  }
  if (declared.chart.has_value()) {
    details.chart = declared.chart;
  }
  if (declared.scroll_status.has_value()) {
    details.scroll_status = resolve(*declared.scroll_status, language);
  }
  for (const composition::AccessibilityAttachment::CustomContent& content :
       declared.custom_content) {
    details.custom_content.push_back(AccessibilityElement::CustomContent{
        .id = content.key.id(),
        .label = resolve(content.key.label(), language),
        .value = resolve(content.value, language)});
  }
  if (declared.navigation_icon.has_value()) {
    details.navigation_icon = declared.navigation_icon;
  }
  if (declared.text_content_type.has_value()) {
    details.text_content_type = declared.text_content_type;
  }
}

auto AccessibilityTree::Collector::enter(const composition::Node& node,
                                         const Visit& above) -> Walk::Step {
  const AccessibilityTree& owner = owner_;
  const bool top = &node == &top_.get();
  if (hidden(node)) {
    return Walk::Step{.state = Visit{}, .descend = false};
  }
  const composition::AccessibilityAttachment* declared = attachment(node);
  if (!top && declared != nullptr && declared->layer_content) {
    return Walk::Step{.state = Visit{}, .descend = false};
  }
  const Language language = owner.language_of(node, above.language);
  if (wraps(node)) {
    add(owner.unit(node, language));
    return Walk::Step{.state = Visit{}, .descend = false};
  }
  if (stands_alone(node, owner.defaults(node, language))) {
    std::vector<AccessibilityElement> single;
    single.push_back(owner.make(node, language));
    add(std::move(single));
    return Walk::Step{.state = Visit{}, .descend = false};
  }
  const bool container =
      top ||
      (declared != nullptr &&
       declared->children == composition::AccessibilityChildBehavior::contain);
  return Walk::Step{
      .state = Visit{.language = language,
                     .begin = found_.size(),
                     .container = container,
                     .priority = declared != nullptr
                                     ? declared->sort_priority.value_or(0.0)
                                     : 0.0},
      .descend = true};
}

void AccessibilityTree::Collector::leave(const composition::Node&,
                                         Visit& visit) {
  if (!visit.container || found_.size() - visit.begin < 2) {
    if (visit.container) {
      std::fill(keys_.begin() + static_cast<std::ptrdiff_t>(visit.begin),
                keys_.end(), visit.priority);
    }
    return;
  }
  std::vector<std::size_t> order(found_.size() - visit.begin);
  std::iota(order.begin(), order.end(), visit.begin);
  std::ranges::stable_sort(order, [this](std::size_t left, std::size_t right) {
    return keys_[left] > keys_[right];
  });
  std::vector<AccessibilityElement> sorted;
  sorted.reserve(order.size());
  for (const std::size_t index : order) {
    sorted.push_back(std::move(found_[index]));
  }
  found_.erase(found_.begin() + static_cast<std::ptrdiff_t>(visit.begin),
               found_.end());
  for (AccessibilityElement& each : sorted) {
    found_.push_back(std::move(each));
  }
  std::fill(keys_.begin() + static_cast<std::ptrdiff_t>(visit.begin),
            keys_.end(), visit.priority);
}

void AccessibilityTree::Collector::add(std::vector<AccessibilityElement> made) {
  for (AccessibilityElement& each : made) {
    keys_.push_back(each.sort_priority());
    found_.push_back(std::move(each));
  }
}

auto AccessibilityTree::collect(const composition::Node& node,
                                const Language& language) const
    -> std::vector<AccessibilityElement> {
  Collector collector{*this, node};
  Walk walk;
  walk.run(node, Visit{.language = language}, collector);
  return collector.take();
}

auto AccessibilityTree::unit(const composition::Node& wrapper,
                             const Language& language) const
    -> std::vector<AccessibilityElement> {
  std::vector<std::pair<const composition::Node*, Language>> wrappers;
  const composition::Node* current = &wrapper;
  Language reached = language;
  while (wraps(*current)) {
    wrappers.emplace_back(current, reached);
    const composition::Node* host = &current->children()[1];
    if (wraps(*host)) {
      reached = language_of(*host, reached);
    }
    current = host;
  }
  std::vector<AccessibilityElement> elements = collect(*current, reached);
  for (const auto& [each, spoken_in] : wrappers | std::views::reverse) {
    elements = layered(std::move(elements), *each, spoken_in);
  }
  return elements;
}

auto AccessibilityTree::layered(std::vector<AccessibilityElement> base,
                                const composition::Node& wrapper,
                                const Language& language) const
    -> std::vector<AccessibilityElement> {
  using Layer = composition::AccessibilityAttachment::Layer;
  const composition::AccessibilityAttachment& declared = *attachment(wrapper);
  const composition::Node* layer = &wrapper.children()[0];
  const composition::Node& content = *layer;
  const composition::NodeId anchor =
      base.empty() ? wrapper.children()[1].id() : base.front().anchor();
  std::vector<AccessibilityElement> elements;
  switch (*declared.layer) {
    case Layer::representation:
    case Layer::children:
      elements = collect(content, language);
      for (AccessibilityElement& each : elements) {
        each.details_.anchor = anchor;
      }
      break;
    case Layer::label: {
      std::string placeholder;
      for (const AccessibilityElement& each : base) {
        if (!placeholder.empty()) {
          placeholder += ", ";
        }
        placeholder += each.label();
      }
      std::string label;
      for (const std::string& word :
           spoken(content, language, std::move(placeholder))) {
        if (!label.empty()) {
          label += ", ";
        }
        label += word;
      }
      elements = std::move(base);
      for (AccessibilityElement& each : elements) {
        each.label_ = label;
      }
      break;
    }
    case Layer::actions: {
      const composition::AccessibilityAttachment* grouped = attachment(content);
      std::optional<std::string> category;
      if (grouped != nullptr && grouped->action_category.has_value()) {
        if (grouped->action_category->is_edit()) {
          category = "Edit";
        } else if (grouped->action_category->name().has_value()) {
          category = resolve(*grouped->action_category->name(), language);
        }
      }
      std::vector<AccessibilityElement::Action> extra;
      for (const AccessibilityElement& made : collect(content, language)) {
        const auto own = std::ranges::find(
            made.actions(),
            composition::AccessibilityActionKind::Kind::standard,
            &AccessibilityElement::Action::kind);
        if (own != made.actions().end()) {
          extra.push_back(own->renamed(std::string{made.label()}, category));
        } else {
          extra.emplace_back(std::string{made.label()},
                             composition::AccessibilityActionKind::Kind::named,
                             category, std::nullopt, made.node());
        }
      }
      elements = std::move(base);
      for (AccessibilityElement& each : elements) {
        each.details_.actions.insert(each.details_.actions.end(), extra.begin(),
                                     extra.end());
      }
      break;
    }
  }
  for (AccessibilityElement& each : elements) {
    overlay(each, declared, language);
  }
  return elements;
}

void AccessibilityTree::finish(std::vector<AccessibilityElement>& elements) {
  for (AccessibilityElement& each : elements) {
    std::ranges::reverse(each.details_.actions);
  }
}

void AccessibilityTree::pair(
    std::vector<AccessibilityElement>& elements) const {
  for (AccessibilityElement& content : elements) {
    const auto& paired = content.details_.labeled_pair;
    if (!paired.has_value() ||
        paired->role != composition::AccessibilityLabeledPairRole::content ||
        !content.label_.empty()) {
      continue;
    }
    for (const AccessibilityElement& label : elements) {
      const auto& theirs = label.details_.labeled_pair;
      if (theirs.has_value() &&
          theirs->role == composition::AccessibilityLabeledPairRole::label &&
          theirs->pairing == paired->pairing) {
        content.label_ = label.label_;
        break;
      }
    }
  }
}

auto AccessibilityTree::elements() const -> std::vector<AccessibilityElement> {
  std::vector<AccessibilityElement> found =
      collect(tree_.get().root(), Language{});
  pair(found);
  finish(found);
  return found;
}

auto AccessibilityTree::elements(const composition::Node& within) const
    -> std::vector<AccessibilityElement> {
  std::vector<AccessibilityElement> found =
      collect(within, language_at(within));
  pair(found);
  finish(found);
  return found;
}

auto AccessibilityTree::element(composition::NodeId node) const
    -> std::optional<AccessibilityElement> {
  const auto found = tree_.get().node(node);
  if (!found.has_value() || hidden(found->get())) {
    return std::nullopt;
  }
  for (AccessibilityElement& each : elements()) {
    if (each.node() == node || each.anchor() == node) {
      return std::move(each);
    }
  }
  std::vector<AccessibilityElement> made;
  if (wraps(found->get())) {
    made = unit(found->get(), language_at(found->get()));
  }
  if (made.empty()) {
    made.push_back(make(found->get(), language_at(found->get())));
  }
  finish(made);
  return std::move(made.front());
}

auto AccessibilityTree::find(std::string_view identifier) const
    -> std::optional<AccessibilityElement> {
  for (AccessibilityElement& each : elements()) {
    if (each.identifier().has_value() && *each.identifier() == identifier) {
      return std::move(each);
    }
  }
  return std::nullopt;
}

auto AccessibilityTree::Gatherer::enter(const composition::Node& node,
                                        const bool&) -> GatherWalk::Step {
  if (hidden(node)) {
    return GatherWalk::Step{.state = false, .descend = false};
  }
  if (const composition::AccessibilityAttachment* declared = attachment(node)) {
    found_.insert(found_.end(), declared->rotors.begin(),
                  declared->rotors.end());
  }
  return GatherWalk::Step{.state = true, .descend = true};
}

auto AccessibilityTree::rotors() const
    -> std::vector<composition::AccessibilityRotor> {
  Gatherer gatherer;
  GatherWalk walk;
  walk.run(tree_.get().root(), true, gatherer);
  return gatherer.take();
}

auto AccessibilityTree::entries(const composition::AccessibilityRotor& rotor)
    const -> std::vector<composition::AccessibilityRotorEntry> {
  return rotor.entries();
}

auto AccessibilityTree::label(
    const composition::AccessibilityRotorEntry& entry) const -> std::string {
  if (entry.label().has_value()) {
    return resolve(*entry.label(), language_at(tree_.get().root()));
  }
  if (const auto found = target(entry)) {
    return std::string{found->label()};
  }
  return {};
}

auto AccessibilityTree::label(
    const composition::AccessibilityRotor& rotor) const -> std::string {
  if (const composition::AccessibilitySystemRotor* system = rotor.system()) {
    return system->name();
  }
  return resolve(std::get<composition::AccessibilityText>(rotor.label()),
                 language_at(tree_.get().root()));
}

auto AccessibilityTree::target(
    const composition::AccessibilityRotorEntry& entry) const
    -> std::optional<AccessibilityElement> {
  if (!entry.id().has_value()) {
    return std::nullopt;
  }
  if (entry.space().has_value()) {
    for (AccessibilityElement& each : elements()) {
      const auto& marked = each.rotor_entry();
      if (marked.has_value() && marked->id == *entry.id() &&
          marked->space == *entry.space()) {
        return std::move(each);
      }
    }
    return std::nullopt;
  }
  const std::optional<composition::NodeId> keyed =
      tree_.get().find(*entry.id());
  if (!keyed.has_value()) {
    return std::nullopt;
  }
  return element(*keyed);
}

auto AccessibilityTree::paired(const AccessibilityElement& element) const
    -> std::optional<AccessibilityElement> {
  const auto& mine = element.labeled_pair();
  if (!mine.has_value()) {
    return std::nullopt;
  }
  for (AccessibilityElement& each : elements()) {
    const auto& theirs = each.labeled_pair();
    if (theirs.has_value() && theirs->pairing == mine->pairing &&
        theirs->role != mine->role) {
      return std::move(each);
    }
  }
  return std::nullopt;
}

auto AccessibilityTree::linked(const AccessibilityElement& element) const
    -> std::vector<AccessibilityElement> {
  std::vector<AccessibilityElement> group;
  const auto& mine = element.linked_group();
  if (!mine.has_value()) {
    return group;
  }
  for (AccessibilityElement& each : elements()) {
    if (each.linked_group() == mine) {
      group.push_back(std::move(each));
    }
  }
  return group;
}

}
