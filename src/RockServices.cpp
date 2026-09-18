#include "PCH.h"
#include "RockServices.h"
namespace wheel {
RockServices& rockServices() { static auto* service=new RockServices(); return *service; }
bool RockServices::connect() {
    const auto module=GetModuleHandleA("ROCK.dll");
    const auto query=module?reinterpret_cast<rock::api::QueryInterfaceV1>(GetProcAddress(module,rock::api::kQueryExportName)):nullptr;
    using rock::api::Status;
    if (client.connect(query,"RobCo PALM")!=Status::Ok) return false;
    if (client.acquire(3,grab)!=Status::Ok || client.acquire(1,weapon)!=Status::Ok || client.acquire(1,hands)!=Status::Ok) {
        const auto status=client.close();
        if(status!=Status::Ok) spdlog::error("PALM ROCK owner cleanup failed: {}",static_cast<unsigned>(status));
        return false;
    }
    // Gesture support is optional; input and animation must both be available.
    if(client.acquire(1,input)==Status::Ok) (void)client.acquire(2,animation);
    return true;
}
bool RockServices::snapshot(RockFrame& frame) const {
    frame={};
    using rock::api::Status;
    if(!client.owner() || client.core()->getFrameSnapshot(client.owner(),&frame)!=Status::Ok) return false;
    rock::api::hands::RolesV1 roles{};
    rock::api::grab::OffhandReservationStateV1 reservation{};
    if(hands->getRoles(client.owner(),&roles)!=Status::Ok ||
       roles.sample.frameIndex!=frame.frameIndex || roles.sample.worldGeneration!=frame.worldGeneration ||
       roles.sample.skeletonGeneration!=frame.skeletonGeneration || roles.sample.providerGeneration!=frame.providerGeneration ||
       grab->getOffhandReservationStateV1(client.owner(),&reservation)!=Status::Ok ||
       reservation.worldGeneration!=frame.worldGeneration || reservation.skeletonGeneration!=frame.skeletonGeneration ||
       reservation.providerGeneration!=frame.providerGeneration) { frame.providerReady=0;return false; }
    frame.offhandHand=roles.offhand;
    frame.offhandReservation=reservation.reservation;
    return true;
}
}
