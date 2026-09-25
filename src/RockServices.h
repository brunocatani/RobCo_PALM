#pragma once
#include "RockFrame.h"
#include <ROCK/Client.h>
#include <ROCK/Hands.h>
#include <ROCK/InputV1_1.h>
namespace wheel {
struct RockSnapshotDiagnostic {
    const char* stage{"owner"}; // Static labels; safe to copy to the log task.
    int status{-1}; // -1 means the query was not called.
    rock::api::SampleV1 observed;
};
struct RockServices {
    rock::api::Client client;
    const rock::api::grab::ApiV1* grab{};
    const rock::api::weapon::ApiV1* weapon{};
    const rock::api::hands::ApiV1* hands{};
    const rock::api::input::ApiV1* input{};
    const rock::api::input::v1_1::Api* placementInput{};
    const rock::api::animation::ApiV1* animation{};
    bool connect();
    bool snapshot(RockFrame& frame, RockSnapshotDiagnostic* diagnostic=nullptr) const;
};
RockServices& rockServices();
}
