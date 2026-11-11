#pragma once
#include "GesturePolicy.h"

namespace wheel {
// Owned exclusively by PALM's game-thread input callback. The renderer gets
// value snapshots only; ROCK owns publication, arbitration and lease cleanup.
class Gestures {
public:
 void initialize();
 bool available() const {return _available;}
 // wheelButtons: per hand (right, left), the button-id bits of the wheel's
 // opening binding while the wheel owns that input, otherwise zero.
 void update(std::uint64_t owner,const wheel::RockFrame& frame,bool usable,const std::array<std::uint64_t,2>& wheelButtons);
 const char* select(std::uint64_t owner,unsigned choice,const wheel::RockFrame& frame);
 void clear(std::uint64_t owner);
 const GestureViewState& view() const {return _view;}
private:
 void clearHand(std::uint64_t owner,unsigned hand,const char* reason);
 bool publish(std::uint64_t owner,unsigned hand,const wheel::RockFrame& frame);
 // Every input of one hand's last availability decision, logged when a
 // selection is refused or an active gesture is cleared.
 struct HandInputs {
  int queryStatus{-1};
  rock::api::grab::HandInteractionState interaction{};
  std::uint32_t unavailableButtons{},heldButtons{}; // bit per entry of kPoseButtons
  unsigned pose{};
  bool uiBusy{},offhandGrip{};
 };
 void logInputs(unsigned hand,const char* event,const wheel::RockFrame& frame) const;
 bool _available{};
 std::array<HandInputs,2> _inputs{};
 GestureViewState _view;
 std::array<rock::api::animation::HandVisualAuthorityRequest,2> _requests;
};
}
