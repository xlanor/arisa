#include <arisa/runtime/root.hpp>

#include <runtime/dispatch_queue.hpp>
#include <runtime/input_gate.hpp>

#include <algorithm>
#include <cassert>
#include <format>
#include <utility>

namespace arisa::runtime {

Root::Root(composition::Element content, provider::ProviderFactory& providers,
           std::optional<async::Executors> executors)
    : diagnostics_(providers.diagnostics()),
      threading_(providers.threading()),
      owner_(providers.threading().current_thread()),
      queue_(std::make_shared<DispatchQueue>()),
      dispatcher_(queue_),
      timeline_(
          [&clock = providers.clock()] { return clock.now().since_origin(); }),
      executors_(executors.value_or(async::Executors{.background = dispatcher_,
                                                     .blocking = dispatcher_})),
      gate_(std::make_unique<InputGate>(dispatcher_)),
      tree_(
          std::move(content),
          [&threading = providers.threading(), owner = owner_] {
            return threading.current_thread() == owner;
          },
          composition::RootServices{.root = dispatcher_,
                                    .frames = timeline_,
                                    .executors = executors_,
                                    .diagnostics = providers.diagnostics()}),
      overlays_(dispatcher_, tree_) {}

Root::~Root() {
  tree_.cancel_tasks();
  queue_->close();
  timeline_.close();
}

auto Root::dispatcher() const -> Dispatcher { return dispatcher_; }

auto Root::on_owner_thread() const noexcept -> bool {
  return threading_.get().current_thread() == owner_;
}

void Root::frame() {
  assert(on_owner_thread());
  queue_->drain();
  timeline_.fire();
  tree_.run_completions();
  tree_.set_frame_time(timeline_.now());
  tree_.reconcile();
  report_duplicate_keys();
  report_failures();
  report_geometry_loops();
  ++frames_;
}

void Root::settle() {
  assert(on_owner_thread());
  tree_.reconcile();
  report_duplicate_keys();
  report_failures();
  report_geometry_loops();
}

void Root::settle_geometry() {
  assert(on_owner_thread());
  tree_.settle_geometry();
  report_duplicate_keys();
  report_failures();
  report_geometry_loops();
}

void Root::frame(Renderer& renderer) {
  frame();
  renderer.render(tree_.root());
}

void Root::wait(std::chrono::nanoseconds limit) {
  assert(on_owner_thread());
  if (tree_.dirty() || tree_.completions_pending()) {
    return;
  }
  if (const auto deadline = timeline_.next_deadline()) {
    const std::chrono::nanoseconds remaining = *deadline - timeline_.now();
    if (remaining <= std::chrono::nanoseconds{0}) {
      return;
    }
    limit = std::min(limit, remaining);
  }
  static_cast<void>(queue_->wait_for(limit));
}

auto Root::activate(const composition::ElementKey& key) -> bool {
  const auto node = tree_.find(key);
  return node.has_value() && tree_.activate(*node);
}

auto Root::activate(composition::NodeId node) -> bool {
  return tree_.activate(node);
}

auto Root::reset_boundary(const composition::ElementKey& key) -> bool {
  const auto node = tree_.find(key);
  return node.has_value() && tree_.reset_boundary(*node);
}

auto Root::exclusive_frame(composition::ExclusiveFrame::Options options)
    -> composition::ExclusiveFrame {
  assert(on_owner_thread());
  return tree_.exclusive_frame(std::move(options));
}

auto Root::block_input() -> InputBlock { return gate_->block(); }

auto Root::pass_input(provider::Buttons buttons, PassedInput passed)
    -> InputPassthrough {
  return gate_->pass(buttons, passed);
}

auto Root::capture_buttons() -> ButtonCapture { return gate_->capture(); }

auto Root::input_blocked() const -> bool { return gate_->blocked(); }

void Root::report_duplicate_keys() {
  for (const composition::DuplicateKey& duplicate :
       tree_.take_duplicate_keys()) {
    diagnostics_.get().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "composition",
        .message = std::format("duplicate key {} under node {}",
                               duplicate.key.description(),
                               duplicate.parent.value())});
  }
}

void Root::report_geometry_loops() {
  for (const composition::NodeId id :
       tree_.geometry_loops(geometry_loop_frames)) {
    diagnostics_.get().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "geometry",
        .message = std::format(
            "node {} re-evaluated by its own geometry callback for {} "
            "consecutive frames",
            id.value(), geometry_loop_frames)});
  }
}

void Root::report_failures() {
  for (const composition::FailureReport& failure : tree_.take_failures()) {
    diagnostics_.get().report(provider::Diagnostic{
        .severity = provider::Severity::error,
        .category = "composition",
        .message =
            failure.boundary.has_value()
                ? std::format("node {} failed: {} (caught by boundary {})",
                              failure.node.value(), failure.error.message(),
                              failure.boundary->value())
                : std::format("node {} failed: {} (no boundary)",
                              failure.node.value(), failure.error.message())});
  }
}

}
