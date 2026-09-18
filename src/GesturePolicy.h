#pragma once
#include "GestureCatalog.h"
#include "RockFrame.h"

namespace wheel {
inline GestureAvailability gestureHandAvailability(
 const wheel::RockFrame& frame,
 const rock::api::grab::HandInteractionStateV1& hand,bool queryOk,
 bool inputAvailable,bool actionHeld,bool poseAvailable,bool poseBusy) {

 if(!poseAvailable || !queryOk || !inputAvailable ||
    !(hand.flags&static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::Valid)) ||
    hand.frameIndex!=frame.frameIndex || hand.worldGeneration!=frame.worldGeneration ||
    hand.skeletonGeneration!=frame.skeletonGeneration || hand.providerGeneration!=frame.providerGeneration)
  return GestureAvailability::Unavailable;
 constexpr auto busyFlags=static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::FiringGrip)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::PartGrip)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::PartCarry)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::TransitionSuppressed)|
  static_cast<unsigned>(rock::api::grab::HandInteractionFlagV1::TouchGrab);
 // Highlighting or brushing a nearby item does not occupy a hand. A pull or
 // catch does, even before ROCK has a held body to report.
 const bool passive=hand.phase==rock::api::grab::HandInteractionPhaseV1::Idle ||
  hand.phase==rock::api::grab::HandInteractionPhaseV1::Touching || hand.phase==rock::api::grab::HandInteractionPhaseV1::Selecting;
 if(poseBusy || !passive || hand.heldBodyCount || (hand.flags&busyFlags) ||
    (hand.hand==frame.offhandHand && frame.offhandReservation!=rock::api::grab::OffhandReservation::Normal))
  return GestureAvailability::Busy;
 return actionHeld?GestureAvailability::ActionHeld:GestureAvailability::Free;
}
}
