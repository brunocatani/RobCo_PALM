#pragma once
#include "GesturePolicy.h"

namespace wheel {
// Owned exclusively by the ROCK game-thread frame callback. The renderer gets
// value snapshots only; ROCK owns publication, arbitration and lease cleanup.
class Gestures {
public:
 void initialize();
 void update(std::uint64_t owner,const rock::provider::RockProviderFrameSnapshot& frame,bool usable);
 const char* select(std::uint64_t owner,unsigned choice,const rock::provider::RockProviderFrameSnapshot& frame);
 void clear(std::uint64_t owner);
 const GestureViewState& view() const {return _view;}
private:
 void clearHand(std::uint64_t owner,unsigned hand,const char* reason);
 bool publish(std::uint64_t owner,unsigned hand,const rock::provider::RockProviderFrameSnapshot& frame);
 bool _available{};
 GestureViewState _view;
 std::array<rock::provider::RockProviderHandVisualAuthorityRequestV1,2> _requests;
};
}
