#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace arisa::controls {

class ClipboardCommands {
 public:
  using Items = std::function<std::vector<composition::TransferItem>()>;
  using Paste = std::function<void(std::vector<composition::TransferItem>)>;

  static constexpr std::string_view cut_label = "Cut";
  static constexpr std::string_view copy_label = "Copy";
  static constexpr std::string_view paste_label = "Paste";

  void copies(Items items) { copy_ = std::move(items); }

  void cuts(Items items) { cut_ = std::move(items); }

  void pastes(std::vector<composition::UTType> types, Paste paste) {
    accepted_ = std::move(types);
    paste_ = std::move(paste);
  }

  void use(std::optional<composition::Pasteboard> board) {
    board_ = std::move(board);
  }

  auto empty() const noexcept -> bool { return !copy_ && !cut_ && !paste_; }

  auto can_paste() const -> bool;

  void copy() const;

  void cut() const;

  void paste() const;

  auto items() const -> std::vector<composition::Element>;

  auto keys(composition::Element target) const -> composition::Element;

 private:
  Items copy_;
  Items cut_;
  std::vector<composition::UTType> accepted_;
  Paste paste_;
  std::optional<composition::Pasteboard> board_;
};

}
