#pragma once
#include "RockFrame.h"
#include <ROCK/Client.h>
#include <ROCK/Hands.h>
namespace wheel {
struct RockServices {
    rock::api::Client client;
    const rock::api::grab::ApiV1* grab{};
    const rock::api::weapon::ApiV1* weapon{};
    const rock::api::hands::ApiV1* hands{};
    const rock::api::input::ApiV1* input{};
    const rock::api::animation::ApiV1* animation{};
    bool connect();
    bool snapshot(RockFrame& frame) const;
};
RockServices& rockServices();
}
