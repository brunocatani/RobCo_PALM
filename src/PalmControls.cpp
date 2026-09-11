#include "PalmControls.h"
#include "tools/render/ConfigUi.h"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <format>

namespace wheel {
namespace {
struct State {std::mutex mutex;Controls controls;std::filesystem::path path;std::string status;bool dirty{};std::atomic_bool save{},gestures{true};};
State& state(){static State value;return value;}
constexpr std::array activationTypes{f4cf::vrcf::ActivationType::Press,f4cf::vrcf::ActivationType::Tap,f4cf::vrcf::ActivationType::DoublePress,f4cf::vrcf::ActivationType::HoldDown,f4cf::vrcf::ActivationType::LongPress,f4cf::vrcf::ActivationType::Release};
constexpr std::array activationNames{"press","tap","double","hold","longpress","release"};
constexpr std::array hands{"primary","offhand","right","left"};
constexpr std::array buttons{vr::k_EButton_SteamVR_Trigger,vr::k_EButton_Grip,vr::k_EButton_A,vr::k_EButton_ApplicationMenu,vr::k_EButton_SteamVR_Touchpad};
constexpr std::array buttonNames{"trigger","grip","a","b","thumbstick"};
const char* buttonName(vr::EVRButtonId button){for(unsigned i=0;i<buttons.size();++i)if(buttons[i]==button)return buttonNames[i];return "invalid";}
bool supported(vr::EVRButtonId button){return std::find(buttons.begin(),buttons.end(),button)!=buttons.end();}
}
std::string bindingText(const Controls& controls) {
 const auto& b=controls.binding;
 auto text=std::format("{} {} {}",hands[static_cast<unsigned>(b.hand)],activationNames[static_cast<unsigned>(std::find(activationTypes.begin(),activationTypes.end(),b.type)-activationTypes.begin())],buttonName(b.button));
 if(b.duration>0)text+=std::format(" {:.2f}",b.duration);
 text+=" suppress";
 if(b.modifier){text+=" +";if(b.modifier->hand)text+=std::string(hands[static_cast<unsigned>(*b.modifier->hand)])+":";text+=buttonName(b.modifier->button);}
 return text;
}
bool parseControls(std::string_view mode,std::string_view text,Controls& out,std::string& error) {
 using namespace f4cf::vrcf;
 if(mode!="hold" && mode!="press"){error="Mode must be hold or press";return false;}
 const auto parsed=parseInputBinding(text);
 if(!parsed || !parsed->isEnabled() || !supported(parsed->button) || (parsed->modifier && !supported(parsed->modifier->button))){error="Choose a button, optionally with one modifier";return false;}
 if(std::find(activationTypes.begin(),activationTypes.end(),parsed->type)==activationTypes.end()) {
  error="Choose press, tap, double tap, hold, long press, or release. Touch and axes are not supported.";return false;
 }
 if(!std::isfinite(parsed->duration) || parsed->duration<0 || parsed->duration>5){error="Timing must be between 0 and 5 seconds";return false;}
 if(parsed->modifier && parsed->modifier->button==parsed->button && parsed->modifier->hand.value_or(parsed->hand)==parsed->hand){error="Choose two different buttons for a chord";return false;}
 out.mode=mode=="hold" && supportsReleaseSelection(parsed->type)?OpenMode::Hold:OpenMode::Press;out.binding=*parsed;
 out.binding.suppress=true;
 if(parsed->type==ActivationType::Press || parsed->type==ActivationType::Tap)out.binding.duration=0;
 error.clear();return true;
}
Controls snapshotControls(){auto& s=state();std::scoped_lock lock(s.mutex);return s.controls;}
void initializeControls(const std::filesystem::path& path) {
 auto& s=state();std::scoped_lock lock(s.mutex);s.path=path;
 if(path.empty())return;
 if(!std::filesystem::exists(path)){s.save=true;return;}
 std::array<wchar_t,256> mode{},binding{};
 GetPrivateProfileStringW(L"Controls",L"sMode",L"hold",mode.data(),static_cast<DWORD>(mode.size()),path.c_str());
 GetPrivateProfileStringW(L"Controls",L"sOpenMenu",L"right hold trigger 0.25 suppress +grip",binding.data(),static_cast<DWORD>(binding.size()),path.c_str());
 const std::wstring_view m(mode.data()),b(binding.data());
 const auto ascii=[](std::wstring_view wide){std::string text;for(wchar_t c:wide){if(c>127)return std::string{"invalid"};text.push_back(static_cast<char>(c));}return text;};
 Controls value;
 if(parseControls(ascii(m),ascii(b),value,s.status)){s.controls=value;s.status="Controls loaded";}
}
bool applyControls(const Controls& controls,bool queueSave) {
 Controls validated;std::string error;
 if(!parseControls(controls.mode==OpenMode::Hold?"hold":"press",bindingText(controls),validated,error)){auto& s=state();std::scoped_lock lock(s.mutex);s.status=error;return false;}
 auto& s=state();std::scoped_lock lock(s.mutex);s.controls=validated;s.dirty=true;if(queueSave)s.save=true;s.status=s.path.empty()?"Preview controls updated":"Saving controls...";return true;
}
bool takeControlsSaveRequest(){return state().save.exchange(false);}
void persistControls() {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(s.path.empty()){s.dirty=false;return;}
 try {
  std::filesystem::create_directories(s.path.parent_path());
  const auto text=bindingText(s.controls);const std::wstring binding(text.begin(),text.end());
  const bool written=WritePrivateProfileStringW(L"Controls",L"sMode",s.controls.mode==OpenMode::Hold?L"hold":L"press",s.path.c_str()) &&
   WritePrivateProfileStringW(L"Controls",L"sOpenMenu",binding.c_str(),s.path.c_str());
  s.dirty=false;
  s.status=written?"Controls saved to PALM.ini":"Could not save PALM.ini; controls apply to this session";
 }catch(...){s.dirty=false;s.status="Could not save PALM.ini; controls apply to this session";}
}
void setGestureIntegrationAvailable(bool available){state().gestures=available;}
bool gestureIntegrationAvailable(){return state().gestures.load();}
void drawControls() {
 using namespace devui;using namespace f4cf::vrcf;
 auto value=snapshotControls();bool changed=false;
 visual::heading("Controls","Choose how the wheel opens and makes selections.");
 visual::caption("Selection");
 ImGui::BeginDisabled(!supportsReleaseSelection(value.binding.type));
 if(visual::button("Release to select",{245,48},value.mode==OpenMode::Hold)){value.mode=OpenMode::Hold;changed=true;}
 ImGui::EndDisabled();ImGui::SameLine();
 if(visual::button("Click to select",{245,48},value.mode==OpenMode::Press)){value.mode=OpenMode::Press;changed=true;}
 {visual::Font font(render::FontRole::Body,18);ImGui::PushTextWrapPos();ImGui::TextColored(visual::muted(),"%s",value.mode==OpenMode::Hold?
  "Keep the opening input held. Release it to select.":"Open with your binding, release its buttons, then click to select. Click Cancel to close.");ImGui::PopTextWrapPos();}
 ImGui::Dummy({0,12});
 const auto combo=[&](const char* label,int& index,const auto& labels) {
  bool edited=false;ImGui::TableNextColumn();visual::caption(label);ImGui::PushID(label);ImGui::SetNextItemWidth(-1);
  if(ImGui::BeginCombo("##value",labels[index])){for(int i=0;i<static_cast<int>(labels.size());++i)if(ImGui::Selectable(labels[i],index==i)){index=i;edited=true;}ImGui::EndCombo();}
  ImGui::PopID();ImGui::Dummy({0,12});return edited;
 };
 if(ImGui::BeginTable("opening-binding",2,ImGuiTableFlags_SizingStretchSame)) {
  constexpr std::array handLabels{"Primary","Off hand","Right","Left"};
  constexpr std::array buttonLabels{"Trigger","Grip","A / X","B / Y","Thumbstick"};
  int hand=static_cast<int>(value.binding.hand);
  if(combo("Hand",hand,handLabels)){value.binding.hand=static_cast<Hand>(hand);changed=true;}
  constexpr std::array activationLabels{"Press","Tap","Double tap","Hold","Long press","Release"};
  int activation=static_cast<int>(std::find(activationTypes.begin(),activationTypes.end(),value.binding.type)-activationTypes.begin());
  if(combo("Opening gesture",activation,activationLabels)) {
   value.binding.type=activationTypes[activation];
   value.binding.duration=value.binding.type==ActivationType::HoldDown?.25f:value.binding.type==ActivationType::LongPress?.6f:value.binding.type==ActivationType::DoublePress?.4f:0;
   if(!supportsReleaseSelection(value.binding.type))value.mode=OpenMode::Press;
   changed=true;
  }
  int button=static_cast<int>(std::find(buttons.begin(),buttons.end(),value.binding.button)-buttons.begin());
  if(combo("Button",button,buttonLabels)){value.binding.button=buttons[button];changed=true;}
  constexpr std::array modifierNames{"None","Trigger","Grip","A / X","B / Y","Thumbstick"};
  int modifier=value.binding.modifier?1+static_cast<int>(std::find(buttons.begin(),buttons.end(),value.binding.modifier->button)-buttons.begin()):0;
  if(combo("Modifier",modifier,modifierNames)) {if(modifier)value.binding.modifier=InputModifier{buttons[modifier-1],{}};else value.binding.modifier.reset();changed=true;}
  if(value.binding.modifier) {
   int modifierHand=value.binding.modifier->hand?1+static_cast<int>(*value.binding.modifier->hand):0;
   constexpr std::array modifierHands{"Same hand","Primary","Off hand","Right","Left"};
   if(combo("Modifier hand",modifierHand,modifierHands)){value.binding.modifier->hand=modifierHand?std::optional<Hand>(static_cast<Hand>(modifierHand-1)):std::nullopt;changed=true;}
  }
  ImGui::EndTable();
 }
 const auto type=value.binding.type;
 if(type==ActivationType::HoldDown || type==ActivationType::LongPress || type==ActivationType::DoublePress || type==ActivationType::Release) {
  visual::caption(type==ActivationType::DoublePress?"Double-tap window":type==ActivationType::Release?"Maximum hold (0 = any)":"Hold time");
  float timing=value.binding.duration>0?value.binding.duration:type==ActivationType::LongPress?.6f:type==ActivationType::DoublePress?.4f:0;
  ImGui::SetNextItemWidth(320);if(ImGui::SliderFloat("##binding-time",&timing,0,1,"%.2f seconds")){value.binding.duration=timing;changed=true;}
 }
 if(!supportsReleaseSelection(type)){visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"Tap and release bindings use click-to-select.");}
 if(changed)(void)applyControls(value,!ImGui::IsAnyItemActive());
 {auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(lock.owns_lock()){if(s.dirty && !ImGui::IsAnyItemActive())s.save=true;visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"%s",s.status.c_str());}}
 ImGui::Dummy({0,24});
}
}
