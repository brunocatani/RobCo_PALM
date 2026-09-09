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
Model selectedWheelInventory(const Model& inventory){auto& s=state();std::scoped_lock lock(s.mutex);return curatedInventory(inventory,s.prefs);}
bool takeWheelConfigChange(){return state().changed.exchange(false);}
void drawWheelConfig() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 using namespace devui;
 const float rail=visual::railWidth(ImGui::GetContentRegionAvail().x);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{20,24});
 ImGui::BeginChild("wheel-categories",{rail,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::caption("QUICK ACCESS");ImGui::Dummy({0,12});
 for(unsigned c=0;c<kCategoryCount;++c) {
  ImGui::PushID(static_cast<int>(c));
  if(visual::navigation(categoryName(static_cast<Category>(c)),s.category==c))s.category=c;
  ImGui::PopID();
 }
 ImGui::EndChild();ImGui::SameLine(0,0);
 ImGui::PopStyleVar();
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,24});
 ImGui::BeginChild("wheel-category-content",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 const float top=ImGui::GetCursorPosY();
 visual::heading(categoryName(static_cast<Category>(s.category)), "Choose up to eight items for this category.");
 const float afterHeading=ImGui::GetCursorPosY();
 ImGui::SetCursorPos({ImGui::GetWindowWidth()-230,top+6});
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{3,3});
 if(ImGui::Checkbox("Show in wheel",&s.prefs.enabled[s.category]))s.changed=true;
 ImGui::PopStyleVar();ImGui::SetCursorPos({26,afterHeading});
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
   const bool changed=ImGui::Checkbox("##selected",&selected);
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
