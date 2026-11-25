#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/motion/transaction_stack.hpp>

#include <utility>

namespace arisa::composition {

void ScrollViewProxy::scroll_to(ElementKey id,
                                std::optional<motion::UnitPoint> anchor) const {
  const auto requests = requests_.lock();
  if (requests == nullptr) {
    return;
  }
  const auto current = motion::TransactionStack::current();
  requests->push(ScrollRequest{.target = std::move(id),
                               .anchor = anchor,
                               .transaction = current.has_value()
                                                  ? current->get()
                                                  : motion::Transaction{},
                               .within = within_});
}

}
