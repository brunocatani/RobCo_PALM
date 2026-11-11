#include "../src/GrenadeSelection.h"
#include <iostream>
#include <stdexcept>
#include <string>

// PALM reads ROCK's applied Grenades.ImmersiveGrenades through ROCK's Configuration API at
// selection time; an unrelated boolean, a missing setting or a failed visit must not select handoff.
namespace
{
    using namespace rock::api::configuration;
    using rock::api::Status;
    std::string grenadeMode = "false";
    bool publishGrenade = true;
    bool grenadeVisitSucceeds = true;
    Status visitGrenade(rock::api::OwnerToken owner, Group group, Visitor callback, void* context) noexcept
    {
        if (owner != 1) return Status::OwnerNotRegistered;
        if (!grenadeVisitSucceeds || group != Group::Consumer) return Status::NotReady;
        const Setting unrelated{ "Grenades", "Other", "true", "true", "", "", ValueType::Boolean, 0 };
        callback(&unrelated, context);
        if (publishGrenade) {
            const Setting setting{ "Grenades", "ImmersiveGrenades", grenadeMode.c_str(), "true", "", "", ValueType::Boolean, 0 };
            callback(&setting, context);
        }
        return Status::Ok;
    }
    Status currentRevision(rock::api::OwnerToken owner, std::uint64_t* out) noexcept
    {
        if (owner != 1) return Status::OwnerNotRegistered;
        *out = 1;
        return Status::Ok;
    }
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
}

int main()
{
    try {
        const Api grenadeApi{ currentRevision, visitGrenade, nullptr };
        require(wheel::immersiveGrenadesEnabled(&grenadeApi, 1) == false, "disabled immersive grenades did not select vanilla mode");
        grenadeMode = "true";
        require(wheel::immersiveGrenadesEnabled(&grenadeApi, 1) == true, "enabled immersive grenades did not select handoff mode");
        grenadeMode = "false";
        require(wheel::immersiveGrenadesEnabled(&grenadeApi, 1) == false, "mode change left a cached handoff selection");
        publishGrenade = false;
        require(!wheel::immersiveGrenadesEnabled(&grenadeApi, 1).has_value(), "missing grenade setting used an unrelated boolean");
        publishGrenade = true;
        grenadeVisitSucceeds = false;
        require(!wheel::immersiveGrenadesEnabled(&grenadeApi, 1).has_value(), "failed setting visit enabled handoff");
        require(!wheel::immersiveGrenadesEnabled(nullptr, 0).has_value(), "missing ROCK configuration API was accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Grenade selection checks passed.\n";
    return 0;
}
