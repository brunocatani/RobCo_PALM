#pragma once
#include "GestureCatalog.h"
#include "ROCKProviderApi.h"

namespace wheel {
inline GestureAvailability gestureHandAvailability(
 const rock::provider::RockProviderFrameSnapshot& frame,
 const rock::provider::RockProviderHandInteractionStateV1& hand,bool queryOk,
 bool inputAvailable,bool actionHeld,bool poseAvailable,bool poseBusy) {
 using namespace rock::provider;
 if(!poseAvailable || !queryOk || !inputAvailable ||
    !(hand.flags&static_cast<unsigned>(RockProviderHandInteractionFlagV1::Valid)) ||
    hand.frameIndex!=frame.frameIndex || hand.worldGeneration!=frame.worldGeneration ||
    hand.skeletonGeneration!=frame.skeletonGeneration || hand.providerGeneration!=frame.providerGeneration)
  return GestureAvailability::Unavailable;
 constexpr auto busyFlags=static_cast<unsigned>(RockProviderHandInteractionFlagV1::FiringGrip)|
  static_cast<unsigned>(RockProviderHandInteractionFlagV1::PartGrip)|
  static_cast<unsigned>(RockProviderHandInteractionFlagV1::PartCarry)|
  static_cast<unsigned>(RockProviderHandInteractionFlagV1::TransitionSuppressed)|
  static_cast<unsigned>(RockProviderHandInteractionFlagV1::TouchGrab);
 // Highlighting or brushing a nearby item does not occupy a hand. A pull or
 // catch does, even before ROCK has a held body to report.
 const bool passive=hand.phase==RockProviderHandInteractionPhaseV1::Idle ||
  hand.phase==RockProviderHandInteractionPhaseV1::Touching || hand.phase==RockProviderHandInteractionPhaseV1::Selecting;
 if(poseBusy || !passive || hand.heldBodyCount || (hand.flags&busyFlags) ||
    (hand.hand==frame.offhandHand && frame.offhandReservation!=RockProviderOffhandReservation::Normal))
  return GestureAvailability::Busy;
 return actionHeld?GestureAvailability::ActionHeld:GestureAvailability::Free;
}
}
