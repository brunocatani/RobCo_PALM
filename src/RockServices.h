#pragma once
#include "RockFrame.h"
#include <ROCK/Client.h>
namespace wheel {
struct RockSnapshotDiagnostic {
    const char* stage{"owner"}; // Static labels; safe to copy to the log task.
    int status{-1}; // -1 means the query was not called.
    rock::api::Sample observed;
};
struct RockServices {
    rock::api::Client client;
    const rock::api::grab::Api* grab{};
    const rock::api::weapon::Api* weapon{};
    const rock::api::hands::Api* hands{};
    const rock::api::input::Api* input{};
    const rock::api::animation::Api* animation{};
    bool connect();
    bool snapshot(RockFrame& frame, RockSnapshotDiagnostic* diagnostic=nullptr) const;
};
RockServices& rockServices();
}
