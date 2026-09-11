#include "PCH.h"
#include "Runtime.h"
#include "Inventory.h"
#include "Equipment.h"
#include "WheelConfig.h"
#include "Renderer.h"
#include "WheelSelectionState.h"
#include "Gestures.h"
#include "ROCKProviderApi.h"
#include "tools/ConfiguratorRuntime.h"
#include "tools/render/FrameworkPanelRenderer.h"
#include <RE/Bethesda/SendPapyrusEvent.h>

namespace wheel {
namespace {
using namespace rock::provider;
constexpr std::uint32_t kBButton=1;
constexpr std::uint64_t kCancelChoice=1,kConfigChoice=2,kSectionChoice=3,kItemChoice=std::uint64_t{1}<<63;
struct RuntimeState {
 std::atomic_bool open{false},held{false},inputReady{false};
 std::atomic_bool gameActionPending{false};
 // A confirmed presentation failure yields to grenade mode until the next load.
 std::atomic_bool presentationFailed{false};
 std::atomic_bool sessionReady{false},sessionResetPending{true};
 std::atomic_uint64_t generation{1},choice{0},choiceGeneration{0};
 std::atomic_uint64_t sectionChoice{0};
 // Serializes panel open/close on the game task and provider callback threads,
 // and short selection publication from the renderer. Never acquires the model
 // or render mutex while held; framework callbacks run outside its own lock.
 std::mutex presentationMutex;
 WheelSelectionState selection;
 rpsui::sdk::PanelPoseV1 wheelPose;
 std::uint64_t owner{},callback{},command{};
 HoldGesture gesture;
 Gestures handGestures;
 bool suppression{},frameworkUnavailable{};
 std::uint32_t world{},skeleton{},provider{};
};
RuntimeState& state(){static RuntimeState s;return s;}
void refreshWheelInventory() {
 const auto ticket=state().generation.load();
 if(const auto* tasks=F4SE::GetTaskInterface())tasks->AddTask([ticket] {
  if(!state().sessionReady.load() || (!state().open.load() && !rock_configurator::isOpen()) || state().generation.load()!=ticket)return;
  try {
   auto inventory=readInventory();auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
   if(!state().sessionReady.load() || state().generation.load()!=ticket)return;
   inventory.category=shared.model.category;inventory.status=shared.lastAction;inventory.gestures=shared.model.gestures;inventory.activeSection=shared.model.activeSection;
   shared.model=std::move(inventory);shared.view={};
  }catch(...){spdlog::error("PALM inventory refresh failed");}
 });
}
void actionStatus(const char* message) {
 auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
 shared.lastAction=message;shared.model.status=message;
}
void publishGestureState() {
 auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
 const auto& view=state().handGestures.view();
 shared.model.gestures.active=view.active;shared.model.gestures.availability=view.availability;
}
void clearSuppression() {
 auto& s=state();
 if(s.suppression && RockProviderApi::inst)
  (void)RockProviderApi::inst->clearHandInputSuppressionV1(s.owner,RockProviderHand::Right);
 s.suppression=false;
}
bool claimInput(const RockProviderFrameSnapshot& frame,bool gestureOwned) {
 auto& s=state();RockProviderHandInputSuppressionRequestV1 request;
 request.hand=RockProviderHand::Right;
 request.flags=static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressGrenadeQuickDraw);
 if(gestureOwned)request.flags|=static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressOpenVrGameInput)|
  static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressNativeVats)|
  static_cast<std::uint32_t>(RockProviderHandInputSuppressionFlagV1::SuppressConfigModeChord);
 request.leaseFrames=3;request.worldGeneration=frame.worldGeneration;
 request.skeletonGeneration=frame.skeletonGeneration;request.providerGeneration=frame.providerGeneration;
 const auto result=RockProviderApi::inst->setHandInputSuppressionV1(s.owner,&request);
 if(result!=RockProviderResultV1::Ok) {
  spdlog::error("PALM could not claim grenade-mode input: {}; grenade mode restored until the next load",static_cast<unsigned>(result));
  s.presentationFailed=true;clearSuppression();return false;
 }
 s.suppression=true;return true;
}
void cancelCommand() {
 auto& s=state();
 if(s.command && RockProviderApi::inst)(void)RockProviderApi::inst->cancelInteractionCommandV1(s.owner,s.command);
 s.command=0;
}
void serviceCommand() {
 auto& s=state();if(!s.command)return;
 RockProviderInteractionCommandResultV1 result;
 const auto query=RockProviderApi::inst->getInteractionCommandResultV1(s.owner,s.command,&result);
 if(query==RockProviderResultV1::Ok && result.state==RockProviderInteractionCommandStateV1::Queued)return;
 const bool success=query==RockProviderResultV1::Ok && result.state==RockProviderInteractionCommandStateV1::Succeeded;
 const char* message=success?(result.hand==RockProviderHand::Left?"Taken into left hand":"Taken into right hand"):
  result.failure==RockProviderInteractionFailureV1::HandBusy?"No free hand — put something down and try again":
  result.failure==RockProviderInteractionFailureV1::TargetAlreadyOwned?"A throwable is already held or attaching":"Item handoff failed";
 actionStatus(message);
 spdlog::info("PALM handoff {}: {} (query {}, failure {})",s.command,message,static_cast<unsigned>(query),static_cast<unsigned>(result.failure));
 if(query!=RockProviderResultV1::Ok)cancelCommand();else s.command=0;
}
bool ready(const RockProviderFrameSnapshot& f) {
 return f.providerReady && !f.menuBlocking && !f.configBlocking &&
  hasLifecycleFlag(f.lifecycleFlags,RockProviderLifecycleFlag::WorldAvailable) &&
  hasLifecycleFlag(f.lifecycleFlags,RockProviderLifecycleFlag::SkeletonReady);
}
std::optional<rpsui::sdk::PanelPoseV1> poseFor(const RockProviderFrameSnapshot& frame) {
 const auto& t=frame.rightHandTransform;float x=t.rotate[0],y=t.rotate[1];const float n=std::hypot(x,y);
 if(!std::isfinite(n) || n<.05f)return {};
 x/=n;y/=n;rpsui::sdk::PanelPoseV1 p;
 p.center[0]=t.translate[0]+x*85;p.center[1]=t.translate[1]+y*85;p.center[2]=t.translate[2]+9;
 p.right[0]=y;p.right[1]=-x;p.right[2]=0;p.front[0]=-x;p.front[1]=-y;p.front[2]=0;
 p.physicalWidth=95;p.physicalHeight=95;
 for(float v:p.center)if(!std::isfinite(v) || std::fabs(v)>1.e8f)return {};
 return p;
}
void submitChoice(const RockProviderFrameSnapshot& frame) {
 auto& s=state();const auto choice=s.choice.exchange(0);
 const auto ticket=s.choiceGeneration.load();
 if(!choice || ticket!=s.generation.load())return;
 if(choice==kConfigChoice) {
  s.handGestures.clear(s.owner);
  const auto& p=s.wheelPose;devui::render::PanelPose pose;
  pose.center={p.center[0],p.center[1],p.center[2]};
  pose.right={p.right[0],p.right[1],p.right[2]};pose.up={p.up[0],p.up[1],p.up[2]};
  pose.front={p.front[0],p.front[1],p.front[2]};
  pose.physicalWidth=p.physicalWidth;pose.physicalHeight=p.physicalHeight;
  rock_configurator::openAt(pose);
  spdlog::info("B release selected Config; replacing wheel at its anchor");
 }else if(choice==kSectionChoice) {
  s.handGestures.clear(s.owner);
  const auto selected=s.sectionChoice.load();
  const auto section=static_cast<palm::api::SectionHandle>(selected>>32),item=static_cast<std::uint32_t>(selected);
  const auto* tasks=F4SE::GetTaskInterface();
  if(!tasks){actionStatus("Mod action task queue unavailable");return;}
  s.gameActionPending=true;
  try {tasks->AddTask([ticket,section,item] {
   auto& runtime=state();
   struct Finish {RuntimeState& state;~Finish(){state.gameActionPending=false;}} finish{runtime};
   if(!runtime.sessionReady.load() || !runtime.inputReady.load() || runtime.generation.load()!=ticket)return;
   try {
    const auto result=sectionRegistry().dispatch(section,item);
    if(result!=palm::api::Result::Ok){actionStatus("Mod selection is no longer available");spdlog::warn("PALM mod action rejected: section={}, item={}, result={}",section,item,static_cast<unsigned>(result));}
   }catch(...){spdlog::error("PALM mod action dispatch failed");}
  });}catch(...){s.gameActionPending=false;throw;}
 }else if(isGestureChoice(choice)) {
  const auto* message=s.handGestures.select(s.owner,static_cast<unsigned>(choice),frame);
  actionStatus(message);
  auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
  const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{};
  if(!vm || !Papyrus::detail::DispatchStaticCall(vm,RE::BSFixedString{"Debug"},RE::BSFixedString{"Notification"},callback,RE::BSFixedString{message}))
   spdlog::warn("PALM gesture notification unavailable: {}",message);
  publishGestureState();
 }else if(choice&kItemChoice) {
  s.handGestures.clear(s.owner);
  const auto token=choice&~kItemChoice;
  std::optional<Item> equipment;
  {
   auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
   for(unsigned c=static_cast<unsigned>(Category::Weapons);c<kCategoryCount;++c)
    for(const auto& item:shared.model.items[c])if(selectionToken(item)==token)equipment=item;
  }
  if(equipment) {
   const auto* tasks=F4SE::GetTaskInterface();
   if(!tasks){actionStatus("Equipment task queue unavailable");return;}
   s.gameActionPending=true;
   try { tasks->AddTask([ticket,item=*equipment] {
    auto& runtime=state();
    struct Finish {RuntimeState& state;~Finish(){state.gameActionPending=false;}} finish{runtime};
    if(!runtime.sessionReady.load() || !runtime.inputReady.load() || runtime.generation.load()!=ticket)return;
    try { actionStatus(toggleEquipment(item,runtime.owner)); }
    catch(...){spdlog::error("Equipment result publication failed");}
   }); } catch(...) {s.gameActionPending=false;throw;}
   return;
  }
  // Only an item in the current non-equipment snapshot may enter the handoff path.
  bool takeToHand=false;
  {
   auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
   for(unsigned c=0;c<static_cast<unsigned>(Category::Weapons);++c)
    for(const auto& item:shared.model.items[c])takeToHand|=selectionToken(item)==token && item.count>0;
  }
  if(!takeToHand){actionStatus("Selection is no longer available");return;}
  RockProviderForceGrabRequestV1 request;
  request.flags=static_cast<std::uint32_t>(RockProviderForceGrabFlagV1::FromPlayerInventory);
  request.targetFormId=static_cast<std::uint32_t>(choice);
  request.worldGeneration=frame.worldGeneration;request.skeletonGeneration=frame.skeletonGeneration;
  request.providerGeneration=frame.providerGeneration;
  const auto result=RockProviderApi::inst->requestForceGrabV1(s.owner,&request,&s.command);
  if(result!=RockProviderResultV1::RequestQueued) {
   s.command=0;actionStatus(result==RockProviderResultV1::HandBusy?"Hands are busy — try again":"Item handoff unavailable");
  }
  spdlog::info("B release selected item {:08X}: queue result {}",request.targetFormId,static_cast<unsigned>(result));
 }
}
void openHeldWheel(const RockProviderFrameSnapshot& frame) {
 auto& s=state();const auto pose=poseFor(frame);const auto* tasks=F4SE::GetTaskInterface();
 if(!pose || !tasks){failWheelPresentation(0);return;}
 std::uint64_t ticket;
 {std::scoped_lock lock(s.presentationMutex);
  s.wheelPose=*pose;ticket=++s.generation;s.selection.begin(ticket);}
 tasks->AddTask([p=*pose,ticket] {
  auto& s=state();
  if(!s.sessionReady.load() || !s.inputReady.load() || !s.held.load() || s.generation.load()!=ticket)return;
  try {
   auto inventory=readInventory();auto& shared=sharedModel();
   {std::scoped_lock lock(shared.mutex);
    if(!s.sessionReady.load() || s.generation.load()!=ticket)return;
    inventory.category=Category::Weapons;inventory.gestures=shared.model.gestures;inventory.gestures.showing=false;
    if(!shared.lastAction.empty())inventory.status=shared.lastAction;
    shared.model=std::move(inventory);shared.view={};}
   std::scoped_lock presentationLock(s.presentationMutex);
   if(!s.sessionReady.load() || !s.inputReady.load() || !s.held.load() || s.generation.load()!=ticket)return;
   s.open=true;
   if(!presentPanel(true,&p)){
    s.open=false;s.presentationFailed=true;++s.generation;(void)s.selection.release();
    spdlog::warn("PALM presentation failed; grenade mode restored until the next load");
   }
   else spdlog::info("B-held wheel opened, generation {}",ticket);
  }catch(...){failWheelPresentation(ticket);}
 });
}
void releaseHeldWheel() {
 auto& s=state();std::scoped_lock lock(s.presentationMutex);
 const auto hover=s.selection.release();
 const auto ticket=++s.generation; // Also cancels a queued open and any in-flight draw.
 if(!s.open.exchange(false))return;
 (void)presentPanel(false);
 s.choiceGeneration=ticket;
 if(hover && hover->section)s.sectionChoice=(static_cast<std::uint64_t>(hover->section)<<32)|hover->sectionItem;
 s.choice=hover?(hover->configHovered?kConfigChoice:hover->hoveredGesture?hover->hoveredGesture:
  hover->section?kSectionChoice:hover->hoveredItem?(kItemChoice|hover->hoveredItem):kCancelChoice):kCancelChoice;
 if(!hover){s.presentationFailed=true;spdlog::warn("PALM had no rendered frame; grenade mode restored until the next load; check RPS_UI_Framework.log");}
 else spdlog::info("B release closed wheel using its last drawn selection, generation {}",ticket);
}
void ROCK_PROVIDER_CALL onFrame(const RockProviderFrameSnapshot* frame,void*) noexcept {
 try {
  if(!frame)return;auto& s=state();
  if(takeWheelConfigChange())refreshWheelInventory();
  const bool gameplayReady=s.sessionReady.load() && ready(*frame);
  const auto nativeContext=gameplayReady?RockProviderApi::inst->getNativeInputContextV1():0u;
  const bool contextAvailable=(nativeContext&static_cast<std::uint32_t>(RockProviderNativeInputContextFlagV1::Available))!=0;
  const bool nativeMenu=(nativeContext&static_cast<std::uint32_t>(RockProviderNativeInputContextFlagV1::MenuActive))!=0;
  const bool nativeTarget=(nativeContext&static_cast<std::uint32_t>(RockProviderNativeInputContextFlagV1::PrimaryActivationTarget))!=0;
  const bool wasOwning=s.open.load() || s.held.load() || rock_configurator::isOpen() || rock_configurator::isOpening();
  const bool nativeReady=gameplayReady && contextAvailable && !nativeMenu;
  const bool hostReady=nativeReady && frameworkReady();
  const bool hostUnavailable=!hostReady;
  if(nativeReady && s.frameworkUnavailable!=hostUnavailable) {
   s.frameworkUnavailable=hostUnavailable;
   if(hostReady)spdlog::info("RPS UI Framework readiness recovered");
   else spdlog::warn("PALM input unavailable: RPS UI Framework is not ready; check RPS_UI_Framework.log");
  }
  const bool usable=nativeReady && hostReady && !s.presentationFailed.load();
  s.inputReady=usable;rock_configurator::setAvailable(usable);
  const bool changed=s.sessionResetPending.exchange(false) || s.world!=frame->worldGeneration || s.skeleton!=frame->skeletonGeneration || s.provider!=frame->providerGeneration;
  if(!usable || changed) {
   if(wasOwning)spdlog::info("PALM input released: nativeMenu={}, contextAvailable={}, providerMenu={}, lifecycleChanged={}",nativeMenu,contextAvailable,frame->menuBlocking,changed);
   s.handGestures.clear(s.owner);publishGestureState();
   closeWheel();cancelCommand();clearSuppression();s.gesture={};
   s.world=frame->worldGeneration;s.skeleton=frame->skeletonGeneration;s.provider=frame->providerGeneration;
   return;
  }
  s.handGestures.update(s.owner,*frame,!s.command && !s.gameActionPending.load() &&
   !rock_configurator::isOpen() && !rock_configurator::isOpening());
  publishGestureState();
  serviceCommand();
  submitChoice(*frame);
  if(rock_configurator::takeInventoryRefreshRequest())refreshWheelInventory();
  RockProviderRawWandButtonStateV1 button;
  if(!RockProviderApi::inst->getRawWandButtonStateV1(RockProviderHand::Right,kBButton,&button) || !button.available) {
   s.handGestures.clear(s.owner);publishGestureState();
   closeWheel();cancelCommand();clearSuppression();s.gesture={};return;
  }
  const bool eligible=!s.command && !s.gameActionPending.load() && !rock_configurator::isOpen() && !rock_configurator::isOpening();
  if(eligible && nativeTarget && button.held && s.gesture.armed && !s.gesture.down)
   spdlog::info("PALM B deferred to native activation target until physical release");
  const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  const bool wasPending=s.gesture.pending;
  const double heldSeconds=now-s.gesture.pressedAt;
  const auto edge=s.gesture.update(eligible,button.held!=0,now,nativeTarget);
  s.held=s.gesture.down;
  if(eligible && wasPending && !button.held)
   spdlog::info("B tap left to native input: held {:.3f}s; wheel did not claim input",heldSeconds);
  if(edge==HoldEdge::Open)
   spdlog::info("B hold qualified after {:.3f}s; wheel owns input through release",heldSeconds);
  // Renew grenade-mode suppression while usable. Only a qualified wheel
  // gesture masks gameplay input, including its physical release so it cannot
  // become a native VATS tap.
  if(!claimInput(*frame,s.gesture.down || edge==HoldEdge::Release)) {
   s.handGestures.clear(s.owner);publishGestureState();
   s.inputReady=false;closeWheel();cancelCommand();s.gesture={};return;
  }
  if(edge==HoldEdge::Open)openHeldWheel(*frame);
  else if(edge==HoldEdge::Release)releaseHeldWheel();
 }catch(...){
  spdlog::error("PALM input failed; grenade mode restored until the next load");
  state().presentationFailed=true;state().inputReady=false;
  state().handGestures.clear(state().owner);
  cancelCommand();clearSuppression();state().gesture={};closeWheel();
 }
}
}
SharedModel& sharedModel(){static SharedModel s;return s;}
std::uint64_t wheelDrawGeneration() {
 auto& s=state();std::scoped_lock lock(s.presentationMutex);
 return s.open.load()?s.generation.load():0;
}
void publishWheelSelection(std::uint64_t ticket,const Action& hover) {
 auto& s=state();std::scoped_lock lock(s.presentationMutex);
 if(s.open.load() && s.generation.load()==ticket)(void)s.selection.publish(ticket,hover);
}
void closeWheel(std::uint64_t expectedGeneration){
 auto& s=state();
 {std::scoped_lock lock(s.presentationMutex);
  if(expectedGeneration && s.generation.load()!=expectedGeneration)return;
  s.open=false;s.held=false;s.choice=0;++s.generation;(void)s.selection.release();
  (void)presentPanel(false);}
 rock_configurator::close();
}
void failWheelPresentation(std::uint64_t expectedGeneration) {
 auto& s=state();
 {std::scoped_lock lock(s.presentationMutex);
  if(expectedGeneration && s.generation.load()!=expectedGeneration)return;
  s.presentationFailed=true;s.inputReady=false;
 }
 spdlog::error("PALM presentation unavailable; grenade mode restored until the next load");
 closeWheel(expectedGeneration);
 // The owning input callback clears the claim, or its three-frame lease expires.
}
void beginGameLoad() {
 auto& s=state();s.sessionReady=false;s.inputReady=false;s.sessionResetPending=true;
 rock_configurator::setAvailable(false);closeWheel();
 s.presentationFailed=false; // Old render tickets have now been invalidated.
 sectionRegistry().clearItems();
 restoreWheelPreferences({});
 auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
 shared.model={};shared.view={};shared.lastAction.clear();
 // Provider commands and suppression are cancelled by its next unusable frame,
 // on their owning callback thread. Queued game tasks fail the generation check.
}
void finishGameLoad(bool success){state().sessionReady=success;}
bool startRuntime() {
 if(!validateEquipmentRuntime())return false;
 if(!installPanel())return false;
 struct RegistrationRollback {
  bool complete{};
  ~RegistrationRollback(){if(!complete){devui::render::Shutdown();unregisterPanel();}}
 } rollback;
 devui::render::PrepareFonts();
 if(!devui::render::InstallFrameworkPanel())return false;
 constexpr auto requiredTableBytes=static_cast<std::uint32_t>(offsetof(RockProviderApi,getNativeInputContextV1)+sizeof(std::declval<RockProviderApi>().getNativeInputContextV1));
 const auto result=RockProviderApi::initialize(ROCK_PROVIDER_API_VERSION,requiredTableBytes);
 auto* api=RockProviderApi::inst;
 if(result || !api || !api->registerConsumerV1 || !api->unregisterConsumerV1 ||
  !api->registerFrameCallbackForOwnerV1 || !api->getRawWandButtonStateV1 || !api->getNativeInputContextV1 ||
  !api->setHandInputSuppressionV1 || !api->clearHandInputSuppressionV1 ||
  !api->requestForceGrabV1 || !api->getInteractionCommandResultV1 || !api->cancelInteractionCommandV1 || !api->getHandInteractionStateV1)return false;
 if(!hasFeatureBitV1(RockProviderApi::negotiatedFeatureBits,RockProviderFeatureBitV1::InventoryForceGrab)) {
  spdlog::error("PALM requires ROCK with inventory-to-hand support");return false;
 }
 RockProviderConsumerRegistrationV1 registration;
 std::snprintf(registration.modName,sizeof(registration.modName),"RobCo PALM");
 registration.requestedCapabilities=static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::FrameSnapshots)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::HandInputSuppression)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::InteractionCommands)|
  static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::HandInteractionState);
 const bool gestureSupport=api->setHandVisualAuthorityV1 && api->clearHandVisualAuthorityV1 && supportsHandVisualAuthorityV1();
 if(gestureSupport)registration.requestedCapabilities|=static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::HandVisualAuthority);
 RockProviderConsumerHandleV1 handle;
 if(api->registerConsumerV1(&registration,&handle)!=RockProviderResultV1::Ok || !handle.ownerToken)return false;
 if((handle.grantedCapabilities&registration.requestedCapabilities)!=registration.requestedCapabilities) {
  (void)api->unregisterConsumerV1(handle.ownerToken);return false;
 }
 auto& s=state();s.owner=handle.ownerToken;
 if(gestureSupport)s.handGestures.initialize();
 if(api->registerFrameCallbackForOwnerV1(s.owner,onFrame,nullptr,&s.callback)!=RockProviderResultV1::Ok || !s.callback) {
  (void)api->unregisterConsumerV1(s.owner);s.owner=0;return false;
 }
 rollback.complete=true;
 spdlog::info("PALM registered with ROCK; tap right B for native input, hold {:.2f}s for the wheel, release over an item or Config",kWheelHoldSeconds);
 return true;
}
}
