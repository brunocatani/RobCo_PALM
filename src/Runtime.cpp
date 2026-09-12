#include "PCH.h"
#include "Runtime.h"
#include "PalmControls.h"
#include "PalmInputPriority.h"
#include "RPSUIInputApi.h"
#include <ShlObj.h>
#include "Inventory.h"
#include "Equipment.h"
#include "WheelConfig.h"
#include "Renderer.h"
#include "WheelSelectionState.h"
#include "Gestures.h"
#include "GrenadeSelection.h"
#include "ROCKProviderApi.h"
#include "tools/ConfiguratorRuntime.h"
#include "tools/render/FrameworkPanelRenderer.h"
#include <RE/Bethesda/SendPapyrusEvent.h>

namespace wheel {
namespace {
using namespace rock::provider;
constexpr std::uint64_t kCancelChoice=1,kConfigChoice=2,kSectionChoice=3,kItemChoice=std::uint64_t{1}<<63;
struct RuntimeState {
 std::atomic_bool open{false},held{false},inputReady{false};
 std::atomic_bool gameActionPending{false};
 // A confirmed presentation failure releases wheel input until the next load.
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
 std::uint64_t owner{},command{};
 ControlGesture gesture;
 Controls controls;
 const rpsui::sdk::InputApiV1* inputApi{};
 std::uint64_t inputToken{};
 std::atomic_bool rockReady{false},clickRequested{false};
 RockProviderFrameSnapshot rockFrame;
 Action clickSelection;
 Gestures handGestures;
 bool leftHanded{};
 std::uint32_t world{},skeleton{},provider{};
 double nextHoldRejectionLog{};
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

bool publishPointerAim() {
 const auto aim=snapshotPointerAim();rpsui::sdk::PointerAimV1 request;
 for(unsigned side=0;side<2;++side){request.pitchDegrees[side]=aim.pitch[side];request.yawDegrees[side]=aim.yaw[side];}
 return state().inputApi && state().inputApi->setPointerAim && state().inputApi->setPointerAim(&request);
}
void clearSuppression() {
 auto& s=state();if(s.inputApi && s.inputToken){rpsui::sdk::InputCaptureV1 capture;(void)s.inputApi->capture(s.inputToken,&capture);}
}
bool claimInput(const rpsui::sdk::InputFrameV1& frame,ControlCapture mode) {
 auto& s=state();const auto masks=controlMasks(s.controls,frame.leftHanded);
 rpsui::sdk::InputCaptureV1 capture;
 for(unsigned hand=0;hand<2;++hand) {
  if(mode==ControlCapture::Buttons)capture.buttons[hand]=masks.buttons[hand];
  else if(mode==ControlCapture::Chord)capture.chord[hand]=masks.buttons[hand];
 }
 const bool accepted=s.inputApi && s.inputApi->capture(s.inputToken,&capture);
 if(!accepted){s.presentationFailed=true;clearSuppression();spdlog::error("PALM input capture unavailable");}
 return accepted;
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

std::optional<rpsui::sdk::PanelPoseV1> poseFor(const rpsui::sdk::InputFrameV1& frame) {
 const auto& t=frame.hands[physicalHand(state().controls.binding.hand,frame.leftHanded)];
 if(!t.valid)return {};
 float x=t.forward[0],y=t.forward[1];const float n=std::hypot(x,y);
 if(!std::isfinite(n) || n<.05f)return {};
 x/=n;y/=n;rpsui::sdk::PanelPoseV1 p;
 p.center[0]=t.position[0]+x*85;p.center[1]=t.position[1]+y*85;p.center[2]=t.position[2]+9;
 p.right[0]=y;p.right[1]=-x;p.right[2]=0;p.front[0]=-x;p.front[1]=-y;p.front[2]=0;
 p.physicalWidth=95;p.physicalHeight=95;
 for(float v:p.center)if(!std::isfinite(v) || std::fabs(v)>1.e8f)return {};
 return p;
}
void requestInventoryHandoff(std::uint32_t id,const RockProviderFrameSnapshot& frame) {
 auto& s=state();
 RockProviderForceGrabRequestV1 request;
 request.flags=static_cast<std::uint32_t>(RockProviderForceGrabFlagV1::FromPlayerInventory);
 request.targetFormId=id;
 request.worldGeneration=frame.worldGeneration;request.skeletonGeneration=frame.skeletonGeneration;
 request.providerGeneration=frame.providerGeneration;
 const auto result=RockProviderApi::inst->requestForceGrabV1(s.owner,&request,&s.command);
 if(result!=RockProviderResultV1::RequestQueued) {
  s.command=0;actionStatus(result==RockProviderResultV1::HandBusy?"Hands are busy — try again":"Item handoff unavailable");
 }
 spdlog::info("Selected item {:08X}: queue result {}",request.targetFormId,static_cast<unsigned>(result));
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
  spdlog::info("Selected Config; replacing wheel at its anchor");
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
    try { actionStatus(toggleEquipment(item,runtime.rockReady.load()?runtime.owner:0)); refreshWheelInventory(); }
    catch(...){spdlog::error("Equipment result publication failed");}
   }); } catch(...) {s.gameActionPending=false;throw;}
   return;
  }
  // Only an item in the current non-equipment snapshot may enter the handoff path.
  bool takeToHand=false,grenade=false;
  {
   auto& shared=sharedModel();std::scoped_lock lock(shared.mutex);
   for(unsigned c=0;c<static_cast<unsigned>(Category::Weapons);++c)
    for(const auto& item:shared.model.items[c])if(selectionToken(item)==token && item.count>0) {
     takeToHand=true;grenade=c==static_cast<unsigned>(Category::Grenades);
    }
  }
  if(!takeToHand){actionStatus("Selection is no longer available");return;}

