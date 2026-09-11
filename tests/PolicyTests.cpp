#include "WheelModel.h"
#include "WheelSelectionState.h"
#include "ItemIconPolicy.h"
#include "physics-interaction/input/NativeVatsInputSuppressionPolicy.h"
#include <stdexcept>
#include <iostream>
#include <limits>
void check(bool b){if(!b)throw std::runtime_error("Wheel policy check failed");}
void checkVatsArbitration() {
 using namespace wheel;
 namespace native=rock::native_vats_input_suppression_policy;
 for(bool wheelFirst:{false,true})for(bool suppressVats:{false,true}) {
  HoldGesture gesture;native::RuntimeState vats;
  bool suppressed=false,previousHeld=false;double pressedAt=0;
  HoldEdge edge=HoldEdge::None;
  const auto sample=[&](bool held,double now) {
   if(held && !previousHeld)pressedAt=now;
   const auto updateWheel=[&] {
    edge=gesture.update(true,held,now);
    suppressed=gesture.down || edge==HoldEdge::Release;
   };
   if(wheelFirst)updateWheel();
   const auto decision=native::update(vats,{
    .buttonDown=held,.justPressed=held && !previousHeld,.released=!held && previousHeld,
    .heldSeconds=static_cast<float>(now-pressedAt),
    .suppressVats=suppressVats,.suppressVans=true,.reserveHoldGesture=true,.suppressAll=suppressed});
   if(!wheelFirst)updateWheel();
   previousHeld=held;return decision;
  };
  (void)sample(false,0);
  for(double press:{1.0,1.125,1.25}) { // Repeated short taps never claim wheel input.
   (void)sample(true,press);check(edge==HoldEdge::None && !suppressed);
   const auto release=sample(false,press+.0625);
   check(edge==HoldEdge::None && !suppressed);
   check(release.forwardNative==!suppressVats && release.vatsSuppressed==suppressVats);
  }
  (void)sample(true,2.0);check(edge==HoldEdge::None && !suppressed);
  (void)sample(true,2.125);check(edge==HoldEdge::None && !suppressed);
  (void)sample(true,2.25);check(edge==HoldEdge::Open && suppressed);
  (void)sample(true,2.5);check(edge==HoldEdge::None && suppressed);
  const auto release=sample(false,2.625);
  check(edge==HoldEdge::Release && release.vatsSuppressed && !release.forwardNative);
  (void)sample(false,2.75);check(edge==HoldEdge::None && !suppressed);
  (void)sample(true,3.0);check(edge==HoldEdge::None && !suppressed);
  check(sample(false,3.125).forwardNative==!suppressVats); // Hold ownership cannot swallow the next tap.
 }
}
int main(){try {
 using namespace wheel;
 check(hitSlot(0,-250,180,336)==0);check(hitSlot(250,0,180,336)==2);
 check(hitSlot(0,0,180,336)==-1);check(hitSlot(0,400,180,336)==-1);
 // Every built-in visibility combination, with zero through ten registered mod
 // sections, keeps Config reachable and hits the same sectors that are drawn.
 for(unsigned mask=0;mask<64;++mask)for(unsigned mods=0;mods<=palm::api::kMaxSections;++mods) {
  Model model;for(unsigned c=0;c<kCategoryCount;++c)model.enabled[c]=(mask&(1u<<c))!=0;
  model.gesturesEnabled=(mask&32)!=0;model.sections.count=mods;
  for(unsigned i=0;i<mods;++i){model.sections.sections[i].handle=i+1;model.sections.sections[i].enabled=true;}
  const auto layout=navigationLayout(model);
  check(layout.count>=1 && layout.count<=palm::api::kMaxVisibleEntries);
  check(layout.entries[layout.count-1]==kConfigNavigation);
  for(unsigned slot=0;slot<layout.count;++slot) {
   const float angle=layout.angle(slot);const auto id=layout.entries[slot];
   check(hitCenter(std::cos(angle)*120,std::sin(angle)*120,1,layout)==static_cast<int>(id));
   check(hitCenter(std::cos(angle)*60,std::sin(angle)*60,.5f,layout)==static_cast<int>(id));
   const float gap=angle+layout.step/2;
   check(hitCenter(std::cos(gap)*120,std::sin(gap)*120,1,layout)==-1);
   if(id==kRightGesturesNavigation)check(std::cos(angle)>0);
   if(id==kLeftGesturesNavigation)check(std::cos(angle)<0);
   const auto category=navigationCategory(id);if(category>=0)check(model.enabled[category]);
  }
  check(hitCenter(0,0,1,layout)==kCancelNavigation);check(hitCenter(0,190,1,layout)==-1);
  model.category=Category::Weapons;normalizeWheelSelection(model);
  if(model.enabled[3])check(model.category==Category::Weapons);
  else if(mask&31)check(model.enabled[static_cast<unsigned>(model.category)]);
  else if(model.gesturesEnabled)check(model.gestures.showing);
  else check(!mods || model.activeSection!=0);
 }
 check(isEquipment(Category::Weapons) && isEquipment(Category::Armor));
 check(!isEquipment(Category::Aid) && !isEquipment(Category::Grenades));
 check(knownItemIcon(true,0x00023736)==Icon::Stimpak);
 check(knownItemIcon(true,0x00023742)==Icon::RadAway);
 check(knownItemIcon(false,0x00023736)==Icon::Automatic); // A mod's matching local ID is a different item.
 const auto weaponArt=[](Icon known,std::uint8_t shape,std::initializer_list<std::uint32_t> keywords) {
  return weaponIcon(known,shape,[&](std::uint32_t id){return std::find(keywords.begin(),keywords.end(),id)!=keywords.end();});
 };
 check(weaponArt(Icon::Automatic,9,{0x0004A0A0})==Icon::Pistol10mm);
 check(weaponArt(Icon::Automatic,9,{0x0004A0A1})==Icon::CombatRifle);
 check(weaponArt(Icon::Automatic,9,{0x0004A0A1,0x0004A0A3})==Icon::Minigun);
 check(weaponArt(Icon::LaserRifle,9,{0x0004A0A0,0x00092A84})==Icon::LaserPistol);
 check(weaponArt(Icon::LaserRifle,9,{0x0004A0A1,0x00092A84})==Icon::LaserRifle);
 check(weaponArt(Icon::Automatic,9,{0x0004A0A1,0x00092A85})==Icon::PlasmaRifle);
 check(weaponArt(Icon::Automatic,2,{})==Icon::CombatKnife);
 check(weaponArt(Icon::Automatic,11,{})==Icon::Mine);
 check(armorIcon(1u<<12,false)==Icon::LeftArm && armorIcon(1u<<13,false)==Icon::RightArm);
 check(armorIcon(1u<<14,false)==Icon::LeftLeg && armorIcon(1u<<15,false)==Icon::RightLeg);
 check(armorIcon(1u<<11,true)==Icon::PowerArmor);
 check(armorIcon((1u<<3)|(1u<<4)|(1u<<5),false)==Icon::Clothing);
 Item first{0x1234,"Rifle",1,false,"variant1",0}, second{0x1234,"Rifle",1,false,"variant2",3};
 check(selectionToken(first)!=selectionToken(second));
 checkVatsArbitration();
 HoldGesture gesture;
 check(gesture.update(true,true,0)==HoldEdge::None); // Held on load must release first.
 check(gesture.update(true,true,1)==HoldEdge::None && !gesture.pending && !gesture.down);
 check(gesture.update(true,false,2)==HoldEdge::None);
 check(gesture.update(true,true,3)==HoldEdge::None && gesture.pending && !gesture.down);
 check(gesture.update(true,true,3.249)==HoldEdge::None && !gesture.down);
 check(gesture.update(true,true,3.25)==HoldEdge::Open && gesture.down);
 check(gesture.update(true,true,4)==HoldEdge::None && gesture.down);
 check(gesture.update(true,false,5)==HoldEdge::Release);
 check(gesture.update(true,false,6)==HoldEdge::None);

 check(gesture.update(true,true,7,true)==HoldEdge::None); // Native activation owns this press.
 check(gesture.update(true,true,8,false)==HoldEdge::None && !gesture.down && !gesture.pending);
 check(gesture.update(true,false,9)==HoldEdge::None);
 check(gesture.update(true,true,10)==HoldEdge::None);
 check(gesture.update(true,true,10.25,true)==HoldEdge::Open); // Moving onto a target cannot reclassify a pending hold.
 check(gesture.update(false,true,11)==HoldEdge::None && !gesture.down); // Menu/provider loss cancels.
 check(gesture.update(true,true,12)==HoldEdge::None); // Still held after the menu; must release.
 check(gesture.update(true,false,13)==HoldEdge::None);
 check(gesture.update(true,true,14)==HoldEdge::None);
 check(gesture.update(false,true,14.125)==HoldEdge::None && !gesture.pending); // Cancellation before qualification.
 check(gesture.update(true,true,15)==HoldEdge::None && !gesture.down);
 check(gesture.update(true,false,16)==HoldEdge::None);
 check(gesture.update(true,true,17)==HoldEdge::None);
 check(gesture.update(true,true,16)==HoldEdge::None && !gesture.pending); // Invalid clock sample fails closed.
 check(gesture.update(true,false,18)==HoldEdge::None);
 check(gesture.update(true,true,19)==HoldEdge::None);
 check(gesture.update(true,true,std::numeric_limits<double>::quiet_NaN())==HoldEdge::None && !gesture.pending);
 // An invisible wheel closes on release without waiting for a render callback.
 WheelSelectionState selection;
 selection.begin(41);
 check(!selection.release().has_value());
 check(selection.generation==0);
 check(!selection.publish(41,{selectionToken(first),false}));
 check(!selection.release().has_value());

 // A visible wheel uses the latest drawn hover, including an explicit cancel.
 selection.begin(42);
 check(selection.publish(42,{selectionToken(first),false}));
 check(selection.publish(42,{selectionToken(second),false}));
 auto chosen=selection.release();
 check(chosen && chosen->hoveredItem==selectionToken(second) && !chosen->configHovered);
 check(!selection.release().has_value()); // Exactly one selection.
 selection.begin(43);
 check(selection.publish(43,{selectionToken(first),false}));
 check(selection.publish(43,{}));
 chosen=selection.release();
 check(chosen && !chosen->hoveredItem && !chosen->configHovered);

 // A cancelled/opening session cannot publish into the next opening or save.
 selection.begin(44);
 check(selection.publish(44,{selectionToken(first),false}));
 selection.begin(45);
 check(!selection.publish(44,{selectionToken(second),false}));
 check(!selection.release().has_value());
 selection.begin(46);
 check(selection.publish(46,{0,true}));
 chosen=selection.release();
 check(chosen && chosen->configHovered && !chosen->hoveredItem);
 check(!selection.publish(46,{selectionToken(first),false}));
 std::cout<<"Wheel selection, center navigation, cancellation and B-hold policies passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
