#pragma once
#include <ROCK/Core.h>
#include <ROCK/Grab.h>
#include <ROCK/Weapon.h>
#include <ROCK/Animation.h>
#include <ROCK/Input.h>
namespace wheel {
struct RockFrame : rock::api::core::SnapshotV1 {
    rock::api::Hand offhandHand{rock::api::Hand::None};
    rock::api::grab::OffhandReservation offhandReservation{rock::api::grab::OffhandReservation::Normal};
};
}
