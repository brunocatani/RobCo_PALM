#pragma once
#include <ROCK/Core.h>
#include <ROCK/Grab.h>
#include <ROCK/Hands.h>
#include <ROCK/Weapon.h>
#include <ROCK/Animation.h>
#include <ROCK/Input.h>
namespace wheel {
struct RockFrame : rock::api::core::Snapshot {
    rock::api::hands::Roles roles{};
    // The hand under another mod's reload reservation, or None.
    rock::api::Hand reservedHand{rock::api::Hand::None};
};
// A readback belongs to this frame only when its publication identity matches.
inline bool sameRockSample(const rock::api::Sample& sample,const RockFrame& frame) {
    return sample.frameIndex==frame.sample.frameIndex && sample.worldGeneration==frame.sample.worldGeneration &&
     sample.skeletonGeneration==frame.sample.skeletonGeneration && sample.providerGeneration==frame.sample.providerGeneration;
}
inline const rock::api::hands::HandWeaponRole& handRole(const rock::api::hands::Roles& roles,rock::api::Hand hand) {
    return hand==rock::api::Hand::Left?roles.left:roles.right;
}
// The hand that supports the primary weapon (slot 0): its support-grip hand,
// otherwise the hand opposite its firing hand. None without a held primary.
inline rock::api::Hand primaryOffhand(const rock::api::hands::Roles& roles) {
    using rock::api::Hand;using rock::api::hands::HandRole;
    const auto holds=[&](Hand hand,HandRole role){const auto& r=handRole(roles,hand);return r.weaponSlot==0 && r.role==role;};
    if(holds(Hand::Right,HandRole::Support))return Hand::Right;
    if(holds(Hand::Left,HandRole::Support))return Hand::Left;
    if(holds(Hand::Right,HandRole::Firing))return Hand::Left;
    if(holds(Hand::Left,HandRole::Firing))return Hand::Right;
    return Hand::None;
}
}
