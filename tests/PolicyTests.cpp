#include "WheelModel.h"
#include "WheelSelectionState.h"
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
 for(unsigned c=0;c<kCategoryCount+1;++c) {
  const float angle=-kPi/2+c*2*kPi/(kCategoryCount+1);
  check(hitCenter(std::cos(angle)*120,std::sin(angle)*120,1)==static_cast<int>(c));
  check(hitCenter(std::cos(angle)*60,std::sin(angle)*60,.5f)==static_cast<int>(c));
 }
 check(hitCenter(0,0,1)==kCancelNavigation);check(hitCenter(0,190,1)==-1);
 check(isEquipment(Category::Weapons) && isEquipment(Category::Armor));
 check(!isEquipment(Category::Aid) && !isEquipment(Category::Grenades));
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
