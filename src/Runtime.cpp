#include "PCH.h"
#include "Runtime.h"
#include "Inventory.h"
#include "WheelConfig.h"
#include <ShlObj.h>
#include "Renderer.h"
#include "ROCKProviderApi.h"
#include "tools/ConfiguratorRuntime.h"
#include "tools/render/FrameworkPanelRenderer.h"

namespace wheel {
namespace {
using namespace rock::provider;
struct RuntimeState {
 std::atomic_bool open{false}, inputReady{false};
 std::atomic_uint64_t generation{1};
 std::atomic_uint32_t requestedItem{0};
 std::atomic_uint64_t requestGeneration{0};
 std::atomic_bool actionPending{false};
 std::atomic_bool configRequested{false};
 rpsui::sdk::PanelPoseV1 wheelPose; // Frame callback owns the open-session anchor.
 std::uint64_t owner{}, callback{};
 std::uint64_t command{}; // Owned by ROCK's frame callback only.
 ToggleGesture toggle;
 bool suppression{};
 std::uint32_t world{},skeleton{},provider{};
};
RuntimeState& state(){static RuntimeState s;return s;}
void refreshWheelInventory() {
 const auto ticket=state().generation.load();
 if(const auto* tasks=F4SE::GetTaskInterface())tasks->AddTask([ticket] {
  if(!state().open.load() || state().generation.load()!=ticket)return;
  try {
   auto inventory=readInventory();auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
   inventory.category=shared.model.category;inventory.status=shared.lastAction;
   shared.model=std::move(inventory);shared.view={};
  } catch(...) {spdlog::error("Wheel inventory refresh failed");}
 });
}
void actionStatus(const char* message) {
 auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
 shared.lastAction=message;shared.model.status=message;
}
void cancelAction() {
 auto& s=state();s.requestedItem.store(0);
 if(s.command && RockProviderApi::inst)
  (void)RockProviderApi::inst->cancelInteractionCommandV1(s.owner,s.command);
 s.command=0;s.actionPending.store(false);
}
void serviceAction(const RockProviderFrameSnapshot& frame) {
 auto& s=state();auto* api=RockProviderApi::inst;
 if(!s.open.load()) {cancelAction();return;}
 if(const auto id=s.requestedItem.exchange(0)) {
  if(s.requestGeneration.load()!=s.generation.load()) {s.actionPending.store(false);return;}
  RockProviderForceGrabRequestV1 request;
  request.flags=static_cast<std::uint32_t>(RockProviderForceGrabFlagV1::FromPlayerInventory);
  request.targetFormId=id;request.worldGeneration=frame.worldGeneration;
  request.skeletonGeneration=frame.skeletonGeneration;request.providerGeneration=frame.providerGeneration;
  const auto result=api->requestForceGrabV1(s.owner,&request,&s.command);
  if(result!=RockProviderResultV1::RequestQueued) {
   s.command=0;s.actionPending.store(false);
   actionStatus(result==RockProviderResultV1::HandBusy?"Hands are busy — free a hand and try again":"Item handoff unavailable");
   spdlog::warn("Wheel handoff {:08X} rejected at queue: {}",id,static_cast<unsigned>(result));
  }
 }
 if(!s.command)return;
 RockProviderInteractionCommandResultV1 result;
 const auto query=api->getInteractionCommandResultV1(s.owner,s.command,&result);
 if(query==RockProviderResultV1::Ok && result.state==RockProviderInteractionCommandStateV1::Queued)return;
 const bool success=query==RockProviderResultV1::Ok && result.state==RockProviderInteractionCommandStateV1::Succeeded;
 const char* message=success?(result.hand==RockProviderHand::Left?"Taken into left hand":"Taken into right hand"):
  (result.failure==RockProviderInteractionFailureV1::HandBusy?"No free hand — put something down and try again":
   result.failure==RockProviderInteractionFailureV1::TargetAlreadyOwned?"A throwable is already held or attaching":"Item handoff failed — try again");
 actionStatus(message);
 spdlog::info("Wheel handoff {}: {} (query {}, failure {})",s.command,message,static_cast<unsigned>(query),static_cast<unsigned>(result.failure));
 if(query!=RockProviderResultV1::Ok)
  (void)api->cancelInteractionCommandV1(s.owner,s.command);
 s.command=0;s.actionPending.store(false);
 if(success)closeWheel();
 else if(const auto* tasks=F4SE::GetTaskInterface()) {
  const auto ticket=s.generation.load();
  tasks->AddTask([ticket] {
   if(state().generation.load()!=ticket || !state().open.load())return;
   try {
    auto inventory=readInventory();auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
    inventory.category=shared.model.category;inventory.status=shared.lastAction;
    shared.model=std::move(inventory);shared.view={};
   } catch(...) {spdlog::error("Wheel inventory refresh failed");}
  });
 }
}
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
  if(takeWheelConfigChange())if(const auto* tasks=F4SE::GetTaskInterface())
   tasks->AddTask([]{
    try {flushWheelConfig();refreshWheelInventory();}
    catch(const std::exception& error){spdlog::error("Wheel item configuration: {}",error.what());}
    catch(...){spdlog::error("Wheel item configuration save failed");}
   });
  auto& s=state(); const bool usable=ready(*frame);s.inputReady.store(usable);
  rock_configurator::setAvailable(usable);
  if(!usable) {++s.generation;s.toggle={};cancelAction();clearSuppression();if(s.open.load())closeWheel();return;}
  if(s.world!=frame->worldGeneration || s.skeleton!=frame->skeletonGeneration || s.provider!=frame->providerGeneration) {
   ++s.generation;s.toggle={};cancelAction();if(s.open.load())closeWheel();
   s.world=frame->worldGeneration;s.skeleton=frame->skeletonGeneration;s.provider=frame->providerGeneration;
  }
  serviceAction(*frame);
  if(rock_configurator::takeInventoryRefreshRequest() && s.open.load())refreshWheelInventory();
  if(s.configRequested.exchange(false) && s.open.load()) {
   if(rock_configurator::isOpen())rock_configurator::close();
   else {
    const auto& p=s.wheelPose;
    devui::render::PanelPose pose;
    pose.center={p.center[0],p.center[1],p.center[2]};
    pose.right={p.right[0],p.right[1],p.right[2]};
    pose.up={p.up[0],p.up[1],p.up[2]};
    pose.front={p.front[0],p.front[1],p.front[2]};
    pose.physicalWidth=p.physicalWidth;pose.physicalHeight=p.physicalHeight;
    rock_configurator::openBeside(pose);
   }
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
  s.wheelPose=*pose;
  const auto* tasks=F4SE::GetTaskInterface();if(!tasks)return;
  const auto ticket=++s.generation;
  tasks->AddTask([p=*pose,ticket] {
   auto& s=state();if(!s.inputReady.load() || s.generation.load()!=ticket)return;
   try {
    auto inventory=readInventory();
    auto& shared=sharedModel();
    {std::scoped_lock lock(shared.mutex);
     inventory.category=shared.model.category;
     if(inventory.status=="Select something to take" && !shared.lastAction.empty())inventory.status=shared.lastAction;
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
void closeWheel(){auto& s=state();s.open.store(false);++s.generation;s.configRequested.store(false);rock_configurator::close();(void)presentPanel(false);}
void toggleConfig(){if(isOpen())state().configRequested.store(true);}
void activateItem(std::uint32_t id) {
 auto& s=state();const auto ticket=s.generation.load();
 if(!id || !s.open.load() || !s.inputReady.load() || s.actionPending.exchange(true))return;
 actionStatus("Taking item into a free hand...");
 s.requestGeneration.store(ticket);s.requestedItem.store(id);
}
bool startRuntime() {
 PWSTR documents=nullptr;
 if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DEFAULT,nullptr,&documents)) || !documents)return false;
 const auto configPath=std::filesystem::path(documents)/"My Games"/"Fallout4VR"/"ROCKWheelMenu"/"ROCKWheelMenu.ini";
 CoTaskMemFree(documents);
 loadWheelConfig(configPath);
 if(!installPanel())return false;
 struct RegistrationRollback {
  bool complete{};
  ~RegistrationRollback(){if(!complete){devui::render::Shutdown();unregisterPanel();}}
 } rollback;
 devui::render::PrepareFonts();
 if(!devui::render::InstallFrameworkPanel())return false;
 const auto result=RockProviderApi::initialize(ROCK_PROVIDER_API_VERSION,ROCK_PROVIDER_API_V1_COMMAND_CANCELLATION_TABLE_BYTES);
 auto* api=RockProviderApi::inst;
 if(result || !api || !api->registerConsumerV1 || !api->unregisterConsumerV1 ||
  !api->registerFrameCallbackForOwnerV1 || !api->getRawWandButtonStateV1 ||
  !api->setHandInputSuppressionV1 || !api->clearHandInputSuppressionV1 ||
  !api->requestForceGrabV1 || !api->getInteractionCommandResultV1 || !api->cancelInteractionCommandV1)return false;
 if(!hasFeatureBitV1(RockProviderApi::negotiatedFeatureBits,RockProviderFeatureBitV1::InventoryForceGrab)) {
  spdlog::error("Wheel requires ROCK with inventory-to-hand support");return false;
 }
 RockProviderConsumerRegistrationV1 registration;
 std::snprintf(registration.modName,sizeof(registration.modName),"ROCK Wheel Menu");
 registration.requestedCapabilities=static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::FrameSnapshots)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::HandInputSuppression)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::InteractionCommands);
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
