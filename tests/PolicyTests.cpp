#include "WheelModel.h"
#include "WheelSelectionState.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("Wheel policy check failed");}
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
 HoldGesture gesture;
 check(gesture.update(true,true)==HoldEdge::None); // Held on load must release first.
 check(gesture.update(true,false)==HoldEdge::None);
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(true,true)==HoldEdge::None);
 check(gesture.update(true,false)==HoldEdge::Release);
 check(gesture.update(true,true,true)==HoldEdge::None); // Native use owns the press.
 check(!gesture.down);
 check(gesture.update(true,true,false)==HoldEdge::None); // Looking away while held cannot steal it.
 check(!gesture.down);
 check(gesture.update(true,false,false)==HoldEdge::None);
 check(gesture.update(true,true,false)==HoldEdge::Open);
 check(gesture.update(true,true,true)==HoldEdge::None && gesture.down); // Existing wheel hold keeps ownership.
 check(gesture.update(false,true,true)==HoldEdge::None && !gesture.down); // Native menu cancels.
 check(gesture.update(true,true,false)==HoldEdge::None); // B held across menu exit stays native.
 check(gesture.update(true,false,false)==HoldEdge::None);
 check(gesture.update(true,true,false)==HoldEdge::Open);
 check(gesture.update(true,false,false)==HoldEdge::Release);
 check(gesture.update(true,false)==HoldEdge::None); // Exactly one selection.
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(false,true)==HoldEdge::None); // Menu/provider loss cancels.
 check(gesture.update(true,true)==HoldEdge::None);
 check(gesture.update(true,false)==HoldEdge::None);
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(true,false)==HoldEdge::Release);
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