  if(!s.rockReady.load() || grenade) {
   const auto* tasks=F4SE::GetTaskInterface();
   if(!tasks){actionStatus("Inventory task queue unavailable");return;}
   s.gameActionPending=true;
   try{tasks->AddTask([ticket,id=static_cast<std::uint32_t>(choice),grenade] {
    auto& runtime=state();struct Finish{RuntimeState& s;~Finish(){s.gameActionPending=false;}} finish{runtime};
    try {
    if(!runtime.sessionReady.load() || !runtime.inputReady.load() || runtime.generation.load()!=ticket)return;
    RockProviderFrameSnapshot current;
    if(grenade && runtime.rockReady.load() && RockProviderApi::inst &&
       RockProviderApi::inst->getFrameSnapshot(&current) && current.providerReady) {
     const auto module=GetModuleHandleW(L"ROCK.dll");
     const auto get=module?reinterpret_cast<rock::configuration_api::GetApiV1>(GetProcAddress(module,rock::configuration_api::kExportName)):nullptr;
     const auto immersive=immersiveGrenadesEnabled(get?get(rock::configuration_api::kVersion):nullptr);
     if(!immersive) {
      actionStatus("ROCK grenade setting unavailable");spdlog::warn("PALM grenade selection could not read ROCK's applied Immersive Grenades setting");return;
     }
     if(*immersive){requestInventoryHandoff(id,current);return;}
    }
    actionStatus(useInventoryItem(id));refreshWheelInventory();
    }catch(...){spdlog::error("PALM item selection task failed");}
   });}catch(...){s.gameActionPending=false;throw;}
   return;
  }
  requestInventoryHandoff(static_cast<std::uint32_t>(choice),frame);
 }
}
void openHeldWheel(const rpsui::sdk::InputFrameV1& frame) {
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
    spdlog::warn("PALM presentation failed; wheel input released until the next load");
   }
   else spdlog::info("PALM wheel opened, generation {}",ticket);
  }catch(...){failWheelPresentation(ticket);}
 });
}

