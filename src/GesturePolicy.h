#pragma once
#include "GestureCatalog.h"
#include "RockFrame.h"

namespace wheel {
inline GestureAvailability gestureHandAvailability(
 const wheel::RockFrame& frame,
 const rock::api::grab::HandInteractionState& hand,bool queryOk,
 bool inputAvailable,bool actionHeld,bool poseAvailable,bool poseBusy) {

 if(!poseAvailable || !queryOk || !inputAvailable ||
    !(hand.flags&static_cast<unsigned>(rock::api::grab::HandInteractionFlag::Valid)) ||
    hand.frameIndex!=frame.sample.frameIndex || hand.worldGeneration!=frame.sample.worldGeneration ||
    hand.skeletonGeneration!=frame.sample.skeletonGeneration || hand.providerGeneration!=frame.sample.providerGeneration)
  return GestureAvailability::Unavailable;
 constexpr auto busyFlags=static_cast<unsigned>(rock::api::grab::HandInteractionFlag::FiringGrip)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlag::PartGrip)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlag::PartCarry)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlag::TransitionSuppressed)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlag::TouchGrab);
 // Highlighting or brushing a nearby item does not occupy a hand. A pull or
 // catch does, even before ROCK has a held body to report.
 const bool passive=hand.phase==rock::api::grab::HandInteractionPhase::Idle ||
  hand.phase==rock::api::grab::HandInteractionPhase::Touching || hand.phase==rock::api::grab::HandInteractionPhase::Selecting;
 if(poseBusy || !passive || hand.heldBodyCount || (hand.flags&busyFlags) ||
    hand.hand==frame.reservedHand)
  return GestureAvailability::Busy;
 return actionHeld?GestureAvailability::ActionHeld:GestureAvailability::Free;
}
}
