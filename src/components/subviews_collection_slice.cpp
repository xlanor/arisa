#include <arisa/components/group.hpp>
#include <arisa/components/subviews_collection_slice.hpp>

#include <utility>
#include <vector>

namespace arisa::components {

SubviewsCollectionSlice::operator composition::Element() && {
  std::vector<composition::Element> members;
  members.reserve(subviews_.size());
  for (Subview& subview : subviews_) {
    composition::ElementKey key = subview.id();
    members.push_back(std::move(subview).view().id(std::move(key)));
  }
  return Group(std::move(members));
}

}