void selectWheel(bool closeAfterSelect,const std::optional<Action>& clicked) {
 auto& s=state();std::scoped_lock lock(s.presentationMutex);
 const auto hover=closeAfterSelect?s.selection.drawn:clicked;
 if(!closeAfterSelect && (!hover || (!hover->cancelHovered && !hover->configHovered && !hover->hoveredGesture && !hover->section && !hover->hoveredItem)))return;
 const bool close=closeAfterSelect || (hover && (hover->configHovered || hover->cancelHovered));
 auto ticket=s.generation.load();
 if(close) {
  ticket=++s.generation;(void)s.selection.release();
  if(!s.open.exchange(false))return;
  s.held=false;(void)presentPanel(false);s.gesture.cancel(true);
 }
 s.choiceGeneration=ticket;
 if(hover && hover->section)s.sectionChoice=(static_cast<std::uint64_t>(hover->section)<<32)|hover->sectionItem;
 s.choice=hover?(hover->configHovered?kConfigChoice:hover->hoveredGesture?hover->hoveredGesture:
  hover->section?kSectionChoice:hover->hoveredItem?(kItemChoice|hover->hoveredItem):kCancelChoice):kCancelChoice;
 if(!hover){s.presentationFailed=true;spdlog::warn("PALM had no rendered selection; input released until next load");}
}
void RPSUI_CALL onFrame(const rpsui::sdk::InputFrameV1* frame,void*) noexcept {
 try {
  if(!frame)return;auto& s=state();
  if(takePointerAimChange() && !publishPointerAim())spdlog::error("PALM pointer aim update rejected by UI framework");
  if(takeControlsSaveRequest())if(const auto* tasks=F4SE::GetTaskInterface())tasks->AddTask([]{persistControls();});
  const auto controls=snapshotControls();
  if(controls!=s.controls || s.leftHanded!=frame->leftHanded){s.controls=controls;s.leftHanded=frame->leftHanded;s.gesture={};s.clickRequested=false;if(s.open.load() || s.held.load())closeWheel();}
  if(takeWheelConfigChange())refreshWheelInventory();
  const bool rockReady=s.owner && RockProviderApi::inst && RockProviderApi::inst->getFrameSnapshot(&s.rockFrame) && s.rockFrame.providerReady;
  const bool providerChanged=s.rockReady.exchange(rockReady)!=rockReady ||
   (rockReady && (s.world!=s.rockFrame.worldGeneration || s.skeleton!=s.rockFrame.skeletonGeneration || s.provider!=s.rockFrame.providerGeneration));
  setGestureIntegrationAvailable(rockReady && s.handGestures.available());
  const bool usable=s.sessionReady.load() && frame->ready && frameworkReady() && !s.presentationFailed.load();
  s.inputReady=usable;rock_configurator::setAvailable(usable);
  const bool reset=s.sessionResetPending.exchange(false);
  if(providerChanged) {
   s.handGestures.clear(s.owner);publishGestureState();cancelCommand();
   s.world=s.rockFrame.worldGeneration;s.skeleton=s.rockFrame.skeletonGeneration;s.provider=s.rockFrame.providerGeneration;
   refreshWheelInventory();
  }
  if(!usable || reset) {
   s.handGestures.clear(s.owner);publishGestureState();closeWheel();cancelCommand();clearSuppression();s.gesture={};s.clickRequested=false;
   s.world=s.rockFrame.worldGeneration;s.skeleton=s.rockFrame.skeletonGeneration;s.provider=s.rockFrame.providerGeneration;
   return;
  }
  serviceCommand();
  if(rock_configurator::takeInventoryRefreshRequest())refreshWheelInventory();
  const bool eligible=!s.command && !s.gameActionPending.load() && !rock_configurator::isOpen() && !rock_configurator::isOpening();
  const auto masks=controlMasks(s.controls,frame->leftHanded);
  bool allDown=true,anyDown=false,valid=true;
  for(unsigned hand=0;hand<2;++hand)if(masks.buttons[hand]) {
   valid &= frame->hands[hand].valid;
   allDown &= (frame->hands[hand].pressed&masks.buttons[hand])==masks.buttons[hand];
   anyDown |= (frame->hands[hand].pressed&masks.buttons[hand])!=0;
  }
  const bool ownedBefore=s.gesture.ownsInput();
  bool interactionBusy=false;
  unsigned priorityHand{},priorityPhase{},priorityFlags{},priorityResult{};
  if(rockReady && eligible && valid && anyDown && isTimedHold(s.controls.binding.type) && !ownedBefore && !s.gesture.draining) {
   for(unsigned side=0;side<2;++side)if(masks.buttons[side]) {
    const auto hand=side?RockProviderHand::Right:RockProviderHand::Left;
    RockProviderHandInteractionStateV1 interaction;
    const auto result=RockProviderApi::inst->getHandInteractionStateV1(s.owner,hand,&interaction);
    interactionBusy = result!=RockProviderResultV1::Ok || interaction.hand!=hand ||
     interaction.worldGeneration!=s.rockFrame.worldGeneration || interaction.skeletonGeneration!=s.rockFrame.skeletonGeneration ||
     interaction.providerGeneration!=s.rockFrame.providerGeneration || interactionBlocksOpeningHold(interaction);
    if(interactionBusy) {
     priorityHand=static_cast<unsigned>(hand);priorityPhase=static_cast<unsigned>(interaction.phase);
     priorityFlags=interaction.flags;priorityResult=static_cast<unsigned>(result);break;
    }
   }
  }
  std::optional<Action> clicked;
  {std::scoped_lock lock(s.presentationMutex);if(s.clickRequested.exchange(false))clicked=s.clickSelection;}
  const auto edge=s.gesture.update(valid && (eligible || s.gesture.open || s.gesture.draining),s.controls,allDown,anyDown,clicked.has_value() && eligible,frame->seconds,interactionBusy);
  if(anyDown && s.gesture.rejection!=HoldRejection::None && frame->seconds>=s.nextHoldRejectionLog) {
   s.nextHoldRejectionLog=frame->seconds+2;
   spdlog::info("PALM hold yielded to {}; release opening buttons to rearm (left={:X}, right={:X}, complete={}, ROCK hand={}, phase={}, flags={:X}, query={})",
    s.gesture.rejection==HoldRejection::Interaction?"ROCK interaction":"existing button hold",
    frame->hands[0].pressed&masks.buttons[0],frame->hands[1].pressed&masks.buttons[1],allDown,
    priorityHand,priorityPhase,priorityFlags,priorityResult);
  }
  s.held=s.gesture.open;
  if(s.open.load() && !s.gesture.open && edge!=ControlEdge::Select)closeWheel();
  const auto capture=s.gesture.captureMode(s.controls,eligible,ownedBefore);
  if(capture!=ControlCapture::None) {
   if(!claimInput(*frame,capture)){closeWheel();s.gesture={};return;}
  }else clearSuppression();
  if(rockReady)s.handGestures.update(s.owner,s.rockFrame,eligible,ownedBefore || s.gesture.ownsInput());
  publishGestureState();
  submitChoice(s.rockFrame);
  if(edge==ControlEdge::Open)openHeldWheel(*frame);
  else if(edge==ControlEdge::Select)selectWheel(s.controls.mode==OpenMode::Hold,clicked);
 }catch(...){
  spdlog::error("PALM input failed; input released until the next load");
  state().presentationFailed=true;state().inputReady=false;
  state().handGestures.clear(state().owner);cancelCommand();clearSuppression();state().gesture={};closeWheel();
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

void requestWheelClick(std::uint64_t ticket,const Action& action) {
 auto& s=state();std::scoped_lock lock(s.presentationMutex);
 if(s.open.load() && s.generation.load()==ticket && !s.clickRequested.load()) {
  s.clickSelection=action;s.clickRequested=true;
 }
}
void closeWheel(std::uint64_t expectedGeneration){
 auto& s=state();
 {std::scoped_lock lock(s.presentationMutex);
  if(expectedGeneration && s.generation.load()!=expectedGeneration)return;
  s.open=false;s.held=false;s.choice=0;s.clickRequested=false;++s.generation;(void)s.selection.release();
  (void)presentPanel(false);}
 rock_configurator::close();
}
void failWheelPresentation(std::uint64_t expectedGeneration) {
 auto& s=state();
 {std::scoped_lock lock(s.presentationMutex);
  if(expectedGeneration && s.generation.load()!=expectedGeneration)return;
  s.presentationFailed=true;s.inputReady=false;
 }
 spdlog::error("PALM presentation unavailable; wheel input released until the next load");
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

 auto& s=state();
 s.inputApi=rpsui::sdk::RequestInputApiV1();
 if(!s.inputApi || !s.inputApi->subscribe || !s.inputApi->capture || !s.inputApi->unsubscribe || !s.inputApi->setPointerAim)return false;
 PWSTR documents{};
 if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents))) {
  const auto path=std::filesystem::path(documents)/"My Games"/"Fallout4VR"/"Mods_Config"/"RobCo_PALM"/"PALM.ini";
  CoTaskMemFree(documents);initializeControls(path);if(takeControlsSaveRequest())persistControls();
  spdlog::info("PALM settings path: {}",path.string());
 }else {spdlog::error("PALM Documents folder unavailable");return false;}
 s.controls=snapshotControls();setGestureIntegrationAvailable(false);
 (void)takePointerAimChange();if(!publishPointerAim())return false;
 constexpr auto requiredTableBytes=static_cast<std::uint32_t>(offsetof(RockProviderApi,cancelInteractionCommandV1)+sizeof(std::declval<RockProviderApi>().cancelInteractionCommandV1));
 const auto initialized=RockProviderApi::initialize(ROCK_PROVIDER_API_VERSION,requiredTableBytes);
 auto* api=RockProviderApi::inst;
 if(!initialized && api && api->registerConsumerV1 && api->unregisterConsumerV1 && api->getFrameSnapshot &&
    api->requestForceGrabV1 && api->getInteractionCommandResultV1 && api->cancelInteractionCommandV1 &&
    api->getHandInteractionStateV1 && hasFeatureBitV1(RockProviderApi::negotiatedFeatureBits,RockProviderFeatureBitV1::InventoryForceGrab)) {
  RockProviderConsumerRegistrationV1 registration;
  std::snprintf(registration.modName,sizeof(registration.modName),"RobCo PALM");
  registration.requestedCapabilities=static_cast<unsigned>(RockProviderConsumerCapabilityV1::FrameSnapshots)|
   static_cast<unsigned>(RockProviderConsumerCapabilityV1::InteractionCommands)|static_cast<unsigned>(RockProviderConsumerCapabilityV1::HandInteractionState);
  const bool gestures=api->setHandVisualAuthorityV1 && api->clearHandVisualAuthorityV1 && supportsHandVisualAuthorityV1();
  if(gestures)registration.requestedCapabilities|=static_cast<unsigned>(RockProviderConsumerCapabilityV1::HandVisualAuthority);
  RockProviderConsumerHandleV1 handle;
  if(api->registerConsumerV1(&registration,&handle)==RockProviderResultV1::Ok &&
     (handle.grantedCapabilities&registration.requestedCapabilities)==registration.requestedCapabilities) {
   s.owner=handle.ownerToken;if(gestures)s.handGestures.initialize();
  }else if(handle.ownerToken)(void)api->unregisterConsumerV1(handle.ownerToken);
 }
 if(GetModuleHandleW(L"ROCK.dll") && !s.owner)
  spdlog::warn("ROCK item/gesture integration unavailable; PALM continues with native controls and vanilla items");
 s.inputToken=s.inputApi->subscribe(onFrame,nullptr);
 if(!s.inputToken){if(s.owner)(void)api->unregisterConsumerV1(s.owner);s.owner=0;return false;}
 rollback.complete=true;
 spdlog::info("PALM standalone input registered; optional ROCK owner={}, controls={}",s.owner,bindingText(s.controls));
 return true;
}
}
