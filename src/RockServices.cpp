#include "PCH.h"
#include "RockServices.h"
#include "tools/LoadedRpsMods.h"
namespace wheel {
RockServices& rockServices() { static auto* service=new RockServices(); return *service; }
bool RockServices::connect() {
    const auto variant=rock_configurator::loadedRockMod();
    const auto module=variant?GetModuleHandleW(rock_configurator::modInfo(*variant).module):nullptr;
    const auto query=module?reinterpret_cast<rock::api::QueryInterfaceV1>(GetProcAddress(module,rock::api::kQueryExportName)):nullptr;
    using rock::api::Status;
    if(!query) return false;
    if(const auto status=client.connect(query,"RobCo PALM");status!=Status::Ok) {
        spdlog::warn("PALM ROCK registration refused: status={}",static_cast<unsigned>(status));
        return false;
    }
    spdlog::info("PALM connected to {}",rock_configurator::modInfo(*variant).name);
    // Permission bits are local to each interface.
    const auto bits=[](auto... permissions){return (static_cast<std::uint32_t>(permissions)|...);};
    namespace api=rock::api;
    const char* failed{};
    auto status=Status::Ok;
    if((status=client.acquire(bits(api::grab::Permission::Read,api::grab::Permission::Write),grab))!=Status::Ok) failed="Grab";
    else if((status=client.acquire(bits(api::weapon::Permission::Read),weapon))!=Status::Ok) failed="Weapon";
    else if((status=client.acquire(bits(api::hands::Permission::Read),hands))!=Status::Ok) failed="Hands";
    else if((status=client.acquire(bits(api::input::Permission::Read),input))!=Status::Ok) failed="Input";
    if(failed) {
        spdlog::warn("PALM ROCK {} interface unavailable: status={}",failed,static_cast<unsigned>(status));
        grab=nullptr;weapon=nullptr;hands=nullptr;input=nullptr;
        if(const auto closed=client.close();closed!=Status::Ok) spdlog::error("PALM ROCK owner cleanup failed: {}",static_cast<unsigned>(closed));
        return false;
    }
    // Gestures are optional: they need hand visual authority on top of input.
    if((status=client.acquire(bits(api::animation::Permission::Write),animation))!=Status::Ok) {
        animation=nullptr;
        spdlog::warn("PALM ROCK Animation interface unavailable; gestures disabled: status={}",static_cast<unsigned>(status));
    }
    return true;
}
bool RockServices::snapshot(RockFrame& frame, RockSnapshotDiagnostic* diagnostic) const {
    frame={};
    RockSnapshotDiagnostic local;
    auto& detail=diagnostic?*diagnostic:local;detail={};
    using rock::api::Status;
    if(!client.owner()) return false;
    detail.stage="core";
    detail.status=static_cast<int>(client.core()->getSnapshot(client.owner(),&frame));
    if(detail.status!=static_cast<int>(Status::Ok)) return false;
    rock::api::hands::Roles roles{};
    rock::api::grab::HandReservationState reservation{};
    detail.stage="roles";
    detail.status=static_cast<int>(hands->getRoles(client.owner(),&roles));
    detail.observed=roles.sample;
    if(detail.status!=static_cast<int>(Status::Ok)) { frame.providerReady=0;return false; }
    detail.stage="roles-sample";
    if(!sameRockSample(roles.sample,frame)) { frame.providerReady=0;return false; }
    detail.stage="reservation";
    detail.status=static_cast<int>(grab->getHandReservationState(client.owner(),&reservation));
    detail.observed={};
    detail.observed.worldGeneration=reservation.worldGeneration;
    detail.observed.skeletonGeneration=reservation.skeletonGeneration;
    detail.observed.providerGeneration=reservation.providerGeneration;
    if(detail.status!=static_cast<int>(Status::Ok)) { frame.providerReady=0;return false; }
    detail.stage="reservation-sample";
    if(reservation.worldGeneration!=frame.sample.worldGeneration || reservation.skeletonGeneration!=frame.sample.skeletonGeneration ||
       reservation.providerGeneration!=frame.sample.providerGeneration) { frame.providerReady=0;return false; }
    frame.roles=roles;
    // ROCK applies a reservation only while its hand is not that weapon's firing hand.
    const auto& reservedRole=handRole(roles,reservation.hand);
    const bool firing=reservedRole.weaponSlot==reservation.weaponSlot && reservedRole.role==rock::api::hands::HandRole::Firing;
    if(reservation.active && reservation.reservation!=rock::api::grab::HandReservationKind::Normal && !firing)
        frame.reservedHand=reservation.hand;
    detail.stage="complete";
    return true;
}
}
