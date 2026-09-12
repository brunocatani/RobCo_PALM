#pragma once
#include "ROCKProviderApi.h"

namespace wheel {
inline bool interactionBlocksOpeningHold(const rock::provider::RockProviderHandInteractionStateV1& hand) {
 using namespace rock::provider;
 using Phase=RockProviderHandInteractionPhaseV1;
 using Flag=RockProviderHandInteractionFlagV1;
 constexpr auto occupied=static_cast<unsigned>(Flag::PartGrip)|static_cast<unsigned>(Flag::PartCarry)|
  static_cast<unsigned>(Flag::TouchGrab)|static_cast<unsigned>(Flag::TransitionSuppressed);
 // Highlighting a loose object does not own input. A firing grip alone also
 // permits opening with an equipped weapon; its existing button hold is checked
 // by ControlGesture. Pulls, catches, carries and other active grips take priority.
 return !(hand.flags&static_cast<unsigned>(Flag::Valid)) || (hand.flags&occupied) || hand.heldBodyCount ||
  (hand.phase!=Phase::Idle && hand.phase!=Phase::Touching && hand.phase!=Phase::Selecting);
}
}
