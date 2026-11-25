#pragma once

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/texture_publisher.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace arisa::provider {

template <class Fence = Unfenced>
class ExternalTexture {
 public:
  using Publisher = TexturePublisher<Fence>;

  static auto adopt(std::shared_ptr<Publisher> publisher, Texture texture,
                    ImportDescription description,
                    support::ScopedCallback released) -> ExternalTexture {
    const TextureId id = texture.id();
    publisher->enlist(std::move(texture), description.premultiplied,
                      std::move(released));
    return ExternalTexture{std::move(publisher), id, description};
  }

  static auto adopt(std::shared_ptr<Publisher> publisher,
                    std::vector<Texture> planes, ImportDescription description,
                    support::ScopedCallback released) -> ExternalTexture {
    const TextureId id = planes.front().id();
    publisher->enlist(std::move(planes), description, std::move(released));
    return ExternalTexture{std::move(publisher), id, description};
  }

  ExternalTexture(ExternalTexture&& other) noexcept = default;
  auto operator=(ExternalTexture&& other) noexcept
      -> ExternalTexture& = default;
  ExternalTexture(const ExternalTexture&) = delete;
  auto operator=(const ExternalTexture&) -> ExternalTexture& = delete;
  ~ExternalTexture() = default;

  auto id() const noexcept -> TextureId { return id_; }

  auto description() const noexcept -> const ImportDescription& {
    return description_;
  }

  auto valid() const noexcept -> bool { return retire_.valid(); }

  void publish(Fence ready = Fence{}) const {
    if (valid()) {
      publisher_->publish(id_, std::move(ready));
    }
  }

  auto in_use() const -> bool { return valid() && publisher_->in_use(id_); }

 private:
  ExternalTexture(std::shared_ptr<Publisher> publisher, TextureId id,
                  ImportDescription description) noexcept
      : publisher_(publisher),
        id_(id),
        description_(description),
        retire_([publisher = std::move(publisher), id]() noexcept {
          publisher->retire(id);
        }) {}

  std::shared_ptr<Publisher> publisher_;
  TextureId id_;
  ImportDescription description_;
  support::ScopedCallback retire_;
};

}
