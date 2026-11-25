#include <composition/node_lifecycle.hpp>
#include <composition/reconciler.hpp>

#include <arisa/async/job_priority.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/toolbar_item.hpp>

#include <arisa/composition/context.hpp>
#include <arisa/composition/error_boundary.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/retained.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

namespace arisa::composition {

auto Reconciler::Positions::next(ElementType type,
                                 const std::optional<ElementKey>& key)
    -> Position {
  if (!key.has_value()) {
    return Position{.type = type, .key = nullptr, .ordinal = unkeyed_++};
  }
  std::size_t& seen =
      keyed_[Position{.type = type, .key = &*key, .ordinal = 0}];
  return Position{.type = type, .key = &*key, .ordinal = seen++};
}

void Reconciler::Positions::reset() noexcept {
  unkeyed_ = 0;
  keyed_.clear();
}

void Reconciler::run(bool holding) {
  Tree& tree = tree_;
  holding_ = holding;
  evaluated_.clear();
  if (tree.candidate_.has_value()) {
    tree.pass_ = std::move(tree.candidate_);
    tree.candidate_.reset();
  }
  collect();
  do {
    while (const auto node = next_dirty()) {
      evaluate(node->get());
    }
  } while (contain());
  transplant();
  pair();
  zoom();
  notify();
  const bool retired = sweep();
  if (retired || !evaluated_.empty()) {
    ++tree.revision_;
  }
}

void Reconciler::collect() {
  Tree& tree = tree_;
  for (auto entry = tree.task_nodes_.begin();
       entry != tree.task_nodes_.end();) {
    const auto found = tree.lookup(*entry);
    if (!found.has_value()) {
      entry = tree.task_nodes_.erase(entry);
      continue;
    }
    Node& node = found->get();
    for (NodeTask& task : node.tasks_) {
      if (auto failure = task.settle()) {
        tree.failures_.push_back(
            Tree::Failure{.node = node.id_, .error = std::move(*failure)});
      }
    }
    if (node.running_tasks() == 0) {
      entry = tree.task_nodes_.erase(entry);
    } else {
      ++entry;
    }
  }
}

auto Reconciler::contain() -> bool {
  Tree& tree = tree_;
  bool rerouted = false;
  for (Tree::Failure& failure : std::exchange(tree.failures_, {})) {
    const auto failed = tree.lookup(failure.node);
    std::optional<NodeId> boundary;
    if (failed.has_value()) {
      boundary = nearest_boundary(failed->get());
    }
    if (boundary.has_value()) {
      Node& catcher = tree.lookup(*boundary)->get();
      if (!catcher.boundary_failure_.has_value()) {
        catcher.boundary_failure_ = failure.error;
        catcher.boundary_rebuild_ = true;
        tree.dirty_.insert(catcher.id_);
        evaluated_.erase(catcher.id_);
        rerouted = true;
      }
    }
    tree.reports_.push_back(FailureReport{.node = failure.node,
                                          .boundary = boundary,
                                          .error = std::move(failure.error)});
  }
  return rerouted;
}

auto Reconciler::nearest_boundary(const Node& node) -> std::optional<NodeId> {
  Tree& tree = tree_;
  std::optional<NodeId> parent = node.parent_;
  while (parent.has_value()) {
    const auto ancestor = tree.lookup(*parent);
    if (!ancestor.has_value()) {
      return std::nullopt;
    }
    const Node& candidate = ancestor->get();
    if (candidate.boundary() && candidate.active() &&
        (!candidate.boundary_failure_.has_value() ||
         candidate.boundary_rebuild_)) {
      return candidate.id_;
    }
    parent = candidate.parent_;
  }
  return std::nullopt;
}

auto Reconciler::children_for(Node& node, std::vector<Element> declared)
    -> std::vector<Element> {
  node.boundary_rebuild_ = false;
  if (!node.boundary() || !node.boundary_failure_.has_value()) {
    return declared;
  }
  std::vector<Element> fallback;
  fallback.push_back(static_cast<ErrorBoundary::Content&>(*node.content_)
                         .fallback(*node.boundary_failure_));
  return fallback;
}

void Reconciler::adopt_tasks(Node& node, std::vector<NodeTask> incoming) {
  bool changed = false;
  std::vector<NodeTask> merged;
  merged.reserve(incoming.size());
  for (std::size_t index = 0; index < incoming.size(); ++index) {
    if (index < node.tasks_.size() &&
        node.tasks_[index].key() == incoming[index].key()) {
      merged.push_back(std::move(node.tasks_[index]));
      continue;
    }
    merged.push_back(std::move(incoming[index]));
    changed = true;
  }
  node.tasks_ = std::move(merged);
  if (changed && node.phase() == NodeLifecyclePhase::live) {
    starting_.push_back(node.id_);
  }
}

void Reconciler::start_tasks() {
  Tree& tree = tree_;
  for (const NodeId id : std::exchange(starting_, {})) {
    const auto found = tree.lookup(id);
    if (!found.has_value() ||
        found->get().phase() != NodeLifecyclePhase::live) {
      continue;
    }
    Node& node = found->get();
    for (const async::JobPriority priority : async::all_job_priorities) {
      for (NodeTask& task : node.tasks_) {
        if (task.priority() == priority) {
          task.start(tree.scheduler());
        }
      }
    }
    if (node.running_tasks() > 0) {
      tree.task_nodes_.insert(id);
    }
  }
}

auto Reconciler::next_dirty() -> std::optional<std::reference_wrapper<Node>> {
  Tree& tree = tree_;
  std::optional<std::reference_wrapper<Node>> chosen;
  for (auto entry = tree.dirty_.begin(); entry != tree.dirty_.end();) {
    if (evaluated_.contains(*entry) ||
        (holding_ && tree.deferred_.contains(*entry))) {
      ++entry;
      continue;
    }
    const auto found = tree.lookup(*entry);
    if (!found.has_value() || !found->get().active()) {
      entry = tree.dirty_.erase(entry);
      continue;
    }
    if (!chosen.has_value() || found->get().depth() < chosen->get().depth()) {
      chosen = found;
    }
    ++entry;
  }
  return chosen;
}

auto Reconciler::top() const noexcept -> const motion::Transaction& {
  if (transactions_.empty()) {
    return tree_.get().none_;
  }
  return transactions_.back();
}

void Reconciler::scope(Node& node) {
  if (node.motion_->scoped_animation.has_value() &&
      node.transaction_.has_value() &&
      !node.transaction_->disables_animations()) {
    node.transaction_->set_animation(*node.motion_->scoped_animation);
  }
}

void Reconciler::evaluate(Node& node) {
  if (!node.parent_.has_value()) {
    evaluate_root(node);
    return;
  }
  start(node, Handoff{.arrival = Arrival::evaluate});
}

void Reconciler::evaluate_root(Node& root) {
  Tree& tree = tree_;
  motion::Transaction transaction = top();
  if (auto pending = tree.transactions_.extract(root.id_); !pending.empty()) {
    transaction = std::move(pending.mapped());
  }
  root.transaction_ = transaction;
  scope(root);
  tree.touched_.insert(root.id_);
  transactions_.push_back(std::move(transaction));
  evaluated_.insert(root.id_);
  tree.dirty_.erase(root.id_);
  if (tree.pending_.has_value()) {
    std::vector<Element> elements;
    elements.push_back(std::move(*tree.pending_));
    tree.pending_.reset();
    reconcile(root, std::move(elements));
  }
  if (tree.debug_changed_) {
    reconcile_debug(root);
  }
  if (!tree.overlay_requests_.empty()) {
    reconcile_overlays(root);
  }
  transactions_.pop_back();
}

void Reconciler::start(Node& node, Handoff handoff) {
  handoff_ = std::move(handoff);
  Walker walker{*this};
  walk_.run(node, Visit{}, walker);
}

auto Reconciler::Walker::enter(Node& node, const Visit&) -> Walk::Step {
  return reconciler_.get().open(node);
}

auto Reconciler::Walker::next(Node& node, Visit& visit) -> Node* {
  return reconciler_.get().advance(node, visit);
}

void Reconciler::Walker::leave(Node& node, Visit& visit) {
  reconciler_.get().close(node, visit);
}

auto Reconciler::open(Node& node) -> Walk::Step {
  Handoff handoff = std::exchange(handoff_, Handoff{});
  Walk::Step step{.state =
                      Visit{.owned = std::move(handoff.owned),
                            .parent = handoff.parent,
                            .adopt = handoff.adopt,
                            .returned = handoff.returned,
                            .inserted = handoff.inserted,
                            .arrival = handoff.arrival,
                            .transaction = std::move(handoff.transaction)}};
  Visit& visit = step.state;
  switch (handoff.arrival) {
    case Arrival::evaluate:
      evaluate_head(node, visit);
      break;
    case Arrival::update:
      update_head(node, std::move(*handoff.element), visit);
      break;
    case Arrival::revive: {
      Element& element = *handoff.element;
      visit.inserted = mounting_ == 0;
      ++mounting_;
      rekey(node, handoff.parent->id_, handoff.parent->depth_ + 1);
      tree_.get().label(
          node, std::move(element.key_),
          element.membership_.extract(&Element::MembershipDeclarations::alias));
      update_head(node, std::move(element), visit);
      break;
    }
    case Arrival::mount:
      if (node.content_->composes()) {
        evaluate_head(node, visit);
      } else {
        begin_reconcile(node, visit, std::move(handoff.element->children_));
      }
      break;
    case Arrival::deliver:
      visit.work = Work::delivering;
      break;
    case Arrival::forward:
      visit.work = Work::forwarding;
      visit.forwarded = std::move(handoff.element);
      break;
    case Arrival::reconcile:
      begin_reconcile(node, visit, std::move(handoff.elements));
      break;
    case Arrival::none:
      break;
  }
  step.descend = visit.work != Work::none;
  return step;
}

auto Reconciler::advance(Node& node, Visit& visit) -> Node* {
  switch (visit.work) {
    case Work::reconciling:
      return next_planned(node, visit);
    case Work::delivering:
      return next_delivery(node, visit);
    case Work::forwarding:
      if (visit.forwarded.has_value()) {
        handoff_ = Handoff{.arrival = Arrival::update,
                           .element = std::exchange(visit.forwarded, {})};
        return node.children_.front().get();
      }
      visit.work = Work::none;
      return nullptr;
    case Work::none:
      return nullptr;
  }
  return nullptr;
}

void Reconciler::close(Node& node, Visit& visit) {
  if (visit.evaluated) {
    transactions_.pop_back();
  }
  if (visit.updated) {
    transactions_.pop_back();
  }
  if (visit.arrival == Arrival::revive) {
    --mounting_;
    if (visit.inserted) {
      introduce(node, implicit(node, node.transaction_.value_or(top())));
    }
  } else if (visit.arrival == Arrival::mount) {
    transactions_.pop_back();
    --mounting_;
    if (visit.inserted) {
      introduce(node, implicit(node, *visit.transaction));
    }
  }
  if (visit.adopt) {
    visit.parent->children_.push_back(std::move(visit.owned));
  } else if (visit.returned) {
    returned_ = std::move(visit.owned);
  }
}

void Reconciler::evaluate_head(Node& node, Visit& visit) {
  Tree& tree = tree_;
  motion::Transaction transaction = top();
  if (auto pending = tree.transactions_.extract(node.id_); !pending.empty()) {
    transaction = std::move(pending.mapped());
  }
  node.transaction_ = transaction;
  scope(node);
  tree.touched_.insert(node.id_);
  transactions_.push_back(std::move(transaction));
  visit.evaluated = true;
  produce_head(node, visit);
}

void Reconciler::produce_head(Node& node, Visit& visit) {
  Tree& tree = tree_;
  evaluated_.insert(node.id_);
  tree.dirty_.erase(node.id_);
  std::vector<Element> elements;
  if (!node.content_->composes()) {
    if (node.content_->refreshes()) {
      Context context{tree, node};
      if (node.content_->refresh(context, node)) {
        tree.repaint(node);
      }
    }
    if (node.boundary_rebuild_) {
      begin_reconcile(node, visit, children_for(node, {}));
    }
    return;
  }
  node.stale_dependencies_ = std::exchange(node.dependencies_, {});
  node.environment_reads_.clear();
  node.highlight_reads_ = false;
  node.cost_.evaluated();
  if (debugging_ || tree.debugs(node)) {
    ++tree.debug_evaluations_;
  } else {
    ++tree.evaluations_;
  }
  {
    Context context{tree, node};
    elements = node.content_->body(context);
  }
  node.stale_dependencies_.clear();
  if (const auto held = node.content_as<PlaceholderContentView::Content>()) {
    held->get().shown_by(node.anchor_);
  }
  if (const auto source = placeholder(node);
      source.has_value() && elements.empty()) {
    if (node.children_.empty()) {
      waiting_.push_back(Waiting{.source = *source, .node = node.id_});
    }
    return;
  }
  begin_reconcile(node, visit, std::move(elements));
}

void Reconciler::reconcile_debug(Node& root) {
  Tree& tree = tree_;
  tree.debug_changed_ = false;
  std::optional<Element> wanted = std::exchange(tree.debug_content_, {});
  Node* existing = nullptr;
  if (tree.debug_layer_.has_value()) {
    for (const std::unique_ptr<Node>& child : root.children_) {
      if (child->id_ == *tree.debug_layer_ && child->active()) {
        existing = child.get();
      }
    }
  }
  debugging_ = true;
  if (wanted.has_value()) {
    if (existing != nullptr) {
      update(*existing, std::move(*wanted));
    } else {
      std::unique_ptr<Node> placed = place(root, std::move(*wanted));
      tree.debug_layer_ = placed->id_;
      root.children_.push_back(std::move(placed));
      tree.repaint(root);
    }
    debugging_ = false;
    return;
  }
  if (existing != nullptr) {
    begin_departure(*existing);
    tree.repaint(root);
  }
  tree.debug_layer_.reset();
  debugging_ = false;
}

auto Reconciler::overlay_source(const Node& root) const -> const Node* {
  const Tree& tree = tree_;
  if (root.children_.empty()) {
    return nullptr;
  }
  const Node& first = *root.children_.front();
  if (!first.active() || first.id_ == tree.debug_layer_ ||
      std::ranges::contains(std::views::values(tree.overlay_nodes_),
                            first.id_)) {
    return nullptr;
  }
  return &first;
}

void Reconciler::inherit_environment(Element& element, const Node& source) {
  Element::EnvironmentDeclarations& declared = element.environment_.edit();
  for (const EnvironmentOverride& entry : source.overrides_) {
    const bool transformed = std::ranges::contains(
        source.transforms_, entry.key(), &EnvironmentTransform::key);
    if (transformed || std::ranges::contains(declared.overrides, entry.key(),
                                             &EnvironmentOverride::key)) {
      continue;
    }
    EnvironmentOverride folded = entry;
    for (const EnvironmentTransform& inner :
         declared.transforms | std::views::reverse) {
      if (inner.key() == entry.key()) {
        folded = inner.apply(&folded.value());
      }
    }
    std::erase_if(declared.transforms,
                  [&entry](const EnvironmentTransform& inner) {
                    return inner.key() == entry.key();
                  });
    declared.overrides.push_back(std::move(folded));
  }
  for (const EnvironmentTransform& transform : source.transforms_) {
    if (!std::ranges::contains(declared.overrides, transform.key(),
                               &EnvironmentOverride::key)) {
      declared.transforms.push_back(transform);
    }
  }
}

void Reconciler::reconcile_overlays(Node& root) {
  Tree& tree = tree_;
  auto requests = std::exchange(tree.overlay_requests_, {});
  const Node* source = overlay_source(root);
  for (auto& [id, wanted, environment] : requests) {
    if (wanted.has_value() && source != nullptr &&
        environment == Tree::OverlayEnvironment::inherited) {
      inherit_environment(*wanted, *source);
    }
    Node* existing = nullptr;
    if (const auto found = tree.overlay_nodes_.find(id);
        found != tree.overlay_nodes_.end()) {
      for (const std::unique_ptr<Node>& child : root.children_) {
        if (child->id_ == found->second && child->active()) {
          existing = child.get();
        }
      }
    }
    if (wanted.has_value()) {
      if (existing != nullptr) {
        update(*existing, std::move(*wanted));
        continue;
      }
      std::unique_ptr<Node> placed = place(root, std::move(*wanted));
      tree.overlay_nodes_.insert_or_assign(id, placed->id_);
      root.children_.push_back(std::move(placed));
      tree.repaint(root);
      continue;
    }
    if (existing != nullptr) {
      begin_departure(*existing);
      tree.repaint(root);
    }
    tree.overlay_nodes_.erase(id);
  }
}

auto Reconciler::placeholder(const Node& node) -> std::optional<Identity> {
  const auto content = node.content_as<PlaceholderContentView::Content>();
  if (!content.has_value()) {
    return std::nullopt;
  }
  return Identity{&content->get().source()};
}

void Reconciler::park(
    Node& node, const std::shared_ptr<const DepartureHold::Token>& token) {
  const auto source = placeholder(node);
  if (!source.has_value() || node.children_.empty()) {
    return;
  }
  parked_.push_back(Parked{.source = *source,
                           .host = node.id_,
                           .token = token,
                           .children = std::exchange(node.children_, {})});
}

auto Reconciler::adopter(Identity source) -> std::optional<NodeId> {
  Tree& tree = tree_;
  for (const Waiting& waiting : waiting_) {
    if (waiting.source != source) {
      continue;
    }
    const auto found = tree.lookup(waiting.node);
    if (found.has_value() && found->get().active() &&
        found->get().children_.empty()) {
      return waiting.node;
    }
  }
  return std::nullopt;
}

void Reconciler::transplant() {
  Tree& tree = tree_;
  while (!parked_.empty()) {
    Parked parked = std::move(parked_.back());
    parked_.pop_back();
    if (const auto target = adopter(parked.source)) {
      Node& adopting = tree.lookup(*target)->get();
      for (const std::unique_ptr<Node>& child : parked.children) {
        reparent(*child, adopting.id_, adopting.depth_ + 1);
      }
      adopting.children_ = std::move(parked.children);
      tree.repaint_subtree(adopting);
      continue;
    }
    const auto host = tree.lookup(parked.host);
    if (!host.has_value()) {
      continue;
    }
    Node& vacated = host->get();
    vacated.children_ = std::move(parked.children);
    for (const std::unique_ptr<Node>& child : vacated.children_) {
      depart(*child, parked.token);
      escape(*child, parked.token);
    }
    tree.repaint_subtree(vacated);
  }
  waiting_.clear();
}

void Reconciler::reparent(Node& node, NodeId parent, std::size_t depth) {
  node.parent_ = parent;
  node.depth_ = depth;
  for (const std::unique_ptr<Node>& child : node.children_) {
    reparent(*child, node.id_, depth + 1);
  }
}

auto Reconciler::derive(Node& node, Element& element, bool mounted)
    -> motion::Transaction {
  motion::Transaction transaction = top();
  if (!mounted) {
    const std::vector<ImplicitAnimation>& held = node.motion_->animations;
    const std::size_t compared =
        std::min(element.motion_->animations.size(), held.size());
    for (std::size_t index = 0; index < compared; ++index) {
      if (!element.motion_->animations[index].same_value(held[index])) {
        element.motion_.edit().animations[index].apply(transaction);
      }
    }
  }
  enlist(node, std::move(element.motion_), mounted);
  if (mounted && node.motion_->zoom.has_value()) {
    zooms_.push_back(node.id_);
  }
  if (element.lifecycle_.engaged() && element.lifecycle_->transaction) {
    element.lifecycle_.edit().transaction(transaction);
  }
  node.transaction_ = transaction;
  scope(node);
  tree_.get().touched_.insert(node.id_);
  return transaction;
}

void Reconciler::observe(
    Node& node, std::vector<std::unique_ptr<ChangeObserver>> incoming) {
  for (std::size_t index = 0;
       index < incoming.size() && index < node.changes_.size(); ++index) {
    if (incoming[index]->differs(*node.changes_[index])) {
      changes_.push_back(Change{.node = node.id_,
                                .index = index,
                                .previous = std::move(node.changes_[index])});
    }
  }
  node.changes_ = std::move(incoming);
}

void Reconciler::sense(const Node& node, const Interactions& incoming) {
  const std::vector<Interactions::Feedback>& previous =
      node.interactions_->feedback;
  for (std::size_t index = 0;
       index < incoming.feedback.size() && index < previous.size(); ++index) {
    const Interactions::Feedback& next = incoming.feedback[index];
    if (next.origin != previous[index].origin) {
      continue;
    }
    if (const std::optional<SensoryFeedback> played =
            next.trigger->resolve(*previous[index].trigger)) {
      tree_.get().request_feedback(node.id(), *played, next.origin);
    }
  }
}

void Reconciler::update(Node& node, Element element) {
  start(node,
        Handoff{.arrival = Arrival::update, .element = std::move(element)});
}

void Reconciler::update_head(Node& node, Element element, Visit& visit) {
  if (node.alias_ != element.membership_->alias) {
    tree_.get().label(
        node, node.key_,
        element.membership_.extract(&Element::MembershipDeclarations::alias));
  }
  if (element.content_->refreshes()) {
    Context context{tree_.get(), node};
    element.content_->refresh(context, node);
  }
  if (node.fresh_) {
    appearing_.push_back(node.id_);
    transactions_.push_back(derive(node, element, true));
    node.changes_ =
        element.lifecycle_.extract(&Element::LifecycleDeclarations::changes);
    for (std::size_t index = 0; index < node.changes_.size(); ++index) {
      if (node.changes_[index]->initial()) {
        changes_.push_back(
            Change{.node = node.id_, .index = index, .previous = nullptr});
      }
    }
    if (!element.lifecycle_->tasks.empty()) {
      starting_.push_back(node.id_);
    }
  } else {
    transactions_.push_back(derive(node, element, false));
    observe(node, element.lifecycle_.extract(
                      &Element::LifecycleDeclarations::changes));
    sense(node, *element.interactions_);
    if (node.interactions_->covered != element.interactions_->covered) {
      coverings_.push_back(Covering{.node = node.id_,
                                    .covered = element.interactions_->covered});
    }
  }
  visit.updated = true;
  refresh_head(node, std::move(element), visit);
}

auto Reconciler::repaints(const Node& node, const Element& element) -> bool {
  const Interactions& held = *node.interactions_;
  const Interactions& incoming = *element.interactions_;
  return node.modifiers_ != element.modifiers_ ||
         held.hit_shape != incoming.hit_shape ||
         held.hit_path != incoming.hit_path ||
         held.hit_fill != incoming.hit_fill ||
         held.focus_shape != incoming.focus_shape ||
         held.focus_ring_inset != incoming.focus_ring_inset ||
         held.scrolling.container != incoming.scrolling.container ||
         held.scrolling.clip_disabled != incoming.scrolling.clip_disabled ||
         held.scrolling.content_margins != incoming.scrolling.content_margins ||
         held.scrolling.indicator_margins !=
             incoming.scrolling.indicator_margins ||
         held.scrolling.ignores_safe_area !=
             incoming.scrolling.ignores_safe_area ||
         held.scrolling.safe_area_bar != incoming.scrolling.safe_area_bar ||
         !held.effects.empty() || !incoming.effects.empty() ||
         !node.content_->paints_like(*element.content_);
}

void Reconciler::refresh_head(Node& node, Element element, Visit& visit) {
  if (node.fresh_ || repaints(node, element)) {
    tree_.get().repaint(node);
  }
  const bool fresh = std::exchange(node.fresh_, false);
  Element::LifecycleDeclarations lifecycle = element.lifecycle_.take();
  Element::MembershipDeclarations membership = element.membership_.take();
  node.hooks_ = std::move(lifecycle.hooks);
  node.modifiers_ = std::move(element.modifiers_);
  node.container_values_ = std::move(membership.container_values);
  node.container_values_.set_tag(membership.tag);
  const std::size_t carried = std::min(node.interactions_->geometry.size(),
                                       element.interactions_->geometry.size());
  for (std::size_t index = 0; index < carried; ++index) {
    element.interactions_->geometry[index]->adopt(
        *node.interactions_->geometry[index]);
  }
  node.interactions_ = std::move(element.interactions_);
  if (!fresh && !node.interactions_->effects.projections.empty()) {
    tree_.get().reproject(node.id_);
  }
  hold_exclusive(node);
  node.transforms_ = element.environment_.extract(
      &Element::EnvironmentDeclarations::transforms);
  std::vector<EnvironmentOverride> incoming = element.environment_.extract(
      &Element::EnvironmentDeclarations::overrides);
  for (EnvironmentOverride& made :
       tree_.get().transformed(node, node.transforms_)) {
    incoming.push_back(std::move(made));
  }
  if (fresh) {
    node.tasks_ = std::move(lifecycle.tasks);
    node.overrides_ = std::move(incoming);
    prime(node);
  } else {
    adopt_tasks(node, std::move(lifecycle.tasks));
    apply(node, std::move(incoming));
  }
  prefer(node, declared(node, element),
         element.environment_.extract(
             &Element::EnvironmentDeclarations::preference_observers),
         fresh);
  if (!node.content_->composes() || fresh) {
    node.content_ = std::move(element.content_);
  }
  tree_.get().enroll(node);
  if (!node.content_->composes()) {
    begin_reconcile(node, visit,
                    children_for(node, std::move(element.children_)));
    return;
  }
  const bool changed = fresh || !node.content_->same_as(*element.content_);
  bool forwarded = false;
  if (changed && !fresh) {
    node.content_ = std::move(element.content_);
  } else if (!changed) {
    forwarded = node.content_->adopt(*element.content_);
  }
  const bool dirty = tree_.get().dirty_.contains(node.id_);
  if ((changed || dirty) && !evaluated_.contains(node.id_)) {
    evaluate_head(node, visit);
  } else if (forwarded) {
    visit.work = Work::delivering;
  }
}

void Reconciler::hold_exclusive(Node& node) {
  const std::optional<ExclusiveFrame::Options>& wanted =
      node.interactions_->exclusive_frame;
  if (!wanted.has_value() || node.phase() == NodeLifecyclePhase::departing) {
    node.exclusive_.reset();
    return;
  }
  if (node.exclusive_.has_value() &&
      std::ranges::equal(node.exclusive_->keep(), wanted->keep)) {
    return;
  }
  node.exclusive_ = tree_.get().exclusive_frame(*wanted, node.id_);
}

auto Reconciler::next_delivery(Node& node, Visit& visit) -> Node* {
  Tree& tree = tree_;
  while (visit.position < node.children_.size()) {
    Node& child = *node.children_[visit.position++];
    if (!child.active()) {
      continue;
    }
    const auto held = child.content_as<PlaceholderContentView::Content>();
    if (!held.has_value() || !held->get().source().pending()) {
      handoff_ = Handoff{.arrival = Arrival::deliver};
      return &child;
    }
    if (evaluated_.contains(child.id_)) {
      tree.dirty_.insert(child.id_);
      continue;
    }
    if (child.children_.size() != 1 || tree.dirty_.contains(child.id_) ||
        !child.children_.front()->active()) {
      handoff_ = Handoff{.arrival = Arrival::evaluate};
      return &child;
    }
    std::optional<Element> element = held->get().take();
    if (!element.has_value()) {
      continue;
    }
    const Node& only = *child.children_.front();
    if (element->content_->kind() != only.kind() ||
        element->key_ != only.key_) {
      std::vector<Element> produced;
      produced.push_back(std::move(*element));
      evaluated_.insert(child.id_);
      handoff_ = Handoff{.arrival = Arrival::reconcile,
                         .elements = std::move(produced)};
      return &child;
    }
    handoff_ =
        Handoff{.arrival = Arrival::forward, .element = std::move(element)};
    return &child;
  }
  visit.work = Work::none;
  return nullptr;
}

auto Reconciler::mount_head(Node& parent, Element element) -> Handoff {
  Tree& tree = tree_;
  const bool inserted = mounting_ == 0;
  ++mounting_;
  auto node = std::make_unique<Node>(
      tree, tree.allocate(), std::move(element.content_),
      std::move(element.key_),
      element.membership_.extract(&Element::MembershipDeclarations::alias),
      parent.id_, parent.depth_ + 1);
  appearing_.push_back(node->id_);
  const motion::Transaction transaction = derive(*node, element, true);
  transactions_.push_back(transaction);
  Element::LifecycleDeclarations lifecycle = element.lifecycle_.take();
  Element::MembershipDeclarations membership = element.membership_.take();
  node->changes_ = std::move(lifecycle.changes);
  for (std::size_t index = 0; index < node->changes_.size(); ++index) {
    if (node->changes_[index]->initial()) {
      changes_.push_back(
          Change{.node = node->id_, .index = index, .previous = nullptr});
    }
  }
  node->hooks_ = std::move(lifecycle.hooks);
  node->modifiers_ = std::move(element.modifiers_);
  node->container_values_ = std::move(membership.container_values);
  node->container_values_.set_tag(membership.tag);
  node->interactions_ = std::move(element.interactions_);
  node->tasks_ = std::move(lifecycle.tasks);
  if (!node->tasks_.empty()) {
    starting_.push_back(node->id_);
  }
  node->overrides_ = element.environment_.extract(
      &Element::EnvironmentDeclarations::overrides);
  node->transforms_ = element.environment_.extract(
      &Element::EnvironmentDeclarations::transforms);
  for (EnvironmentOverride& made : tree.transformed(*node, node->transforms_)) {
    node->overrides_.push_back(std::move(made));
  }
  prefer(*node, declared(*node, element),
         element.environment_.extract(
             &Element::EnvironmentDeclarations::preference_observers),
         true);
  tree.enroll(*node);
  prime(*node);
  if (node->content_->refreshes()) {
    Context context{tree, *node};
    node->content_->refresh(context, *node);
  }
  return Handoff{.arrival = Arrival::mount,
                 .element = std::move(element),
                 .owned = std::move(node),
                 .parent = &parent,
                 .inserted = inserted,
                 .transaction = transaction};
}

void Reconciler::prime(Node& node) const {
  using Property = motion::Presentation::Property;
  const GeometryEffects& effects = node.modifiers_->effects;
  const std::chrono::nanoseconds now = tree_.get().frame_time();
  motion::Presentation& shown = node.presentation_.edit();
  shown.prime(Property::scale,
              motion::AnimatableVector{effects.scale_x, effects.scale_y}, now);
  shown.prime(Property::offset,
              motion::AnimatableVector{effects.offset_x, effects.offset_y},
              now);
  shown.prime(Property::rotation,
              motion::AnimatableVector{effects.rotation.radians()}, now);
  shown.prime(Property::opacity,
              motion::AnimatableVector{node.modifiers_->opacity}, now);
}

auto Reconciler::carrier(Node& root) -> Node& {
  std::reference_wrapper<Node> current = root;
  while (!current.get().motion_->transition.has_value() &&
         current.get().content_->composes() &&
         current.get().children_.size() == 1) {
    current = *current.get().children_.front();
  }
  if (current.get().motion_->transition.has_value()) {
    return current.get();
  }
  return root;
}

auto Reconciler::implicit(const Node& node, motion::Transaction transaction)
    -> motion::Transaction {
  if (!transaction.animation().has_value() &&
      !transaction.disables_animations() &&
      node.motion_->transition_animation.has_value()) {
    transaction.set_animation(node.motion_->transition_animation);
  }
  return transaction;
}

auto Reconciler::attaches(Node& root) -> bool {
  Node& target = carrier(root);
  if (layered(target)) {
    return std::ranges::any_of(
        target.children_, [](const std::unique_ptr<Node>& child) {
          return child->motion_->transition.has_value() &&
                 child->motion_->transition->attached_animation().has_value();
        });
  }
  return target.motion_->transition.has_value() &&
         target.motion_->transition->attached_animation().has_value();
}

auto Reconciler::layered(const Node& node) noexcept -> bool {
  return node.content_as<Layer::Content>().has_value() &&
         !node.motion_->transition.has_value();
}

void Reconciler::introduce(Node& root, const motion::Transaction& transaction) {
  if (!transaction.animation().has_value() &&
      (transaction.disables_animations() || !attaches(root))) {
    return;
  }
  Node& target = carrier(root);
  if (layered(target)) {
    for (const std::unique_ptr<Node>& child : target.children_) {
      const motion::Transition transition =
          child->motion_->transition.value_or(motion::Transition::opacity());
      if (transition != motion::Transition::identity() &&
          motion::Presentation::carried(transition, transaction).has_value()) {
        child->presentation_.edit().insert(transition, transaction);
        tree_.get().repaint(*child);
      }
    }
    return;
  }
  const motion::Transition transition =
      target.motion_->transition.value_or(motion::Transition::opacity());
  if (transition == motion::Transition::identity() ||
      !motion::Presentation::carried(transition, transaction).has_value()) {
    return;
  }
  target.presentation_.edit().insert(transition, transaction);
  tree_.get().repaint(target);
}

void Reconciler::hold(
    Node& target, const motion::Transition& transition,
    const motion::Transaction& transaction,
    const std::shared_ptr<const DepartureHold::Token>& token) {
  target.presentation_.edit().remove(transition, transaction);
  tree_.get().repaint(target);
  target.transition_hold_.emplace(token);
  tree_.get().transition_holds_.push_back(target.id_);
}

auto Reconciler::withdraw(
    Node& root, const motion::Transaction& incoming,
    const std::shared_ptr<const DepartureHold::Token>& token) -> bool {
  const motion::Transaction transaction = implicit(root, incoming);
  Node& target = carrier(root);
  if (layered(target)) {
    bool held = false;
    for (const std::unique_ptr<Node>& child : target.children_) {
      const motion::Transition transition =
          child->motion_->transition.value_or(motion::Transition::opacity());
      if (transition != motion::Transition::identity() &&
          motion::Presentation::carried(transition, transaction).has_value()) {
        hold(*child, transition, transaction, token);
        held = true;
      }
    }
    return held;
  }
  const motion::Transition transition =
      target.motion_->transition.value_or(motion::Transition::opacity());
  if (transition == motion::Transition::identity() ||
      !motion::Presentation::carried(transition, transaction).has_value()) {
    return false;
  }
  hold(target, transition, transaction, token);
  return true;
}

auto Reconciler::escape(
    Node& node, const std::shared_ptr<const DepartureHold::Token>& token)
    -> bool {
  bool held = false;
  for (const std::unique_ptr<Node>& child : node.children_) {
    if (child->content_as<Layer::Content>().has_value() &&
        withdraw(*child, top(), token)) {
      held = true;
    }
    held = escape(*child, token) || held;
  }
  return held;
}

auto Reconciler::lookup(const Node& node, Identity key) const
    -> const EnvironmentOverride::Value* {
  const auto own =
      std::ranges::find(node.overrides_, key, &EnvironmentOverride::key);
  if (own != node.overrides_.end()) {
    return &own->value();
  }
  return tree_.get().inherited(node, key);
}

void Reconciler::host_swipes(const Node& parent,
                             std::vector<Element>& elements) const {
  if (std::ranges::none_of(elements, &Element::has_swipe_actions)) {
    return;
  }
  const EnvironmentOverride::Value* enabled =
      lookup(parent, ListEnvironment::swipe_actions_container.identity());
  if (enabled == nullptr ||
      !static_cast<const EnvironmentOverride::Typed<bool>&>(*enabled).get()) {
    return;
  }
  const EnvironmentOverride::Value* found =
      lookup(parent, ListEnvironment::swipe_host.identity());
  if (found == nullptr) {
    return;
  }
  const SwipeHost* host =
      static_cast<const EnvironmentOverride::Typed<const SwipeHost*>&>(*found)
          .get();
  if (host == nullptr) {
    return;
  }
  for (Element& element : elements) {
    if (element.has_swipe_actions()) {
      const std::shared_ptr<const ListRow> row = element.list_row();
      element = host->wrap(std::move(element), *row);
    }
  }
}

void Reconciler::reconcile(Node& parent, std::vector<Element> elements) {
  start(parent, Handoff{.arrival = Arrival::reconcile,
                        .elements = std::move(elements)});
}

void Reconciler::begin_reconcile(Node& parent, Visit& visit,
                                 std::vector<Element> elements) {
  for (Element& element : elements) {
    element.distribute_decoration();
  }
  host_swipes(parent, elements);
  if (depth_ == scratch_.size()) {
    scratch_.emplace_back();
  }
  visit.work = Work::reconciling;
  visit.depth = depth_;
  visit.planned = 0;
  visit.elements = std::move(elements);
  Scratch& here = scratch_[depth_];
  ++depth_;
  here.matches.clear();
  here.existing.reset();
  here.incoming.reset();
  std::vector<std::unique_ptr<Node>>& previous = here.previous;
  previous.clear();
  previous.swap(parent.children_);
  here.order.clear();
  for (const std::unique_ptr<Node>& child : previous) {
    here.order.push_back(child.get());
  }
  for (std::size_t index = 0; index < previous.size(); ++index) {
    const Node& child = *previous[index];
    if (child.active()) {
      here.matches.emplace(here.existing.next(child.kind(), child.key_), index);
    }
  }
  plan(here, visit.elements);
  std::vector<std::unique_ptr<Node>>& next = parent.children_;
  next.clear();
  next.reserve(here.planned.size() + previous.size());
}

auto Reconciler::next_planned(Node& parent, Visit& visit) -> Node* {
  Tree& tree = tree_;
  Scratch& here = scratch_[visit.depth];
  std::vector<std::unique_ptr<Node>>& previous = here.previous;
  std::vector<std::unique_ptr<Node>>& next = parent.children_;
  while (visit.planned < here.planned.size()) {
    Planned& entry = here.planned[visit.planned++];
    if (entry.kept != nullptr) {
      next.push_back(std::move(entry.kept));
      continue;
    }
    Element& element =
        entry.built.has_value() ? *entry.built : visit.elements[entry.source];
    const Position position = *entry.position;
    if (position.key != nullptr && position.ordinal > 0) {
      tree.duplicate_keys_.push_back(
          DuplicateKey{.parent = parent.id_, .key = *position.key});
    }
    const auto match = here.matches.find(position);
    if (match != here.matches.end() && previous[match->second] != nullptr) {
      handoff_ = Handoff{.arrival = Arrival::update,
                         .element = std::move(element),
                         .owned = std::move(previous[match->second]),
                         .parent = &parent,
                         .adopt = true};
      return handoff_.owned.get();
    }
    handoff_ = arrive(parent, std::move(element));
    handoff_.adopt = true;
    return handoff_.owned.get();
  }
  finish_reconcile(parent, visit);
  visit.work = Work::none;
  return nullptr;
}

void Reconciler::finish_reconcile(Node& parent, Visit& visit) {
  Tree& tree = tree_;
  Scratch& here = scratch_[visit.depth];
  std::vector<std::unique_ptr<Node>>& previous = here.previous;
  std::vector<std::unique_ptr<Node>>& next = parent.children_;
  for (std::size_t index = 0; index < previous.size(); ++index) {
    if (previous[index] == nullptr) {
      continue;
    }
    if (previous[index]->active()) {
      begin_departure(*previous[index]);
    }
    const auto offset =
        static_cast<std::ptrdiff_t>(std::min(index, next.size()));
    next.insert(std::next(next.begin(), offset), std::move(previous[index]));
  }
  previous.clear();
  if (!std::ranges::equal(here.order, next, {}, {},
                          &std::unique_ptr<Node>::get)) {
    tree.repaint(parent);
    if (parent.preferring_) {
      tree.touch_preferences(parent);
    }
  }
  --depth_;
}

void Reconciler::plan(Scratch& here, std::vector<Element>& elements) {
  std::vector<Planned>& planned = here.planned;
  planned.clear();
  here.kept_from = 0;
  std::size_t total = 0;
  for (const Element& element : elements) {
    total +=
        element.type() == ElementType::of<Retained::Content>()
            ? static_cast<const Retained::Content&>(*element.content_).count()
            : 1;
  }
  planned.reserve(total);
  for (std::size_t source = 0; source < elements.size(); ++source) {
    Element& element = elements[source];
    if (element.type() != ElementType::of<Retained::Content>()) {
      planned.push_back(Planned{.kept = nullptr,
                                .built = std::nullopt,
                                .source = source,
                                .position = here.incoming.next(
                                    element.content_->kind(), element.key_)});
      continue;
    }
    const auto& run = static_cast<const Retained::Content&>(*element.content_);
    for (std::size_t index = 0; index < run.count(); ++index) {
      Planned& entry = planned.emplace_back();
      entry.source = source;
      if (const auto found = keep(here, run.key(index)); found.has_value()) {
        entry.kept = std::move(here.previous[*found]);
        continue;
      }
      entry.built = run.build(index);
      entry.position =
          here.incoming.next(entry.built->content_->kind(), entry.built->key_);
    }
  }
}

auto Reconciler::keep(Scratch& here, const ElementKey& key)
    -> std::optional<std::size_t> {
  const std::vector<std::unique_ptr<Node>>& previous = here.previous;
  const std::size_t count = previous.size();
  if (count == 0) {
    return std::nullopt;
  }
  for (std::size_t step = 0; step < count; ++step) {
    const std::size_t index = (here.kept_from + step) % count;
    const std::unique_ptr<Node>& child = previous[index];
    if (child != nullptr && child->active() && child->key_ == key) {
      here.kept_from = index + 1;
      return index;
    }
  }
  return std::nullopt;
}

void Reconciler::begin_departure(Node& root) {
  if (root.preferring_ && root.parent_.has_value()) {
    if (const auto parent = tree_.get().lookup(*root.parent_)) {
      tree_.get().touch_preferences(parent->get());
    }
  }
  auto token = std::make_shared<const DepartureHold::Token>();
  root.departure_ = token;
  tree_.get().departing_roots_.push_back(root.id_);
  depart(root, token);
  const bool animated = withdraw(root, top(), token);
  if (escape(root, token) && !animated && !root.hooks_.depart) {
    root.ghost_ = true;
  }
  tree_.get().repaint_subtree(root);
}

void Reconciler::enlist(Node& node,
                        support::Sparse<MotionDeclarations> incoming,
                        bool mounted) {
  if (node.motion_->matched != incoming->matched) {
    unlist(node);
    tree_.get().repaint(node);
  }
  node.motion_ = std::move(incoming);
  if (!node.motion_->matched.has_value() || !node.motion_->matched->is_source) {
    return;
  }
  tree_.get().sources_.insert_or_assign(node.motion_->matched->key, node.id_);
  if (mounted) {
    arrivals_.push_back(node.id_);
  }
}

void Reconciler::unlist(const Node& node) {
  if (!node.motion_->matched.has_value() || !node.motion_->matched->is_source) {
    return;
  }
  auto& sources = tree_.get().sources_;
  if (const auto found = sources.find(node.motion_->matched->key);
      found != sources.end() && found->second == node.id_) {
    sources.erase(found);
  }
}

void Reconciler::zoom() {
  Tree& tree = tree_;
  for (const NodeId id : std::exchange(zooms_, {})) {
    const auto arrival = tree.lookup(id);
    if (!arrival.has_value()) {
      continue;
    }
    Node& incoming = arrival->get();
    if (!incoming.motion_->zoom.has_value() ||
        !incoming.transaction_.has_value() ||
        !incoming.transaction_->animation().has_value()) {
      continue;
    }
    const auto source = tree.sources_.find(*incoming.motion_->zoom);
    if (source == tree.sources_.end()) {
      continue;
    }
    const auto found = tree.lookup(source->second);
    if (!found.has_value() || !found->get().presentation_->observed()) {
      continue;
    }
    incoming.presentation_.edit().morph(
        motion::Presentation::Morph{
            .source = found->get().presentation_->snapshot(),
            .properties = motion::MatchedGeometryProperties::frame |
                          motion::MatchedGeometryProperties::corner_radius,
            .anchor = motion::UnitPoint::center},
        tree.frame_time_);
    tree.repaint(incoming);
  }
}

void Reconciler::pair() {
  Tree& tree = tree_;
  for (const NodeId id : std::exchange(arrivals_, {})) {
    const auto arrival = tree.lookup(id);
    if (!arrival.has_value() || departures_.empty()) {
      continue;
    }
    Node& incoming = arrival->get();
    if (!incoming.motion_->matched.has_value() ||
        !incoming.transaction_.has_value() ||
        !incoming.transaction_->animation().has_value()) {
      continue;
    }
    const auto match =
        std::ranges::find_if(departures_, [&tree, &incoming](NodeId leaving) {
          const auto found = tree.lookup(leaving);
          return found.has_value() &&
                 found->get().motion_->matched.has_value() &&
                 found->get().motion_->matched->key ==
                     incoming.motion_->matched->key;
        });
    if (match == departures_.end()) {
      continue;
    }
    Node& outgoing = tree.lookup(*match)->get();
    incoming.presentation_.edit().morph(
        motion::Presentation::Morph{
            .source = outgoing.presentation_->snapshot(),
            .properties = incoming.motion_->matched->properties,
            .anchor = incoming.motion_->matched->anchor},
        tree.frame_time_);
    outgoing.presentation_.edit().conceal();
    tree.repaint(incoming);
    tree.repaint(outgoing);
    departures_.erase(match);
  }
  departures_.clear();
}

void Reconciler::depart(
    Node& node, const std::shared_ptr<const DepartureHold::Token>& token) {
  if (node.motion_->matched.has_value() && node.motion_->matched->is_source) {
    unlist(node);
    departures_.push_back(node.id_);
  }
  if (node.lifecycle().depart()) {
    node.tasks_.clear();
    node.exclusive_.reset();
    disappearing_.push_back(Departure{.node = node.id_, .token = token});
  }
  park(node, token);
  for (const std::unique_ptr<Node>& child : node.children_) {
    depart(*child, token);
  }
}

void Reconciler::apply(Node& node, std::vector<EnvironmentOverride> overrides) {
  std::vector<Shift> changed;
  for (const EnvironmentOverride& entry : overrides) {
    const auto previous = std::ranges::find(node.overrides_, entry.key(),
                                            &EnvironmentOverride::key);
    if (previous == node.overrides_.end()) {
      changed.push_back(
          Shift{.key = entry.key(), .before = nullptr, .after = nullptr});
    } else if (!previous->same_value(entry)) {
      changed.push_back(Shift{.key = entry.key(),
                              .before = &previous->value(),
                              .after = &entry.value()});
    }
  }
  for (const EnvironmentOverride& entry : node.overrides_) {
    if (std::ranges::find(overrides, entry.key(), &EnvironmentOverride::key) ==
        overrides.end()) {
      changed.push_back(
          Shift{.key = entry.key(), .before = nullptr, .after = nullptr});
    }
  }
  if (changed.empty()) {
    return;
  }
  const std::vector<EnvironmentOverride> previous =
      std::exchange(node.overrides_, std::move(overrides));
  tree_.get().repaint(node);
  for (const Shift& shift : changed) {
    invalidate(node, shift, true);
  }
}

void Reconciler::invalidate(Node& node, const Shift& shift, bool origin) {
  const Identity key = shift.key;
  if (!node.active()) {
    return;
  }
  if (!origin && node.overrides(key)) {
    if (Tree::transforms(node, key) && tree_.get().rematerialize(node, key)) {
      invalidate(node, Shift{.key = key, .before = nullptr, .after = nullptr},
                 true);
    }
    return;
  }
  if (node.reads(key) &&
      (shift.before == nullptr || shift.after == nullptr ||
       !node.content_->ignores_change(key, *shift.before, *shift.after))) {
    tree_.get().dirty_.insert(node.id_);
    tree_.get().repaint(node);
    tree_.get().transactions_.try_emplace(node.id_, top());
  }
  for (const std::unique_ptr<Node>& child : node.children_) {
    invalidate(*child, shift, false);
  }
}

void Reconciler::notify() {
  Tree& tree = tree_;
  for (const Departure& departure : std::exchange(disappearing_, {})) {
    const auto found = tree.lookup(departure.node);
    if (!found.has_value()) {
      continue;
    }
    NodeHooks& hooks = found->get().hooks_;
    if (hooks.disappear && !obscured(found->get())) {
      hooks.disappear();
    }
    if (hooks.depart) {
      hooks.depart(DepartureHold{departure.token});
    }
  }
  std::vector<Covering> coverings = std::exchange(coverings_, {});
  for (const Covering& covering : coverings) {
    const auto found = tree.lookup(covering.node);
    if (!covering.covered || !found.has_value() || !found->get().active()) {
      continue;
    }
    const std::optional<NodeId> parent = found->get().parent_;
    const auto above = parent.has_value() ? tree.lookup(*parent) : std::nullopt;
    if (!above.has_value() || !obscured(above->get())) {
      announce(found->get(), false, true);
    }
  }
  for (const NodeId id : std::exchange(appearing_, {})) {
    const auto found = tree.lookup(id);
    if (!found.has_value()) {
      continue;
    }
    Node& node = found->get();
    if (node.lifecycle().appear() && node.hooks_.appear && !obscured(node)) {
      node.hooks_.appear();
    }
  }
  for (const Covering& covering : coverings) {
    const auto found = tree.lookup(covering.node);
    if (!covering.covered && found.has_value() && found->get().active() &&
        !obscured(found->get())) {
      announce(found->get(), true, true);
    }
  }
  for (Change& change : std::exchange(changes_, {})) {
    const auto found = tree.lookup(change.node);
    if (!found.has_value() || change.index >= found->get().changes_.size()) {
      continue;
    }
    ChangeObserver& current = *found->get().changes_[change.index];
    current.notify(change.previous != nullptr ? *change.previous : current);
  }
  start_tasks();
}

auto Reconciler::obscured(const Node& node) -> bool {
  std::reference_wrapper<const Node> current = node;
  while (true) {
    if (current.get().interactions_->covered) {
      return true;
    }
    if (!current.get().parent_.has_value()) {
      return false;
    }
    const auto parent = tree_.get().lookup(*current.get().parent_);
    if (!parent.has_value()) {
      return false;
    }
    current = parent->get();
  }
}

void Reconciler::announce(Node& node, bool appearing, bool root) {
  if (!root && (node.interactions_->covered || !node.active())) {
    return;
  }
  if (node.phase() == NodeLifecyclePhase::live) {
    if (appearing && node.hooks_.appear) {
      node.hooks_.appear();
    } else if (!appearing && node.hooks_.disappear) {
      node.hooks_.disappear();
    }
  }
  for (const std::unique_ptr<Node>& child : node.children_) {
    announce(*child, appearing, false);
  }
}

void Reconciler::release_transitions() {
  Tree& tree = tree_;
  std::erase_if(tree.transition_holds_, [&tree](NodeId id) {
    const auto found = tree.lookup(id);
    if (!found.has_value()) {
      return true;
    }
    Node& node = found->get();
    if (!node.presentation_->removal_finished()) {
      return false;
    }
    node.transition_hold_.reset();
    return true;
  });
}

auto Reconciler::sweep() -> bool {
  Tree& tree = tree_;
  release_transitions();
  bool retired = false;
  std::erase_if(tree.departing_roots_, [this, &tree, &retired](NodeId id) {
    const auto found = tree.lookup(id);
    if (!found.has_value()) {
      return true;
    }
    if (!found->get().departure_.expired()) {
      return false;
    }
    retire(found->get());
    retired = true;
    return true;
  });
  std::erase_if(tree.departing_roots_,
                [&tree](NodeId id) { return !tree.lookup(id).has_value(); });
  return retired;
}

void Reconciler::retire(Node& node) {
  conclude(node);
  const auto parent = tree_.get().lookup(*node.parent_);
  if (!parent.has_value()) {
    return;
  }
  std::vector<std::unique_ptr<Node>>& siblings = parent->get().children_;
  const auto found = std::ranges::find_if(
      siblings, [&node](const std::unique_ptr<Node>& child) {
        return child.get() == &node;
      });
  if (found == siblings.end()) {
    return;
  }
  std::unique_ptr<Node> owned = std::move(*found);
  siblings.erase(found);
  tree_.get().repaint(parent->get());
  if (recyclable(parent->get(), *owned)) {
    stash(parent->get(), std::move(owned));
  }
}

auto Reconciler::recyclable(const Node& parent, const Node& node) -> bool {
  Tree& tree = tree_;
  if (!node.key_.has_value() || node.ghost_ ||
      node.transition_hold_.has_value() || !parent.parent_.has_value()) {
    return false;
  }
  const auto container = tree.lookup(*parent.parent_);
  if (!container.has_value() || !container->get().content_->reveals()) {
    return false;
  }
  const auto pooled = tree.recycled_.find(parent.id_);
  return pooled == tree.recycled_.end() ||
         pooled->second.size() < recycled_limit;
}

void Reconciler::stash(const Node& parent, std::unique_ptr<Node> node) {
  wipe(*node);
  tree_.get().recycled_[parent.id_].push_back(std::move(node));
}

void Reconciler::wipe(Node& node) {
  Tree& tree = tree_;
  unlist(node);
  tree.forget(node.id_);
  node.state_.clear();
  node.environment_reads_.clear();
  node.highlight_reads_ = false;
  node.dependencies_.clear();
  node.stale_dependencies_.clear();
  node.hooks_ = NodeHooks{};
  node.departure_.reset();
  node.boundary_failure_.reset();
  node.boundary_rebuild_ = false;
  node.tasks_.clear();
  node.exclusive_.reset();
  node.interactions_.reset();
  node.overrides_.clear();
  node.transforms_.clear();
  node.preferences_.clear();
  node.published_.reset();
  node.preference_observers_.clear();
  node.preferring_ = false;
  node.anchored_ = false;
  node.preference_stamp_ = 0;
  node.preference_resolved_ = 0;
  node.frame_ = Rect{};
  node.presented_.clear();
  node.scroll_presentation_ = ScrollPresentation{};
  node.content_size_.reset();
  node.safe_area_ = theme::Edges{};
  node.enclosure_.reset();
  node.motion_.reset();
  node.changes_.clear();
  node.transaction_.reset();
  node.presentation_.clear();
  node.transition_hold_.reset();
  node.ghost_ = false;
  for (const std::unique_ptr<Node>& child : node.children_) {
    wipe(*child);
  }
}

void Reconciler::rekey(Node& node, NodeId parent, std::size_t depth) {
  Tree& tree = tree_;
  node.id_ = tree.allocate();
  node.parent_ = parent;
  node.depth_ = depth;
  node.paint_stamp_ = 0;
  node.own_paint_stamp_ = 0;
  *node.lifecycle_ = Node::Lifecycle{};
  if (node.anchor_lent_ || node.anchor_.use_count() != 1) {
    node.anchor_ = std::make_shared<NodeAnchor>(tree, node.id_);
    node.anchor_lent_ = false;
  } else {
    node.anchor_->rebind(node.id_);
  }
  node.fresh_ = true;
  tree.adopt(node);
  for (const std::unique_ptr<Node>& child : node.children_) {
    rekey(*child, node.id_, depth + 1);
  }
}

auto Reconciler::place(Node& parent, Element element) -> std::unique_ptr<Node> {
  Handoff handoff = arrive(parent, std::move(element));
  handoff.returned = true;
  Node& node = *handoff.owned;
  start(node, std::move(handoff));
  return std::move(returned_);
}

auto Reconciler::arrive(Node& parent, Element element) -> Handoff {
  Tree& tree = tree_;
  const auto pooled = tree.recycled_.find(parent.id_);
  if (pooled != tree.recycled_.end()) {
    std::vector<std::unique_ptr<Node>>& spare = pooled->second;
    const auto match =
        std::ranges::find_if(spare | std::views::reverse,
                             [kind = element.content_->kind()](
                                 const std::unique_ptr<Node>& candidate) {
                               return candidate->kind() == kind;
                             });
    if (match != (spare | std::views::reverse).end()) {
      std::unique_ptr<Node> node = std::move(*match);
      spare.erase(std::next(match).base());
      ++tree.recycles_;
      return Handoff{.arrival = Arrival::revive,
                     .element = std::move(element),
                     .owned = std::move(node),
                     .parent = &parent};
    }
  }
  return mount_head(parent, std::move(element));
}

void Reconciler::lend_declarer(
    const Node& node,
    const std::vector<std::shared_ptr<const ToolbarItem>>& items) {
  const std::optional<NodeId> parent = node.parent();
  if (!parent.has_value()) {
    return;
  }
  const auto found = tree_.get().lookup(*parent);
  if (!found.has_value()) {
    return;
  }
  Node& declarer = found->get();
  declarer.anchor_lent_ = true;
  for (const std::shared_ptr<const ToolbarItem>& item : items) {
    if (item->content() != nullptr && !item->content()->declared()) {
      item->content()->declared_by(declarer.anchor_);
    }
  }
}

auto Reconciler::declared(Node& node, Element& element)
    -> std::vector<Preference> {
  std::vector<Preference> declarations = element.environment_.extract(
      &Element::EnvironmentDeclarations::preferences);
  if (!element.navigation_.engaged()) {
    return declarations;
  }
  NavigationDeclarations& navigation = element.navigation_.edit();
  std::vector<Preference> made;
  if (!navigation.title.has_value() && navigation.toolbar.empty() &&
      navigation.destinations.empty() &&
      !navigation.container_background.has_value() &&
      navigation.chrome.empty()) {
    made = std::move(declarations);
    return made;
  }
  made.reserve(declarations.size() + 5);
  if (navigation.title.has_value()) {
    made.push_back(Preference::value(NavigationDeclarations::title_key,
                                     std::move(navigation.title)));
  }
  if (!navigation.toolbar.empty()) {
    lend_declarer(node, navigation.toolbar);
    made.push_back(Preference::contribution(NavigationDeclarations::toolbar_key,
                                            std::move(navigation.toolbar)));
  }
  if (!navigation.destinations.empty()) {
    made.push_back(
        Preference::contribution(NavigationDeclarations::destinations_key,
                                 std::move(navigation.destinations)));
  }
  if (navigation.container_background.has_value()) {
    made.push_back(
        Preference::value(NavigationDeclarations::background_key,
                          std::move(navigation.container_background)));
  }
  if (!navigation.chrome.empty()) {
    made.push_back(Preference::contribution(NavigationDeclarations::chrome_key,
                                            std::move(navigation.chrome)));
  }
  for (Preference& entry : declarations) {
    made.push_back(std::move(entry));
  }
  return made;
}

void Reconciler::prefer(
    Node& node, std::vector<Preference> incoming,
    std::vector<std::shared_ptr<PreferenceObserver>> observers, bool fresh) {
  const bool same =
      !fresh && node.preferences_.size() == incoming.size() &&
      std::ranges::equal(node.preferences_, incoming,
                         [](const Preference& held, const Preference& next) {
                           return held.absorbs(next);
                         });
  if (!same) {
    const bool touched = !node.preferences_.empty() || !incoming.empty();
    node.preferences_ = std::move(incoming);
    node.anchored_ =
        std::ranges::any_of(
            node.preferences_,
            [](const Preference& entry) { return entry.anchored(); }) ||
        (node.published_.has_value() && node.published_->anchored());
    if (touched) {
      tree_.get().touch_preferences(node);
    }
  }
  const std::vector<std::shared_ptr<PreferenceObserver>>& previous =
      node.preference_observers_;
  for (std::size_t index = 0;
       index < observers.size() && index < previous.size(); ++index) {
    if (observers[index]->key() == previous[index]->key()) {
      observers[index]->adopt(*previous[index]);
    }
  }
  node.preference_observers_ = std::move(observers);
}

void Reconciler::conclude(Node& node) {
  node.tasks_.clear();
  node.exclusive_.reset();
  if (!node.lifecycle().retire()) {
    node.lifecycle().abandon();
  }
  for (const std::unique_ptr<Node>& child : node.children_) {
    conclude(*child);
  }
}

}
