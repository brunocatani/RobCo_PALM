#pragma once
#include <array>
#include <cstdint>

namespace wheel {
enum class Gesture : unsigned { ThumbsUp, MiddleFinger, RockAndRoll, Peace, Pointing, Fist, OpenHand, Shaka, Count };
inline constexpr unsigned kGestureCount=static_cast<unsigned>(Gesture::Count);
// Authored flex uses the provider skeleton's convention: zero closed, one open.
// Each row is thumb/index/middle/ring/pinky; splay is in radians.
struct GestureDefinition {
 const char* name;
 std::array<float,5> flex;
 std::array<float,5> splay{};
};
inline constexpr std::array<GestureDefinition,kGestureCount> kGestures{{
 {"Thumbs up",     {1.5f,0,0,0,0}, {.45f,0,0,0,0}},
 {"Middle finger", {0,0,1,0,0}},
 {"Rock and roll", {0,1,0,0,1}, {0,-.10f,0,0,.10f}},
 {"Peace",         {0,1,1,0,0}, {0,-.16f,.16f,0,0}},
 {"Pointing",      {0,1,0,0,0}},
 {"Fist",          {0,0,0,0,0}},
 {"Open hand",     {1,1,1,1,1}},
 {"Shaka",         {1.5f,0,0,0,1}, {.45f,0,0,0,.12f}},
}};
// Small nonzero choices are distinct from both inventory tokens and navigation.
inline constexpr unsigned gestureChoice(unsigned gesture,bool left) {
 return gesture<kGestureCount?16+gesture+(left?kGestureCount:0):0;
}
inline constexpr bool isGestureChoice(std::uint64_t choice) {return choice>=16 && choice<16+2*kGestureCount;}
inline constexpr unsigned gestureIndex(unsigned choice) {return (choice-16)%kGestureCount;}
inline constexpr bool gestureIsLeft(unsigned choice) {return choice>=16+kGestureCount;}
enum class GestureAvailability : unsigned { Unavailable, Free, Busy, ActionHeld };
struct GestureViewState {
 bool showing{},left{true};
 std::array<GestureAvailability,2> availability{}; // right, left
 std::array<unsigned,2> active{}; // choice tokens, zero when inactive
};
inline const char* gestureAvailabilityText(GestureAvailability state) {
 switch(state) {
 case GestureAvailability::Free:return "FREE";
 case GestureAvailability::Busy:return "BUSY - FREE THIS HAND FIRST";
 case GestureAvailability::ActionHeld:return "RELEASE HAND ACTION BUTTONS";
 default:return "HAND POSES UNAVAILABLE";
 }
}
}
