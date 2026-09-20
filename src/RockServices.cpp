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
bool RockServices::snapshot(RockFrame& frame, RockSnapshotDiagnostic* diagnostic) const {
    frame={};
    RockSnapshotDiagnostic local;
    auto& detail=diagnostic?*diagnostic:local;detail={};
    using rock::api::Status;
    if(!client.owner()) return false;
    detail.stage="core";
    detail.status=static_cast<int>(client.core()->getFrameSnapshot(client.owner(),&frame));
    if(detail.status!=static_cast<int>(Status::Ok)) return false;
    rock::api::hands::RolesV1 roles{};
    rock::api::grab::OffhandReservationStateV1 reservation{};
    detail.stage="roles";
    detail.status=static_cast<int>(hands->getRoles(client.owner(),&roles));
    detail.observed=roles.sample;
    if(detail.status!=static_cast<int>(Status::Ok)) { frame.providerReady=0;return false; }
    detail.stage="roles-sample";
    if(roles.sample.frameIndex!=frame.frameIndex || roles.sample.worldGeneration!=frame.worldGeneration ||
       roles.sample.skeletonGeneration!=frame.skeletonGeneration || roles.sample.providerGeneration!=frame.providerGeneration) { frame.providerReady=0;return false; }
    detail.stage="reservation";
    detail.status=static_cast<int>(grab->getOffhandReservationStateV1(client.owner(),&reservation));
    detail.observed={};
    detail.observed.worldGeneration=reservation.worldGeneration;
    detail.observed.skeletonGeneration=reservation.skeletonGeneration;
    detail.observed.providerGeneration=reservation.providerGeneration;
    if(detail.status!=static_cast<int>(Status::Ok)) { frame.providerReady=0;return false; }
    detail.stage="reservation-sample";
    if(reservation.worldGeneration!=frame.worldGeneration || reservation.skeletonGeneration!=frame.skeletonGeneration ||
       reservation.providerGeneration!=frame.providerGeneration) { frame.providerReady=0;return false; }
    frame.offhandHand=roles.offhand;
    frame.offhandReservation=reservation.reservation;
    detail.stage="complete";
    return true;
}
}
