#pragma once
#include "GesturePolicy.h"

namespace wheel {
// Owned exclusively by PALM's game-thread input callback. The renderer gets
// value snapshots only; ROCK owns publication, arbitration and lease cleanup.
class Gestures {
public:
 void initialize();
 bool available() const {return _available;}
 void update(std::uint64_t owner,const wheel::RockFrame& frame,bool usable,bool wheelInputOwned=false);
 const char* select(std::uint64_t owner,unsigned choice,const wheel::RockFrame& frame);
 void clear(std::uint64_t owner);
 const GestureViewState& view() const {return _view;}
private:
 void clearHand(std::uint64_t owner,unsigned hand,const char* reason);
 bool publish(std::uint64_t owner,unsigned hand,const wheel::RockFrame& frame);
 bool _available{};
 GestureViewState _view;
 std::array<rock::api::animation::HandVisualAuthorityRequestV1,2> _requests;
};
}
