#include "PCH.h"
#include "Runtime.h"
#include "Inventory.h"
#include "Renderer.h"
#include "ROCKProviderApi.h"

namespace wheel {
namespace {
using namespace rock::provider;
struct RuntimeState {
 std::atomic_bool open{false}, inputReady{false};
 std::atomic_uint64_t generation{1};
 std::uint64_t owner{}, callback{};
 ToggleGesture toggle;
 bool suppression{};
 std::uint32_t world{},skeleton{},provider{};
};
RuntimeState& state(){static RuntimeState s;return s;}
bool ready(const RockProviderFrameSnapshot& f) {
 return f.providerReady && !f.menuBlocking && !f.configBlocking &&
 hasLifecycleFlag(f.lifecycleFlags,RockProviderLifecycleFlag::WorldAvailable) &&
 hasLifecycleFlag(f.lifecycleFlags,RockProviderLifecycleFlag::SkeletonReady);
}
void clearSuppression() {
 auto& s=state();
 if(s.suppression && RockProviderApi::inst && RockProviderApi::inst->clearHandInputSuppressionV1)
  (void)RockProviderApi::inst->clearHandInputSuppressionV1(s.owner,RockProviderHand::Right);
 s.suppression=false;
}
std::optional<rpsui::sdk::PanelPoseV1> poseFor(const RockProviderFrameSnapshot& frame) {
 const auto& t=frame.rightHandTransform;
 float x=t.rotate[0],y=t.rotate[1]; const float n=std::hypot(x,y);
 if(!std::isfinite(n) || n<.05f)return {};
 x/=n;y/=n;
 rpsui::sdk::PanelPoseV1 p;
 p.center[0]=t.translate[0]+x*85;p.center[1]=t.translate[1]+y*85;p.center[2]=t.translate[2]+9;
 p.right[0]=y;p.right[1]=-x;p.right[2]=0;
 p.front[0]=-x;p.front[1]=-y;p.front[2]=0;
 p.physicalWidth=95;p.physicalHeight=95;
 for(float v:p.center)if(!std::isfinite(v) || std::fabs(v)>1.e8f)return {};
 return p;
}
void ROCK_PROVIDER_CALL onFrame(const RockProviderFrameSnapshot* frame,void*) noexcept {
 try {
  if(!frame)return;
  auto& s=state(); const bool usable=ready(*frame);s.inputReady.store(usable);
  if(!usable) {++s.generation;s.toggle={};clearSuppression();if(s.open.load())closeWheel();return;}
  if(s.world!=frame->worldGeneration || s.skeleton!=frame->skeletonGeneration || s.provider!=frame->providerGeneration) {
   ++s.generation;s.toggle={};if(s.open.load())closeWheel();
   s.world=frame->worldGeneration;s.skeleton=frame->skeletonGeneration;s.provider=frame->providerGeneration;
  }
  RockProviderRawWandButtonStateV1 button{};
  if(!RockProviderApi::inst->getRawWandButtonStateV1(RockProviderHand::Right,32,&button) || !button.available) {
   s.toggle={};clearSuppression();if(s.open.load())closeWheel();return;
  }
  if(button.held || s.toggle.down) {
   RockProviderHandInputSuppressionRequestV1 request;
   request.hand=RockProviderHand::Right;
   request.flags=static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressOpenVrGameInput)|
    static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressConfigModeChord);
   request.leaseFrames=3;request.worldGeneration=frame->worldGeneration;
   request.skeletonGeneration=frame->skeletonGeneration;request.providerGeneration=frame->providerGeneration;
   s.suppression=RockProviderApi::inst->setHandInputSuppressionV1(s.owner,&request)==RockProviderResultV1::Ok;
  } else clearSuppression();
  const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if(!s.toggle.update(button.held!=0,now))return;
  if(s.open.load()) {closeWheel();return;}
  const auto pose=poseFor(*frame);if(!pose || !s.suppression)return;
  const auto* tasks=F4SE::GetTaskInterface();if(!tasks)return;
  const auto ticket=++s.generation;
  tasks->AddTask([p=*pose,ticket] {
   auto& s=state();if(!s.inputReady.load() || s.generation.load()!=ticket)return;
   try {
    auto inventory=readInventory();
    auto& shared=sharedModel();
    {std::scoped_lock lock(shared.mutex);
     inventory.category=shared.model.category;
     if(inventory.status=="Select something to use" && !shared.lastAction.empty())inventory.status=shared.lastAction;
     shared.model=std::move(inventory);shared.view={};}
    s.open.store(true);
    if(!presentPanel(true,&p))s.open.store(false);
    else spdlog::info("Wheel opened, generation {}",ticket);
   } catch(const std::exception& e){spdlog::error("Wheel inventory: {}",e.what());closeWheel();}
   catch(...){spdlog::error("Wheel inventory failed");closeWheel();}
  });
 } catch(...) {state().inputReady.store(false);closeWheel();}
}
}
SharedModel& sharedModel(){static SharedModel s;return s;}
bool isOpen(){return state().open.load();}
void closeWheel(){auto& s=state();s.open.store(false);++s.generation;(void)presentPanel(false);}
void activateItem(std::uint32_t id) {
 auto& s=state();if(!s.open.exchange(false))return;
 (void)presentPanel(false);
 const auto ticket=++s.generation;
 const auto* tasks=F4SE::GetTaskInterface();if(!tasks)return;
 tasks->AddTask([id,ticket] {
  if(!state().inputReady.load() || state().generation.load()!=ticket)return;
  try {
   std::string message; const bool requested=useInventoryItem(id,message);
   spdlog::info("Wheel action {:08X}: {} ({})",id,message,requested);
   auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);shared.lastAction=message;shared.model.status=message;
  } catch(const std::exception& e) {spdlog::error("Wheel action failed: {}",e.what());}
  catch(...){spdlog::error("Wheel action failed");}
 });
}
bool startRuntime() {
 if(!installPanel())return false;
 struct RegistrationRollback {
  bool complete{};
  ~RegistrationRollback(){if(!complete)unregisterPanel();}
 } rollback;
 const auto result=RockProviderApi::initialize(ROCK_PROVIDER_API_VERSION,ROCK_PROVIDER_API_V1_OWNER_FRAME_CALLBACKS_TABLE_BYTES);
 auto* api=RockProviderApi::inst;
 if(result || !api || !api->registerConsumerV1 || !api->unregisterConsumerV1 ||
  !api->registerFrameCallbackForOwnerV1 || !api->getRawWandButtonStateV1 ||
  !api->setHandInputSuppressionV1 || !api->clearHandInputSuppressionV1)return false;
 RockProviderConsumerRegistrationV1 registration;
 std::snprintf(registration.modName,sizeof(registration.modName),"ROCK Wheel Menu");
 registration.requestedCapabilities=static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::FrameSnapshots)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::HandInputSuppression);
 RockProviderConsumerHandleV1 handle;
 if(api->registerConsumerV1(&registration,&handle)!=RockProviderResultV1::Ok || !handle.ownerToken)return false;
 if((handle.grantedCapabilities&registration.requestedCapabilities)!=registration.requestedCapabilities) {
  (void)api->unregisterConsumerV1(handle.ownerToken);return false;
 }
 auto& s=state();s.owner=handle.ownerToken;
 if(api->registerFrameCallbackForOwnerV1(s.owner,onFrame,nullptr,&s.callback)!=RockProviderResultV1::Ok || !s.callback) {
  (void)api->unregisterConsumerV1(s.owner);s.owner=0;return false;
 }
 rollback.complete=true;
 spdlog::info("Wheel registered with ROCK; right thumbstick short click toggles the wheel");
 return true;
}
}
