#include "WheelConfig.h"
#include <imgui.h>
#include <Windows.h>
#include <fstream>
#include <mutex>
#include <atomic>

namespace wheel {
namespace {
struct State {
 std::mutex mutex;Preferences prefs;Model inventory;
 std::filesystem::path path;bool writable=true;std::atomic_bool changed=false;
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
void loadWheelConfig(const std::filesystem::path& path) {
 auto& s=state();std::scoped_lock lock(s.mutex);s.path=path;
 std::error_code error;const bool exists=std::filesystem::exists(path,error);
 if(!error && !exists){s.status="Choose up to eight items per category";return;}
 std::ifstream file(path);Preferences prefs;
 if(error || std::filesystem::file_size(path,error)>65536 || error || !file || !readPreferences(file,prefs)) {
  s.writable=false;s.status="Item configuration could not be read; existing file will not be overwritten";return;
 }
 s.prefs=std::move(prefs);s.status="Item choices loaded";
}
void flushWheelConfig() {
 auto& s=state();Preferences prefs;std::filesystem::path path;
 {std::scoped_lock lock(s.mutex);if(s.path.empty() || !s.writable)return;prefs=s.prefs;path=s.path;}
 std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
 auto temporary=path;temporary+=L".tmp";
 bool saved=false;
 if(!error) {
  std::ofstream file(temporary,std::ios::trunc);writePreferences(file,prefs);file.flush();
  const bool written=static_cast<bool>(file);file.close();
  if(written)saved=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 }
 {std::scoped_lock lock(s.mutex);s.status=saved?"Item choices saved":"Could not save item choices — check the configuration folder";}
}
void publishWheelInventory(const Model& inventory){auto& s=state();std::scoped_lock lock(s.mutex);s.inventory=inventory;}
Model selectedWheelInventory(const Model& inventory){auto& s=state();std::scoped_lock lock(s.mutex);return curatedInventory(inventory,s.prefs);}
bool takeWheelConfigChange(){return state().changed.exchange(false);}
void drawWheelConfig() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 ImGui::TextUnformatted("WHEEL CONTENTS");
 ImGui::TextDisabled("Choose the categories and items available in quick access. Eight slots per category.");
 ImGui::Spacing();
 for(unsigned c=0;c<3;++c) {
  ImGui::PushID(static_cast<int>(c));
  if(c)ImGui::SameLine(0,24);
  if(ImGui::Checkbox(categoryName(static_cast<Category>(c)),&s.prefs.enabled[c]))s.changed=true;
  ImGui::PopID();
 }
 ImGui::Separator();
 for(unsigned c=0;c<3;++c) {
  if(c)ImGui::SameLine(0,24);
  if(ImGui::RadioButton(categoryName(static_cast<Category>(c)),s.category==c))s.category=c;
 }
 auto& favorites=s.prefs.slots[s.category];const auto& catalog=s.inventory.items[s.category];
 ImGui::Text("%zu / 8 selected",favorites.size());
 ImGui::SameLine();ImGui::TextDisabled("%s",s.status.c_str());
 ImGui::SetNextItemWidth(-1);
 ImGui::InputTextWithHint("##item-search","Find an item in your inventory...",s.search,sizeof(s.search));
 ImGui::BeginChild("wheel-item-selection",{0,0});
 // Unavailable favorites remain removable and keep their original slot identity.
 for(std::size_t i=0;i<favorites.size();) {
  const auto f=favorites[i];
  const bool carried=std::any_of(catalog.begin(),catalog.end(),[&](const Item& item){return item.key==f.key;});
  if(carried || !matches(f.name,s.search)){++i;continue;}
  ImGui::PushID(f.key.c_str());bool selected=true;
  if(ImGui::Checkbox(f.name.c_str(),&selected)){s.prefs.select(s.category,f,false);s.changed=true;}
  else ++i;
  ImGui::SameLine();ImGui::TextDisabled("not currently carried");ImGui::PopID();
 }
 std::array<std::size_t,kMaxItems> filtered{};int count=0;
 for(std::size_t i=0;i<catalog.size() && count<static_cast<int>(filtered.size());++i)
  if(!catalog[i].key.empty() && matches(catalog[i].name,s.search))filtered[count++]=i;
 if(!count && favorites.empty())ImGui::TextDisabled("No matching items carried. The spawner can add items to your inventory.");
 ImGuiListClipper clipper;clipper.Begin(count);
 while(clipper.Step())for(int n=clipper.DisplayStart;n<clipper.DisplayEnd;++n) {
  const auto& item=catalog[filtered[n]];
  bool selected=std::any_of(favorites.begin(),favorites.end(),[&](const Favorite& f){return f.key==item.key;});
  ImGui::PushID(item.key.c_str());ImGui::BeginDisabled(!selected && favorites.size()>=kSlots);
  if(ImGui::Checkbox(item.name.c_str(),&selected)) {
   if(s.prefs.select(s.category,{item.key,item.name},selected))s.changed=true;
  }
  ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("%u carried",item.count);ImGui::PopID();
 }
 ImGui::EndChild();
}
}
