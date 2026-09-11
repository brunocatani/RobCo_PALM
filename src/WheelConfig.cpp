#include "WheelConfig.h"
#include <imgui.h>
#include "tools/render/ConfigUi.h"
#include <mutex>
#include <atomic>

namespace wheel {
namespace {
struct State {
 std::mutex mutex;Preferences prefs;Model inventory;
 std::atomic_bool changed=false;
 std::string status="Preview — choices stay in memory";
 unsigned category{};char search[128]{};
};
State& state(){static State s;return s;}
bool matches(std::string_view name,std::string_view needle) {
 const auto lower=[](char c){return c>='A' && c<='Z'?static_cast<char>(c+32):c;};
 return std::search(name.begin(),name.end(),needle.begin(),needle.end(),
  [&](char a,char b){return lower(a)==lower(b);})!=name.end() || needle.empty();
}
}
Preferences snapshotWheelPreferences() {
 auto& s=state();std::scoped_lock lock(s.mutex);return s.prefs;
}
void restoreWheelPreferences(Preferences prefs) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 s.prefs=std::move(prefs);s.inventory={};s.changed=false;s.category=0;s.search[0]='\0';
 s.status="Saved with your game";
}
void publishWheelInventory(const Model& inventory){auto& s=state();std::scoped_lock lock(s.mutex);s.inventory=inventory;}
Model selectedWheelInventory(const Model& inventory){
 auto& s=state();std::scoped_lock lock(s.mutex);auto result=curatedInventory(inventory,s.prefs);
 if(sectionRegistry().snapshot(result.sections))
  for(unsigned i=0;i<result.sections.count;++i)result.sections.sections[i].enabled=s.prefs.sectionEnabled(result.sections.sections[i].id);
 return result;
}
void refreshWheelSections(Model& model) {
 if(model.sections.revision==sectionRegistry().revision())return;
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 if(!sectionRegistry().snapshot(model.sections))return;
 for(unsigned i=0;i<model.sections.count;++i)model.sections.sections[i].enabled=s.prefs.sectionEnabled(model.sections.sections[i].id);
}
bool takeWheelConfigChange(){return state().changed.exchange(false);}
void drawWheelSettings() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 using namespace devui;
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,24});
 ImGui::BeginChild("wheel-settings",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::heading("Wheel sections","Choose which sections appear. Visible sections fill the ring.");
 {visual::Font font(render::FontRole::Body,18);
  ImGui::TextColored(visual::muted(),"%u / %u slots enabled. Config and Cancel always stay available.",s.prefs.enabledEntries(),palm::api::kMaxVisibleEntries);
  ImGui::TextColored(visual::muted(),"Hiding a section keeps its item choices. Gestures uses two slots.");}
 ImGui::Dummy({0,16});
 const auto toggle=[&](const char* name,const char* detail,bool& enabled,unsigned cost) {
  ImGui::TableNextColumn();ImGui::PushID(name);
  ImGui::BeginDisabled(!enabled && s.prefs.enabledEntries()+cost>palm::api::kMaxVisibleEntries);
  bool changed;
  {visual::Font font(render::FontRole::Medium,24);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4,4});
   changed=devui::visual::checkbox(name,&enabled);ImGui::PopStyleVar();}
  ImGui::EndDisabled();
  {visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"%s",detail);}
  ImGui::Dummy({0,14});ImGui::PopID();return changed;
 };
 if(ImGui::BeginTable("wheel-section-toggles",2,ImGuiTableFlags_SizingStretchSame)) {
  for(const auto category:{Category::Weapons,Category::Armor,Category::Aid,Category::Food,Category::Grenades}) {
   const auto c=static_cast<unsigned>(category);
   if(toggle(categoryName(category),"Inventory items",s.prefs.enabled[c],1))s.changed=true;
  }
  if(toggle("Gestures","Left and right hand menus",s.prefs.gesturesEnabled,2))s.changed=true;
  ImGui::EndTable();
 }
 SectionCatalog catalog;
 if(sectionRegistry().snapshot(catalog)) {
  visual::caption("MOD SECTIONS");ImGui::Dummy({0,12});
  if(catalog.count==0 && s.prefs.sections.empty()) {
   visual::Font font(render::FontRole::Body,20);ImGui::TextColored(visual::muted(),"No mod sections registered.");
  }else if(ImGui::BeginTable("mod-section-toggles",2,ImGuiTableFlags_SizingStretchSame)) {
   for(unsigned i=0;i<catalog.count;++i) {
    const auto& section=catalog.sections[i];bool enabled=s.prefs.sectionEnabled(section.id);
    ImGui::PushID(section.id);
    if(toggle(section.name,section.modName,enabled,1) && s.prefs.setSectionEnabled(section.id,enabled))s.changed=true;
    ImGui::PopID();
   }
   // Remembered unavailable sections remain removable instead of reserving slots forever.
   for(std::size_t i=0;i<s.prefs.sections.size();) {
    const auto& id=s.prefs.sections[i];
    const bool present=std::any_of(catalog.sections.begin(),catalog.sections.begin()+catalog.count,[&](const auto& value){return id==value.id;});
    if(present){++i;continue;}
    bool enabled=true;
    if(toggle(id.c_str(),"Mod not currently registered",enabled,1)) {
     s.prefs.sections.erase(s.prefs.sections.begin()+i);s.changed=true;
    }else ++i;
   }
   ImGui::EndTable();
  }
 }
 ImGui::EndChild();ImGui::PopStyleVar();
}
void drawWheelConfig() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 using namespace devui;
 const float rail=visual::railWidth(ImGui::GetContentRegionAvail().x);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{20,24});
 ImGui::BeginChild("wheel-categories",{rail,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::caption("QUICK ACCESS");ImGui::Dummy({0,12});
 for(unsigned c=0;c<kCategoryCount;++c) {
  ImGui::PushID(static_cast<int>(c));
  if(visual::navigation(categoryName(static_cast<Category>(c)),s.category==c,s.prefs.enabled[c]?nullptr:"OFF"))s.category=c;
  ImGui::PopID();
 }
 ImGui::EndChild();ImGui::SameLine(0,0);
 ImGui::PopStyleVar();
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,24});
 ImGui::BeginChild("wheel-category-content",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::heading(categoryName(static_cast<Category>(s.category)), "Choose up to eight items for this category.");
 if(!s.prefs.enabled[s.category]){visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"Hidden from the wheel. Enable it in PALM settings.");ImGui::Dummy({0,8});}
 auto& favorites=s.prefs.slots[s.category];const auto& catalog=s.inventory.items[s.category];
 const float searchWidth=(std::max)(220.0f,ImGui::GetContentRegionAvail().x-280);
 ImGui::SetNextItemWidth(searchWidth);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{16,12});
 ImGui::InputTextWithHint("##item-search","Search inventory...",s.search,sizeof(s.search));
 ImGui::PopStyleVar();ImGui::SameLine(0,24);
 ImGui::BeginGroup();
 ImGui::Text("%zu / 8 selected",favorites.size());
 { visual::Font font(render::FontRole::Body,14);ImGui::TextColored(visual::muted(),"%s",s.status.c_str()); }
 ImGui::EndGroup();ImGui::Dummy({0,16});
 constexpr auto flags=ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersOuter|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp;
 if(ImGui::BeginTable("wheel-item-selection",2,flags,{0,0})) {
  ImGui::TableSetupColumn("ITEM",ImGuiTableColumnFlags_WidthStretch);
  const bool equipment=isEquipment(static_cast<Category>(s.category));
  const char* statusColumn=equipment?"EQUIPMENT":"CARRIED";
  ImGui::TableSetupColumn(statusColumn,ImGuiTableColumnFlags_WidthFixed,160);
  visual::tableHeader("ITEM",statusColumn);
  const auto row=[&](std::string_view key,std::string_view name,unsigned carried,bool available,bool selected,bool equipped=false) {
   ImGui::PushID(key.data(),key.data()+key.size());
   ImGui::TableNextRow(0,44);ImGui::TableSetColumnIndex(0);
   if(selected)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,visual::color(visual::accent(0.13f)));
   ImGui::BeginDisabled(!selected && favorites.size()>=kSlots);
   ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{3,3});
   const bool changed=devui::visual::checkbox("##selected",&selected);
   ImGui::PopStyleVar();ImGui::SameLine(0,18);
   ImGui::TextUnformatted(name.data(),name.data()+name.size());ImGui::EndDisabled();
   ImGui::TableSetColumnIndex(1);
   if(available && equipment){visual::Font font(render::FontRole::Medium,20);ImGui::TextColored(equipped?visual::accent():visual::muted(),"%s",equipped?"Equipped":"In inventory");}
   else if(available)ImGui::TextColored(visual::muted(),"%u",carried);
   else {visual::Font font(render::FontRole::Body,16);ImGui::TextDisabled("Not carried");}
   if(changed && s.prefs.select(s.category,{std::string(key),std::string(name)},selected))s.changed=true;
   ImGui::PopID();return changed;
  };
  // Unavailable favorites remain removable and retain their slot identity.
  for(std::size_t i=0;i<favorites.size();) {
   const auto& f=favorites[i];
   const bool carried=std::any_of(catalog.begin(),catalog.end(),[&](const Item& item){return item.key==f.key;});
   if(carried || !matches(f.name,s.search)){++i;continue;}
   if(!row(f.key,f.name,0,false,true))++i;
  }
  std::array<std::size_t,kMaxItems> filtered{};int count=0;
  for(std::size_t i=0;i<catalog.size() && count<static_cast<int>(filtered.size());++i)
   if(!catalog[i].key.empty() && matches(catalog[i].name,s.search))filtered[count++]=i;
  ImGuiListClipper clipper;clipper.Begin(count);
  while(clipper.Step())for(int n=clipper.DisplayStart;n<clipper.DisplayEnd;++n) {
   const auto& item=catalog[filtered[n]];
   const bool selected=std::any_of(favorites.begin(),favorites.end(),[&](const Favorite& f){return f.key==item.key;});
   row(item.key,item.name,item.count,true,selected,item.equipped);
  }
  if(!count && favorites.empty()){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextDisabled("No matching inventory items.");}
  ImGui::EndTable();
 }
 ImGui::EndChild();ImGui::PopStyleVar();
}
}
