#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/tree.hpp>

namespace arisa::composition {

void NodeAnchor::mark_dirty() const { tree_.get().mark_dirty(node_); }

}
