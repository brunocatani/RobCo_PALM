#include "WheelModel.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("Wheel policy check failed");}
int main(){try {
 using namespace wheel;
 check(hitSlot(0,-250,180,336)==0);check(hitSlot(250,0,180,336)==2);
 check(hitSlot(0,0,180,336)==-1);check(hitSlot(0,400,180,336)==-1);
 check(hitCenter(0,-120,1)==0);check(hitCenter(120,0,1)==1);
 check(hitCenter(0,120,1)==2);check(hitCenter(-120,0,1)==3);
 check(hitCenter(0,0,1)==4);check(hitCenter(0,190,1)==-1);
 check(hitCenter(60,0,.5f)==1);
 HoldGesture gesture;
 check(gesture.update(true,true)==HoldEdge::None); // Held on load must release first.
 check(gesture.update(true,false)==HoldEdge::None);
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(true,true)==HoldEdge::None);
 check(gesture.update(true,false)==HoldEdge::Release);
 check(gesture.update(true,false)==HoldEdge::None); // Exactly one selection.
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(false,true)==HoldEdge::None); // Menu/provider loss cancels.
 check(gesture.update(true,true)==HoldEdge::None);
 check(gesture.update(true,false)==HoldEdge::None);
 check(gesture.update(true,true)==HoldEdge::Open);
 check(gesture.update(true,false)==HoldEdge::Release);
 std::cout<<"Wheel selection, center navigation, cancellation and B-hold policies passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
