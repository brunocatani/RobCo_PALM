#include "WheelModel.h"
#include <stdexcept>
#include <iostream>
void check(bool b){if(!b)throw std::runtime_error("Wheel policy check failed");}
int main(){try {
 using namespace wheel;
 check(hitSlot(0,-250,180,336)==0);check(hitSlot(250,0,180,336)==2);
 check(hitSlot(0,0,180,336)==-1);check(hitSlot(0,400,180,336)==-1);
 check(pageCount(0)==1);check(pageCount(8)==1);check(pageCount(9)==2);
 ClickLatch click;check(click.update(true,true,false,42)==0);check(click.update(false,false,true,43)==0);
 check(click.update(true,true,false,42)==0);check(click.update(false,false,true,42)==42);
 ToggleGesture gesture;check(!gesture.update(true,10));check(gesture.update(false,10.2));
 check(!gesture.update(true,11));check(!gesture.update(false,12));
 std::cout<<"Wheel selection, paging, cancellation and toggle policies passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
