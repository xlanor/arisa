#pragma once

#include <arisa/composition/observable.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

template <class Document>
class DocumentBaseBox {
 public:
  DocumentBaseBox() noexcept = default;

  explicit DocumentBaseBox(
      std::shared_ptr<composition::Observable<Document>> base) noexcept
      : base_(std::move(base)) {}

  auto base() const noexcept
      -> const std::shared_ptr<composition::Observable<Document>>& {
    return base_;
  }

  void set_base(std::shared_ptr<composition::Observable<Document>> base) {
    base_ = std::move(base);
  }

 private:
  std::shared_ptr<composition::Observable<Document>> base_;
};

}
