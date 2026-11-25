#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/surface_presence.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace arisa::components {

class SurfaceView : public composition::Element {
 public:
  class Content;
  class Source;
  class Owner;

  explicit SurfaceView(const provider::GpuSurface& surface);

  template <class Gpu>
  SurfaceView(Gpu& gpu, provider::SurfaceOptions options,
              typename Gpu::SurfaceFunction function)
      : SurfaceView(std::shared_ptr<Source>{std::make_shared<Binding<Gpu>>(
            gpu, options, std::move(function))}) {}

 private:
  template <class Gpu>
  class Binding;

  explicit SurfaceView(std::shared_ptr<Source> source);

  void fill();
};

class SurfaceView::Source {
 public:
  Source() = default;
  Source(const Source&) = delete;
  auto operator=(const Source&) -> Source& = delete;
  Source(Source&&) = delete;
  auto operator=(Source&&) -> Source& = delete;
  virtual ~Source() = default;

  [[nodiscard]] virtual auto attach()
      -> std::expected<provider::GpuSurface, provider::ProviderError> = 0;

  virtual void adopt(Source& incoming) = 0;

  virtual auto options() const noexcept -> const provider::SurfaceOptions& = 0;

  virtual auto kind() const noexcept -> const void* = 0;
};

template <class Gpu>
class SurfaceView::Binding final : public SurfaceView::Source {
 public:
  using Function = typename Gpu::SurfaceFunction;

  Binding(Gpu& gpu, provider::SurfaceOptions options, Function function)
      : gpu_(gpu),
        options_(options),
        slot_(std::make_shared<Slot>(std::move(function))) {}

  auto attach()
      -> std::expected<provider::GpuSurface, provider::ProviderError> override {
    return gpu_.get().add_surface(options_, [slot = slot_](auto&&... args) {
      slot->call(std::forward<decltype(args)>(args)...);
    });
  }

  void adopt(Source& incoming) override {
    if (incoming.kind() != kind()) {
      return;
    }
    auto& other = static_cast<Binding&>(incoming);
    slot_->replace(other.slot_->take());
    options_ = other.options_;
  }

  auto options() const noexcept -> const provider::SurfaceOptions& override {
    return options_;
  }

  auto kind() const noexcept -> const void* override { return &tag; }

 private:
  class Slot {
   public:
    explicit Slot(Function function) noexcept
        : function_(std::move(function)) {}

    template <class... Args>
    void call(Args&&... args) {
      const std::scoped_lock lock{mutex_};
      if (function_) {
        function_(std::forward<Args>(args)...);
      }
    }

    void replace(Function function) {
      Function released;
      {
        const std::scoped_lock lock{mutex_};
        released = std::exchange(function_, std::move(function));
      }
    }

    auto take() -> Function {
      const std::scoped_lock lock{mutex_};
      return std::exchange(function_, nullptr);
    }

   private:
    std::mutex mutex_;
    Function function_;
  };

  static constexpr char tag{};

  std::reference_wrapper<Gpu> gpu_;
  provider::SurfaceOptions options_;
  std::shared_ptr<Slot> slot_;
};

class SurfaceView::Content final : public composition::ElementContent {
 public:
  Content(provider::SurfaceId surface,
          std::shared_ptr<const provider::SurfacePresence> presence) noexcept
      : surface_(surface), presence_(std::move(presence)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto framed() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto surface() const noexcept -> provider::SurfaceId { return surface_; }

 private:
  provider::SurfaceId surface_;
  std::shared_ptr<const provider::SurfacePresence> presence_;
};

class SurfaceView::Owner final : public composition::ElementContent {
 public:
  explicit Owner(std::shared_ptr<Source> source) noexcept
      : source_(std::move(source)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  class Attachment {
   public:
    Attachment(std::shared_ptr<Source> source,
               provider::GpuSurface surface) noexcept
        : source_(std::move(source)), surface_(std::move(surface)) {}

    void refresh(Source& incoming);

    auto surface() const noexcept -> const provider::GpuSurface& {
      return surface_;
    }

   private:
    std::shared_ptr<Source> source_;
    provider::GpuSurface surface_;
  };

  static const composition::StateKey<std::shared_ptr<Attachment>> attachment;

  std::shared_ptr<Source> source_;
};

}
