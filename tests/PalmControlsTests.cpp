#include "PalmControls.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <Windows.h>
#include <limits>

void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try {
 using namespace wheel;using namespace f4cf::vrcf;
 Controls controls;ControlGesture gesture;
 const auto update=[&](bool all,bool any,double time,bool click=false,bool usable=true){return gesture.update(usable,controls,all,any,click,time);};
 check(update(true,true,0)==ControlEdge::None,"a held binding opened across a context change");
 update(false,false,1);check(update(true,true,2)==ControlEdge::None && gesture.pending,"chord was not captured immediately");
 check(update(true,true,2.2)==ControlEdge::None,"hold threshold ignored");
 check(update(true,true,2.26)==ControlEdge::Open,"qualified hold did not open");
 check(update(false,true,2.3)==ControlEdge::Select && gesture.draining,"partial release did not select and drain");
 check(update(true,true,2.4)==ControlEdge::None,"remaining button reopened the wheel");
 update(false,false,3);update(true,true,4);update(false,true,4.1);
 check(gesture.draining && !gesture.open,"short chord activated instead of draining");
 update(false,false,5);controls.mode=OpenMode::Press;controls.binding.type=ActivationType::Press;
 check(update(true,true,6)==ControlEdge::Open,"press did not open immediately");
 check(update(true,true,6.1,true)==ControlEdge::None,"opening press selected an item");
 check(update(false,false,6.2)==ControlEdge::None && gesture.open,"release closed a latched wheel");
 check(update(false,false,6.3,true)==ControlEdge::Select && gesture.open,"click did not select while remaining open");
 update(false,false,6.4);check(update(false,false,6.5,true)==ControlEdge::Select,"next fresh click was lost");
 gesture.cancel(true);check(update(true,true,6.6)==ControlEdge::None,"Cancel replayed held input");
 update(false,false,7);update(true,true,8);update(true,true,8.1,false,false);
 check(!gesture.ownsInput(),"context loss retained capture");
 std::string error;Controls parsed;
 check(parseControls("press","offhand press a +primary:grip",parsed,error),"framework chord syntax rejected");
 auto masks=controlMasks(parsed,false);check(masks.buttons[0]==(1ull<<7) && masks.buttons[1]==(1ull<<2),"cross-hand binding mapped incorrectly");
 masks=controlMasks(parsed,true);check(masks.buttons[1]==(1ull<<7) && masks.buttons[0]==(1ull<<2),"left-handed mode did not swap logical hands");
 check(parseControls("hold","right press trigger",parsed,error) && parsed.mode==OpenMode::Hold,"opening gesture was coupled to selection mode");
 check(!parseControls("hold","right hold trigger nan",parsed,error),"nonfinite hold accepted");
 check(!parseControls("hold","right hold trigger +trigger",parsed,error),"duplicate chord members accepted");
 check(!parseControls("press","none",parsed,error),"inaccessible opening binding accepted");
 check(!parseControls("press","right touch trigger",parsed,error),"touch binding accepted");
 check(!parseControls("press","right thumbstick up",parsed,error),"axis binding accepted");
 for(const auto text:{"left press grip","right tap a +grip","left double b 0.4 +right:trigger","primary hold trigger 0.25 +grip","offhand longpress a 0.6","left release grip"}) {
  check(parseControls("hold",text,parsed,error),"button binding type rejected");
  Controls roundtrip;check(parseControls(parsed.mode==OpenMode::Hold?"hold":"press",bindingText(parsed),roundtrip,error) && roundtrip==parsed,"binding type did not roundtrip");
 }
 for(bool modifierHeld:{false,true}) {
  controls=Controls{};controls.binding.type=ActivationType::Tap;controls.mode=OpenMode::Press;gesture={};
  update(false,false,0);check(update(true,true,1)==ControlEdge::None,"tap opened before release");
  check(update(false,modifierHeld,1.1)==ControlEdge::Open,"tap/chord tap did not open");
  gesture={};update(false,false,2);update(true,true,3);
  check(update(false,modifierHeld,3.4)==ControlEdge::None,"long hold fired a tap");
  controls.binding.type=ActivationType::DoublePress;controls.binding.duration=.4f;gesture={};
  update(false,false,0);check(update(true,true,1)==ControlEdge::None,"first tap opened a double-tap binding");
  update(false,modifierHeld,1.1);
  check(update(true,true,1.2)==ControlEdge::Open,"double tap failed while modifier remained held");
 }
 controls.binding.type=ActivationType::DoublePress;gesture={};update(false,false,0);update(true,true,1);update(false,false,1.1);
 check(update(true,true,1.6)==ControlEdge::None,"expired double tap opened the wheel");
 controls.binding.type=ActivationType::LongPress;controls.binding.duration=0;gesture={};update(false,false,0);update(true,true,1);
 check(update(true,true,1.5)==ControlEdge::None && update(true,true,1.61)==ControlEdge::Open,"long-press default threshold ignored");
 controls.binding.type=ActivationType::Release;gesture={};update(false,false,0);update(true,true,1);
 check(update(false,false,4)==ControlEdge::Open,"release binding failed after a long hold");
 controls.binding.duration=.2f;gesture={};update(false,false,0);update(true,true,1);
 check(update(false,false,1.3)==ControlEdge::None,"release binding ignored its maximum hold");
 check(parseControls("hold","left tap b +grip",parsed,error) && parsed.mode==OpenMode::Press,"tap offered impossible release-to-select");
 check(parseControls("hold",bindingText(Controls{}),parsed,error) && parsed==Controls{},"default binding did not roundtrip");
 const auto path=std::filesystem::temp_directory_path()/("PALM-controls-"+std::to_string(GetCurrentProcessId())+".ini");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
 {std::ofstream file(path);file<<"[Other]\nuntouched=hello\n[Controls]\nsMode=press\nsOpenMenu=left press b +grip\n";}
 initializeControls(path);const auto loaded=snapshotControls();check(loaded.mode==OpenMode::Press && loaded.binding.hand==Hand::Left,"PALM did not load its own INI");
 check(applyControls(Controls{}) && takeControlsSaveRequest(),"edit did not queue persistence");persistControls();initializeControls(path);
 check(snapshotControls()==Controls{},"controls were not restored from disk");
 check(snapshotPointerAim()==PointerAim{},"missing pointer keys did not use neutral calibration");
 (void)takePointerAimChange();
 PointerAim aim;aim.pitch={-75,-68};aim.yaw={-2,4};
 check(applyPointerAim(aim,false) && !takePointerAimChange(),"dragging the calibration slider changed live aim before release");
 check(applyPointerAim(aim) && takePointerAimChange() && takeControlsSaveRequest(),"released calibration edit did not apply and queue its save");
 persistControls();initializeControls(path);
 check(snapshotPointerAim()==aim && snapshotControls()==Controls{},"per-hand calibration did not persist independently of bindings");
 auto invalidAim=aim;invalidAim.pitch[0]=91;
 check(!applyPointerAim(invalidAim) && snapshotPointerAim()==aim,"out-of-range calibration changed pointer settings");
 invalidAim=aim;invalidAim.yaw[1]=std::numeric_limits<float>::quiet_NaN();
 check(!applyPointerAim(invalidAim),"non-finite calibration was accepted");
 WritePrivateProfileStringW(L"Pointer",L"fLeftPitchDegrees",L"invalid",path.c_str());initializeControls(path);
 check(snapshotPointerAim().pitch[0]==0 && snapshotPointerAim().pitch[1]==aim.pitch[1],"invalid pointer angle replaced the other hand's calibration");
 wchar_t preserved[32]{};GetPrivateProfileStringW(L"Other",L"untouched",L"",preserved,32,path.c_str());
 check(std::wstring_view(preserved)==L"hello","PALM rewrote unrelated INI values");
 initializeControls({});std::cout<<"Control gestures, binding validation, handedness, and isolated persistence passed\n";
 return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
