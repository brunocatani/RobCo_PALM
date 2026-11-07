#include "tools/RockConfigurationClient.h"
#include "RockServices.h"
#include "PCH.h"
#include "Runtime.h"
#include "DeveloperActions.h"
#include "PalmControls.h"
#include "api/VirtualHolstersAPI.h"
#include "RpsUiQuery.h"
#include <RPS_UI_Framework/Input.h>
#include <ShlObj.h>
#include "Inventory.h"
#include "Equipment.h"
#include "WheelConfig.h"
#include "Renderer.h"
#include "WheelSelectionState.h"
#include "Gestures.h"
#include "GrenadeSelection.h"
#include "RockInputPolicy.h"
#include "RockFrame.h"
#include "tools/ConfiguratorRuntime.h"
#include "tools/render/FrameworkPanelRenderer.h"
#include <RE/Bethesda/SendPapyrusEvent.h>

namespace wheel {
namespace {

// kAddWeaponChoice+slot (slots 0..7) stays below the gesture choices at 16.
constexpr std::uint64_t kCancelChoice=1,kConfigChoice=2,kSectionChoice=3,kAddWeaponChoice=4,kItemChoice=std::uint64_t{1}<<63;
constexpr bool isAddWeaponChoice(std::uint64_t choice){return choice>=kAddWeaponChoice && choice<kAddWeaponChoice+kSlots;}
struct RuntimeState {
 std::atomic_bool open{false},held{false},inputReady{false};
 std::atomic_bool gameActionPending{false};
 // A confirmed presentation failure releases wheel input until the next load.
 std::atomic_bool presentationFailed{false};
 std::atomic_bool sessionReady{false},sessionResetPending{true};
 std::atomic_uint64_t generation{1},choice{0},choiceGeneration{0};
 std::atomic_uint64_t sectionChoice{0};
 std::atomic<PointerHand> choiceHand{PointerHand::None};
 // RockEquip::pack(), written by the equipment task and submitted by the input
 // callback on ROCK's owner thread; equipCommand is owned by that callback.
 std::atomic_uint64_t pendingEquip{0};
 std::uint64_t equipCommand{};
 // Serializes panel open/close on the game task and provider callback threads,
 // and short selection publication from the renderer. Never acquires the model
 // or render mutex while held; framework callbacks run outside its own lock.
 std::mutex presentationMutex;
 WheelSelectionState selection;
 rpsui::api::framework::PanelPose wheelPose;
 std::uint64_t owner{},command{};
 ControlGesture gesture;
 RockyInputPriority rockyPriority;
 bool rockLoaded{};
 Controls controls;
 // Owned by the input callback, like the gesture. No disk writes in that callback.
 InputMode loggedInputMode{InputMode::StickClick};
 bool loggedControls{},loggedUsable{};
 double nextOpeningReport{},nextAvailabilityReport{};
 // Borrowed for the loaded plugin's process lifetime; queried on the game
 // input callback thread, where Virtual Holsters owns its zone updates.
 VirtualHolstersAPI* holsters{};
 const rpsui::api::input::Api* inputApi{};
 std::uint64_t inputToken{};
 std::atomic_bool rockReady{false},clickRequested{false};
 wheel::RockFrame rockFrame;
 Action clickSelection;
 Gestures handGestures;
 bool leftHanded{};
 std::uint32_t world{},skeleton{},provider{};
};
RuntimeState& state(){static RuntimeState s;return s;}
template<class Log> void queueInputLog(Log log) noexcept {
 // Use the existing game-task path and capture only values. Diagnostics must
 // never turn a logging/queue failure into a gameplay input failure.
 try {
  if(const auto* tasks=F4SE::GetTaskInterface())tasks->AddTask([log=std::move(log)]() noexcept {
   try{log();}catch(...){OutputDebugStringA("PALM input diagnostic write failed\n");}
  });
 }catch(...){OutputDebugStringA("PALM input diagnostic queue failed\n");}
}
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
 const auto aim=snapshotPointerAim();rpsui::api::input::PointerAim request;
 for(unsigned side=0;side<2;++side){request.pitchDegrees[side]=nativePointerPitch(aim.pitch[side]);request.yawDegrees[side]=aim.yaw[side];}
 return state().inputApi && state().inputApi->setPointerAim && state().inputApi->setPointerAim(&request);
}
void clearSuppression() {
 auto& s=state();if(s.inputApi && s.inputToken){rpsui::api::input::Capture capture;(void)s.inputApi->capture(s.inputToken,&capture);}
}
bool claimInput(const rpsui::api::input::Frame& frame,bool owned) {
 auto& s=state();const auto masks=controlMasks(s.controls,frame.leftHanded);
 rpsui::api::input::Capture capture;
 // A plain click cannot reserve its press: FRIK/holsters must see it while
 // PALM waits for release to establish that it never became a chord.
 if(isPlainStickClick(s.controls) && !s.gesture.open){clearSuppression();return true;}
 for(unsigned hand=0;hand<2;++hand) {
  if(owned)capture.buttons[hand]=masks.buttons[hand];
  else capture.chord[hand]=masks.buttons[hand];
 }
 const bool accepted=s.inputApi && s.inputApi->capture(s.inputToken,&capture);
 if(!accepted){s.presentationFailed=true;clearSuppression();spdlog::error("PALM input capture unavailable");}
 return accepted;
}
void cancelCommand() {
 auto& s=state();
 if(s.command && rockServices().client.owner())(void)rockServices().grab->cancelInteractionCommand(s.owner,s.command);
 s.command=0;
}
void serviceCommand() {
 auto& s=state();if(!s.command)return;
 rock::api::grab::InteractionCommandResult result;
 const auto query=rockServices().grab->getInteractionCommandResult(s.owner,s.command,&result);
 if(query==rock::api::Status::Ok && result.state==rock::api::grab::InteractionCommandState::Queued)return;
 const bool success=query==rock::api::Status::Ok && result.state==rock::api::grab::InteractionCommandState::Succeeded;
 const char* message=success?(result.hand==rock::api::Hand::Left?"Taken into left hand":"Taken into right hand"):
  result.failure==rock::api::grab::InteractionFailure::HandBusy?"No free hand — put something down and try again":
  result.failure==rock::api::grab::InteractionFailure::TargetAlreadyOwned?"A throwable is already held or attaching":"Item handoff failed";
 actionStatus(message);
 spdlog::info("PALM handoff {}: {} (query {}, failure {})",s.command,message,static_cast<unsigned>(query),static_cast<unsigned>(result.failure));
 if(query!=rock::api::Status::Ok)cancelCommand();else s.command=0;
}
void serviceEquipCommand() {
 auto& s=state();
 if(const auto pending=s.pendingEquip.exchange(0)) {
  // A new selection supersedes the result of an earlier one.
  std::uint64_t command{};
  actionStatus(submitRockEquip(s.owner,RockEquip::unpack(pending),command));
  s.equipCommand=command;
  if(!command){refreshWheelInventory();return;}
 }
 const auto command=s.equipCommand;
 if(!command || !rockServices().weapon)return;
 rock::api::weapon::EquipResult result;
 const auto query=rockServices().weapon->getInventoryEquipResult(s.owner,command,&result);
 using State=rock::api::weapon::EquipState;
 if(query==rock::api::Status::Ok && (result.state==State::Queued || result.state==State::Switching))return;
 s.equipCommand=0;
 using Failure=rock::api::weapon::EquipFailure;
 const bool left=result.hand==rock::api::Hand::Left;
 const char* message=query!=rock::api::Status::Ok?"Weapon change result unavailable":
  result.state==State::Succeeded?(left?"Equipped in left hand":"Equipped in right hand"):
  result.failure==Failure::HandBusy?"That hand is busy — put down what it holds":
  result.failure==Failure::HandUnavailable?"That hand is unavailable":
  result.failure==Failure::InventoryChanged?"That weapon is no longer carried":
  result.state==State::Cancelled?"Weapon change cancelled":"The weapon change failed";
 actionStatus(message);
 spdlog::info("PALM inventory equip {}: {} (query {}, state {}, failure {}, slot {}, incoming {:08X}, outgoing {:08X})",command,message,
  static_cast<unsigned>(query),static_cast<unsigned>(result.state),static_cast<unsigned>(result.failure),result.weaponSlot,result.incomingFormId,result.outgoingFormId);
 refreshWheelInventory();
}

std::optional<rpsui::api::framework::PanelPose> poseFor(const rpsui::api::input::Frame& frame) {
 const auto& t=frame.hands[physicalHand(state().controls.binding.hand,frame.leftHanded)];
 if(!t.valid)return {};
 float x=t.forward[0],y=t.forward[1];const float n=std::hypot(x,y);
 if(!std::isfinite(n) || n<.05f)return {};
 x/=n;y/=n;rpsui::api::framework::PanelPose p;
 p.center[0]=t.position[0]+x*85;p.center[1]=t.position[1]+y*85;p.center[2]=t.position[2]+9;
 p.right[0]=y;p.right[1]=-x;p.right[2]=0;p.front[0]=-x;p.front[1]=-y;p.front[2]=0;
 p.physicalWidth=95;p.physicalHeight=95;
 for(float v:p.center)if(!std::isfinite(v) || std::fabs(v)>1.e8f)return {};
 return p;
}
void requestInventoryHandoff(std::uint32_t id,const wheel::RockFrame& frame) {
 auto& s=state();
 rock::api::grab::InventoryGrabRequest request;
 request.baseFormId=id;
 request.worldGeneration=frame.sample.worldGeneration;request.skeletonGeneration=frame.sample.skeletonGeneration;
 request.providerGeneration=frame.sample.providerGeneration;
 const auto result=rockServices().grab->requestInventoryGrab(s.owner,&request,&s.command);
 if(result!=rock::api::Status::RequestQueued) {
  s.command=0;actionStatus(result==rock::api::Status::HandBusy?"Hands are busy — try again":"Item handoff unavailable");
 }
 spdlog::info("Selected item {:08X}: queue result {}",request.baseFormId,static_cast<unsigned>(result));
}
void submitChoice(const wheel::RockFrame& frame) {
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
  const auto section=static_cast<palm::api::menu::SectionHandle>(selected>>32),item=static_cast<std::uint32_t>(selected);
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
 }else if(isAddWeaponChoice(choice)) {
  const auto slot=static_cast<unsigned>(choice-kAddWeaponChoice);
  s.handGestures.clear(s.owner);
  // The weapon whose grip the pointing hand holds; with that hand empty, the
  // weapon the other hand holds. Without ROCK only the native weapon exists.
  std::uint32_t equipIndex=0;
  if(s.rockReady.load()) {
   using rock::api::Hand;using rock::api::hands::kNoWeaponSlot;
   const auto pointing=s.choiceHand.load()==PointerHand::Left?Hand::Left:Hand::Right;
   const auto other=pointing==Hand::Left?Hand::Right:Hand::Left;
   equipIndex=handRole(frame.roles,pointing).weaponSlot;
   if(equipIndex==kNoWeaponSlot)equipIndex=handRole(frame.roles,other).weaponSlot;
   if(equipIndex==kNoWeaponSlot){actionStatus("Hold the weapon you want to add");return;}
  }
  const auto* tasks=F4SE::GetTaskInterface();
  if(!tasks){actionStatus("Equipment task queue unavailable");return;}
  s.gameActionPending=true;
  try { tasks->AddTask([ticket,equipIndex,slot] {
   auto& runtime=state();
   struct Finish {RuntimeState& state;~Finish(){state.gameActionPending=false;}} finish{runtime};
   if(!runtime.sessionReady.load() || !runtime.inputReady.load() || runtime.generation.load()!=ticket)return;
   try { actionStatus(addHeldWeaponToWheel(equipIndex,slot)); refreshWheelInventory(); }
   catch(...){spdlog::error("PALM add held weapon publication failed");}
  }); } catch(...) {s.gameActionPending=false;throw;}
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
   try { tasks->AddTask([ticket,item=*equipment,hand=s.choiceHand.load()] {
    auto& runtime=state();
    struct Finish {RuntimeState& state;~Finish(){state.gameActionPending=false;}} finish{runtime};
    if(!runtime.sessionReady.load() || !runtime.inputReady.load() || runtime.generation.load()!=ticket)return;
    try {
     RockEquip rock;
     actionStatus(toggleEquipment(item,runtime.rockReady.load()?runtime.owner:0,hand,rock));
     if(rock.form)runtime.pendingEquip=rock.pack();
     else refreshWheelInventory();
    }
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
    wheel::RockFrame current;
    if(grenade && runtime.rockReady.load() && rockServices().client.owner() &&
       rockServices().snapshot(current) && current.providerReady) {
     const auto connection=rock_configurator::configurationConnection();
     const auto immersive=immersiveGrenadesEnabled(connection.api,connection.owner);
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
void openHeldWheel(const rpsui::api::input::Frame& frame) {
 auto& s=state();const auto pose=poseFor(frame);const auto* tasks=F4SE::GetTaskInterface();
 if(!pose || !tasks){failWheelPresentation(0);return;}
 std::uint64_t ticket;
 {std::scoped_lock lock(s.presentationMutex);
  s.wheelPose=*pose;ticket=++s.generation;s.selection.begin(ticket);}
 tasks->AddTask([p=*pose,ticket,holdToSelect=s.controls.mode==OpenMode::Hold] {
  auto& s=state();
  if(!s.sessionReady.load() || !s.inputReady.load() || !s.held.load() || s.generation.load()!=ticket)return;
  try {
   developerActions().refresh();
   auto inventory=readInventory();auto& shared=sharedModel();
   {std::scoped_lock lock(shared.mutex);
    if(!s.sessionReady.load() || s.generation.load()!=ticket)return;
    inventory.category=Category::Weapons;inventory.gestures=shared.model.gestures;inventory.gestures.showing=false;inventory.holdToSelect=holdToSelect;
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
 if(!closeAfterSelect && (!hover || (!hover->cancelHovered && !hover->configHovered && !hover->hoveredGesture && !hover->section && !hover->hoveredItem && !hover->addHeldWeapon)))return;
 const bool close=closeAfterSelect || (hover && (hover->configHovered || hover->cancelHovered || developerActions().ownsSection(hover->section)));
 auto ticket=s.generation.load();
 if(close) {
  ticket=++s.generation;(void)s.selection.release();
  if(!s.open.exchange(false))return;
  s.held=false;(void)presentPanel(false);s.gesture.cancel(true);
 }
 s.choiceGeneration=ticket;
 if(hover && hover->section)s.sectionChoice=(static_cast<std::uint64_t>(hover->section)<<32)|hover->sectionItem;
 s.choiceHand=hover?hover->hand:PointerHand::None;
 s.choice=hover?(hover->configHovered?kConfigChoice:hover->hoveredGesture?hover->hoveredGesture:
  hover->section?kSectionChoice:hover->hoveredItem?(kItemChoice|hover->hoveredItem):
  hover->addHeldWeapon?kAddWeaponChoice+hover->addHeldWeapon-1:kCancelChoice):kCancelChoice;
 // Release may arrive before the first draw, including a skipped depth frame.
 // Cancelling this opening is not evidence that presentation is unavailable.
 if(!hover)spdlog::info("PALM opening cancelled before a rendered selection; waiting for both buttons to release");
}
void RPSUI_CALL onFrame(const rpsui::api::input::Frame* frame,void*) noexcept {
 try {
  if(!frame)return;auto& s=state();
  if(takePointerAimChange() && !publishPointerAim())spdlog::error("PALM pointer aim update rejected by UI framework");
  if(takeControlsSaveRequest())if(const auto* tasks=F4SE::GetTaskInterface())tasks->AddTask([]{persistControls();});
  InputMode inputMode;
  const auto controls=snapshotControls(&inputMode);
  const bool leftHanded=frame->leftHanded!=0;
  if(!s.loggedControls || inputMode!=s.loggedInputMode || controls!=s.controls || s.leftHanded!=leftHanded) {
   s.loggedControls=true;s.loggedInputMode=inputMode;
   queueInputLog([inputMode,controls,leftHanded,sequence=frame->sequence] {
    spdlog::info("PALM controls applied: mode={} selection={} binding='{}' leftHanded={} inputFrame={}",
     static_cast<unsigned>(inputMode),controls.mode==OpenMode::Hold?"release":"click",bindingText(controls),leftHanded,sequence);
   });
  }
  if(controls!=s.controls || s.leftHanded!=leftHanded){s.controls=controls;s.leftHanded=leftHanded;s.gesture={};s.clickRequested=false;if(s.open.load() || s.held.load())closeWheel();}
  if(takeWheelConfigChange())refreshWheelInventory();
  RockSnapshotDiagnostic snapshotDiagnostic;
  const bool rockReady=s.owner && rockServices().client.owner() && rockServices().snapshot(s.rockFrame,&snapshotDiagnostic) && s.rockFrame.providerReady;
  const bool providerChanged=s.rockReady.exchange(rockReady)!=rockReady ||
   (rockReady && (s.world!=s.rockFrame.sample.worldGeneration || s.skeleton!=s.rockFrame.sample.skeletonGeneration || s.provider!=s.rockFrame.sample.providerGeneration));
  setGestureIntegrationAvailable(rockReady && s.handGestures.available());
  const bool sessionReady=s.sessionReady.load(),uiReady=frameworkReady(),presentationFailed=s.presentationFailed.load();
  const bool usable=sessionReady && frame->ready && uiReady && !presentationFailed;
  if(s.loggedUsable!=usable && frame->seconds>=s.nextAvailabilityReport) {
   s.loggedUsable=usable;s.nextAvailabilityReport=frame->seconds+.5;
   queueInputLog([usable,sessionReady,uiReady,presentationFailed,ready=frame->ready,sequence=frame->sequence] {
    spdlog::info("PALM input availability: usable={} session={} input={} framework={} presentationFailed={} inputFrame={}",
     usable,sessionReady,ready,uiReady,presentationFailed,sequence);
   });
  }
  s.inputReady=usable;rock_configurator::setAvailable(usable);
  const bool reset=s.sessionResetPending.exchange(false);
  if(providerChanged) {
   s.handGestures.clear(s.owner);publishGestureState();cancelCommand();s.pendingEquip=0;s.equipCommand=0;
   s.world=s.rockFrame.sample.worldGeneration;s.skeleton=s.rockFrame.sample.skeletonGeneration;s.provider=s.rockFrame.sample.providerGeneration;
   refreshWheelInventory();
  }
  if(!usable || reset) {
   s.handGestures.clear(s.owner);publishGestureState();closeWheel();cancelCommand();s.pendingEquip=0;s.equipCommand=0;clearSuppression();s.gesture={};s.rockyPriority={};s.clickRequested=false;
   s.world=s.rockFrame.sample.worldGeneration;s.skeleton=s.rockFrame.sample.skeletonGeneration;s.provider=s.rockFrame.sample.providerGeneration;
   return;
  }
  serviceCommand();serviceEquipCommand();
  {
   // Asking only while the keyboard panel is up is what releases the keyboard otherwise:
   // the capture lapses by itself three frames after the last request.
   rpsui::api::input::KeyboardRead keyboard;
   std::array<rock_configurator::ConsoleKeyEvent,rpsui::api::input::kKeyboardEventCapacity> keys;
   const bool held=rock_configurator::keyboardInputActive() && s.inputApi->captureKeyboard && s.inputApi->captureKeyboard(s.inputToken,&keyboard);
   const std::uint32_t count=held?keyboard.count:0;
   for(std::uint32_t i=0;i<count;++i)keys[i]={keyboard.events[i].kind==rpsui::api::input::KeyboardEventKind::Character,keyboard.events[i].code};
   rock_configurator::serviceConsole(held,held && keyboard.focused!=0,std::span<const rock_configurator::ConsoleKeyEvent>(keys.data(),count));
  }
  if(rock_configurator::takeInventoryRefreshRequest())refreshWheelInventory();
  const bool gameActionPending=s.gameActionPending.load(),configOpen=rock_configurator::isOpen(),configOpening=rock_configurator::isOpening();
  const bool eligible=!s.command && !gameActionPending && !configOpen && !configOpening;
  const auto masks=controlMasks(s.controls,frame->leftHanded);
  const std::array pressed{frame->hands[0].pressed,frame->hands[1].pressed};
  const std::array<bool,2> handValid{frame->hands[0].valid!=0,frame->hands[1].valid!=0};
  const bool rockyOwnsInput=s.rockLoaded && s.rockyPriority.update(pressed,handValid);
  bool allDown=true,anyDown=false,valid=true;
  for(unsigned hand=0;hand<2;++hand)if(masks.buttons[hand]) {
   valid &= frame->hands[hand].valid!=0;
   allDown &= (frame->hands[hand].pressed&masks.buttons[hand])==masks.buttons[hand];
   anyDown |= (frame->hands[hand].pressed&masks.buttons[hand])!=0;
  }
  std::array<bool,2> inHolster{};
  // Optional provider readiness gates only its zone queries, never the wheel.
  const bool holstersReady=s.holsters && s.holsters->IsInitialized();
  if(holstersReady)for(unsigned hand=0;hand<2;++hand)
   if(masks.buttons[hand])inHolster[hand]=s.holsters->IsHandInHolsterZone(hand==0);
  bool bipodAllows=true;
  rock::api::weapon::WeaponState weapon;
  int weaponStatus=-1;
  if(rockReady) {
   bipodAllows=(rockServices().weapon!=nullptr) &&
    (weaponStatus=static_cast<int>(rockServices().weapon->getWeaponState(s.owner,0,&weapon)))==static_cast<int>(rock::api::Status::Ok) &&
    bipodAllowsOpening(weapon,s.rockFrame);
  }
  bool triggerEquipAllows=true;
  std::array<rock::api::grab::HandInteractionState,2> interactions;
  std::array<int,2> interactionStatus{-1,-1};
  for(unsigned hand=0;hand<2;++hand)if(s.rockLoaded && (masks.buttons[hand]&(1ull<<33))) {
   const auto physical=hand==0?rock::api::Hand::Left:rock::api::Hand::Right;
   auto& interaction=interactions[hand];
   triggerEquipAllows &= rockReady && (rockServices().grab!=nullptr) &&
    (interactionStatus[hand]=static_cast<int>(rockServices().grab->getHandInteractionState(s.owner,physical,&interaction)))==static_cast<int>(rock::api::Status::Ok) &&
    triggerEquipAllowsOpening(interaction,s.rockFrame,physical);
  }
  bool placementAllows=true;
  if(rockReady && (masks.buttons[1]&(1ull<<32))) {
   rock::api::input::PlacementClickState placement;
   placementAllows=rockServices().input->getPlacementClickState(s.owner,&placement)==rock::api::Status::Ok &&
    placementAllowsOpening(placement,s.rockFrame);
  }
  const bool openingAllowed=bipodAllows && placementAllows && triggerEquipAllows && openingInputAllowed(masks,pressed,handValid,inHolster);
  const bool openingBlocked=rockyOwnsInput || (!s.gesture.open && !openingAllowed);
  const bool ownedBefore=s.gesture.ownsInput();
  std::optional<Action> clicked;
  {std::scoped_lock lock(s.presentationMutex);if(s.clickRequested.exchange(false))clicked=s.clickSelection;}
  const auto before=s.gesture;
  const auto edge=s.gesture.update(valid && (eligible || s.gesture.open || s.gesture.draining),s.controls,allDown,anyDown,clicked.has_value() && eligible,frame->seconds,openingAllowed,rockyOwnsInput);
  // At most two reports per second, only while an opening attempt is active.
  // Repeated held samples also expose a gesture continually reset by a guard.
  if(!before.open && (anyDown || before.pending || edge==ControlEdge::Open) &&
     std::isfinite(frame->seconds) && frame->seconds>=s.nextOpeningReport) {
   s.nextOpeningReport=frame->seconds+.5;
   queueInputLog([inputMode,sequence=frame->sequence,seconds=frame->seconds,thread=GetCurrentThreadId(),
    pressed,masks,handValid,allDown,anyDown,eligible,gameActionPending,configOpen,configOpening,command=s.command,
    holstersReady,inHolster,bipodAllows,triggerEquipAllows,rockyOwnsInput,openingAllowed,before,after=s.gesture,edge,
    rockLoaded=s.rockLoaded,rockReady,rockFrame=s.rockFrame,snapshotDiagnostic,weaponStatus,weapon,interactionStatus,interactions] {
    spdlog::info("PALM opening input: mode={} inputFrame={} thread={} pressed[L,R]=[{:X},{:X}] binding[L,R]=[{:X},{:X}] valid[L,R]=[{},{}] allDown={} anyDown={} eligible={} config[open,opening]=[{},{}] actionPending={} command={} holstersReady={} zones[L,R]=[{},{}] bipodAllows={} triggerAllows={} rocky={} openingAllowed={}",
     static_cast<unsigned>(inputMode),sequence,thread,pressed[0],pressed[1],masks.buttons[0],masks.buttons[1],handValid[0],handValid[1],
     allDown,anyDown,eligible,configOpen,configOpening,gameActionPending,command,holstersReady,inHolster[0],inHolster[1],bipodAllows,triggerEquipAllows,rockyOwnsInput,openingAllowed);
    spdlog::info("PALM opening gesture: inputFrame={} before[armed,pending,open,draining]=[{},{},{},{}] after=[{},{},{},{}] heldSeconds={:.3f} edge={}",
     sequence,before.armed,before.pending,before.open,before.draining,after.armed,after.pending,after.open,after.draining,
     before.pending?seconds-before.pressedAt:0.,edge==ControlEdge::Open?"open":edge==ControlEdge::Select?"select":"none");
    if(rockLoaded && !rockReady) {
     const auto& observed=snapshotDiagnostic.observed;
     spdlog::info("PALM opening ROCK snapshot: inputFrame={} stage={} status={} (-1=not-called,0=ok,17=wrong-thread) expected[frame,world,skeleton,provider]=[{},{},{},{}] observed=[{},{},{},{}]",
      sequence,snapshotDiagnostic.stage,snapshotDiagnostic.status,rockFrame.sample.frameIndex,rockFrame.sample.worldGeneration,rockFrame.sample.skeletonGeneration,rockFrame.sample.providerGeneration,
      observed.frameIndex,observed.worldGeneration,observed.skeletonGeneration,observed.providerGeneration);
    }
    if(!bipodAllows)spdlog::info("PALM opening bipod: inputFrame={} status={} flags={:X} expected[frame,world,skeleton,provider]=[{},{},{},{}] observed=[{},{},{},{}]",
     sequence,weaponStatus,weapon.flags,rockFrame.sample.frameIndex,rockFrame.sample.worldGeneration,rockFrame.sample.skeletonGeneration,rockFrame.sample.providerGeneration,
     weapon.sample.frameIndex,weapon.sample.worldGeneration,weapon.sample.skeletonGeneration,weapon.sample.providerGeneration);
    if(!triggerEquipAllows)for(unsigned hand=0;hand<2;++hand)if(masks.buttons[hand]&(1ull<<33)) {
     const auto& interaction=interactions[hand];
     spdlog::info("PALM opening trigger: inputFrame={} physicalHand={} status={} reportedHand={} flags={:X} expected[frame,world,skeleton,provider]=[{},{},{},{}] observed=[{},{},{},{}]",
      sequence,hand==0?"left":"right",interactionStatus[hand],static_cast<unsigned>(interaction.hand),interaction.flags,
      rockFrame.sample.frameIndex,rockFrame.sample.worldGeneration,rockFrame.sample.skeletonGeneration,rockFrame.sample.providerGeneration,
      interaction.frameIndex,interaction.worldGeneration,interaction.skeletonGeneration,interaction.providerGeneration);
    }
   });
  }
  s.held=s.gesture.open;
  if(s.open.load() && !s.gesture.open && edge!=ControlEdge::Select)closeWheel();
  if(!openingBlocked && ((eligible && s.gesture.armed) || ownedBefore || s.gesture.ownsInput())) {
   if(!claimInput(*frame,ownedBefore || s.gesture.ownsInput())){closeWheel();s.gesture={};return;}
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
 initializeEquipmentRuntime();
 if(!installPanel())return false;
 struct RegistrationRollback {
  bool complete{};
  ~RegistrationRollback(){if(!complete){devui::render::Shutdown();unregisterPanel();}}
 } rollback;
 devui::render::PrepareFonts();
 if(!devui::render::InstallFrameworkPanel())return false;

 auto& s=state();
 if(rpsui::api::acquire(rpsUiQuery(),s.inputApi)!=rpsui::api::Result::Ok) {
  spdlog::error("PALM startup failed: RPS UI independent input API unavailable");return false;
 }
 PWSTR documents{};
 if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents))) {
  const auto path=std::filesystem::path(documents)/"My Games"/"Fallout4VR"/"Mods_Config"/"RobCo_PALM"/"PALM.ini";
  CoTaskMemFree(documents);initializeControls(path);if(takeControlsSaveRequest())persistControls();
  spdlog::info("PALM settings path: {}",path.string());
 }else {spdlog::error("PALM Documents folder unavailable");return false;}
 s.controls=snapshotControls();setGestureIntegrationAvailable(false);
 s.rockLoaded=rock_configurator::loadedRockMod().has_value();
 s.holsters=RequestVirtualHolstersAPI();
 if(s.holsters && s.holsters->GetVersion()!=1)s.holsters=nullptr;
 if(GetModuleHandleW(L"VirtualHolsters.dll") && !s.holsters)
  spdlog::warn("PALM Virtual Holsters zone guard disabled: loaded provider has no supported zone API; wheel input remains available");
 else spdlog::info("PALM Virtual Holsters zone guard: {}",s.holsters?"available":"provider absent");
 (void)takePointerAimChange();if(!publishPointerAim()) {
  spdlog::error("PALM startup failed: RPS UI rejected pointer calibration");return false;
 }
 if(rockServices().connect()) {
  s.owner=rockServices().client.owner();
  if(rockServices().animation && rockServices().input)s.handGestures.initialize();
 }
 if(s.rockLoaded && !s.owner)
  spdlog::warn("ROCK integration unavailable; PALM trigger bindings wait for held-weapon state, other bindings and vanilla items remain available");
 s.inputToken=s.inputApi->subscribe(onFrame,nullptr);
 if(!s.inputToken){if(s.owner)(void)rockServices().client.close();s.owner=0;
  spdlog::error("PALM startup failed: RPS UI input subscription rejected");return false;}
 rollback.complete=true;
 spdlog::info("PALM standalone input registered; optional ROCK owner={}, controls={}",s.owner,bindingText(s.controls));
 return true;
}
}
