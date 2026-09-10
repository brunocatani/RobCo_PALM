#pragma once
#include "WheelModel.h"
#include <optional>
#include <utility>

namespace wheel {
// Protected by RuntimeState::presentationMutex. A render callback may finish
// after physical release or a new opening; only its own generation may publish.
struct WheelSelectionState {
 std::uint64_t generation{};
 std::optional<Action> drawn;

 void begin(std::uint64_t ticket) {generation=ticket;drawn.reset();}
 bool publish(std::uint64_t ticket,const Action& hover) {
  if(!ticket || ticket!=generation)return false;
  drawn=hover;return true;
 }
 std::optional<Action> release() {
  generation=0;return std::exchange(drawn,std::nullopt);
 }
};
}
