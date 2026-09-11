#pragma once
#include "WheelModel.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <cstdio>
#include <string_view>

namespace wheel {
struct Favorite {std::string key,name;};
struct Preferences {
 std::array<bool,kCategoryCount> enabled{true,true,true,true,true};
 bool gesturesEnabled{true};
 std::vector<std::string> sections;
 std::array<std::vector<Favorite>,kCategoryCount> slots;
 unsigned enabledEntries() const {return 1+static_cast<unsigned>(std::count(enabled.begin(),enabled.end(),true))+(gesturesEnabled?2:0)+static_cast<unsigned>(sections.size());}
 bool sectionEnabled(std::string_view id) const {return std::find(sections.begin(),sections.end(),id)!=sections.end();}
 bool setSectionEnabled(std::string_view id,bool value,unsigned visibleCount=~0u) {
  if(visibleCount==~0u)visibleCount=enabledEntries();
  const auto it=std::find(sections.begin(),sections.end(),id);
  if(!value){if(it==sections.end())return false;sections.erase(it);return true;}
  if(it!=sections.end() || id.empty() || id.size()>=64 || sections.size()>=palm::api::kMaxSections || visibleCount>=palm::api::kMaxVisibleEntries)return false;
  sections.emplace_back(id);return true;
 }
 bool select(unsigned category,const Favorite& item,bool selected) {
  if(category>=kCategoryCount || item.key.empty() || item.key.size()>300 || item.name.empty() || item.name.size()>512 ||
   item.key.find_first_of("\r\n")!=std::string::npos || item.name.find_first_of("\r\n")!=std::string::npos)return false;
  auto& list=slots[category];
  auto found=std::find_if(list.begin(),list.end(),[&](const auto& f){return f.key==item.key;});
  if(!selected) {if(found==list.end())return false;list.erase(found);return true;}
  if(found!=list.end() || list.size()>=kSlots)return false;
  list.push_back(item);return true;
 }
};
inline std::string stableItemKey(std::string plugin,std::uint32_t form) {
 if(plugin.empty() || (form>>24)==0xff)return {};
 for(char& c:plugin)if(c>='A' && c<='Z')c=static_cast<char>(c+'a'-'A');
 char local[16];std::snprintf(local,sizeof(local),"|%06X",form&((form>>24)==0xfe?0xfffu:0xffffffu));
 return plugin+local;
}
inline Model curatedInventory(const Model& inventory,const Preferences& prefs) {
 Model result;result.category=inventory.category;result.status=inventory.status;result.enabled=prefs.enabled;result.gesturesEnabled=prefs.gesturesEnabled;
 result.sections=inventory.sections;result.activeSection=inventory.activeSection;
 for(unsigned i=0;i<result.sections.count;++i)result.sections.sections[i].enabled=prefs.sectionEnabled(result.sections.sections[i].id);
 for(unsigned c=0;c<kCategoryCount;++c) {
  if(!prefs.enabled[c])continue;
  for(const auto& favorite:prefs.slots[c]) {
   const auto& items=inventory.items[c];
   const auto found=std::find_if(items.begin(),items.end(),[&](const Item& i){return i.key==favorite.key && i.id!=0 && i.count>0;});
   // Retain the preference for reacquisition, but never draw absent inventory.
   if(found!=items.end())result.items[c].push_back(*found);
  }
 }
 normalizeWheelSelection(result);
 return result;
}
inline void writePreferences(std::ostream& out,const Preferences& prefs) {
 out<<"PALMItems 1\n";
 out<<"gestures "<<prefs.gesturesEnabled<<'\n';
 for(const auto& section:prefs.sections)out<<"section "<<std::quoted(section)<<'\n';
 for(unsigned c=0;c<kCategoryCount;++c) {
  out<<"category "<<c<<' '<<prefs.enabled[c]<<'\n';
  for(const auto& f:prefs.slots[c])out<<"item "<<c<<' '<<std::quoted(f.key)<<' '<<std::quoted(f.name)<<'\n';
 }
}
inline bool readPreferences(std::istream& in,Preferences& prefs) {
 Preferences parsed;std::string line;
 if(!std::getline(in,line))return false;
 if(!line.empty() && line.back()=='\r')line.pop_back();
 if(line!="PALMItems 1")return false;
 std::array<bool,kCategoryCount> categories{};unsigned lines=0;bool gesturesRead=false;
 while(std::getline(in,line)) {
  if(++lines>kCategoryCount*(kSlots+1)+palm::api::kMaxSections+1 || line.size()>2048)return false;
  std::istringstream row(line);std::string kind;unsigned c;
  if(!(row>>kind))return false;
  if(kind=="section") {
   std::string id;if(!(row>>std::quoted(id)) || id.empty() || id.size()>=64 || parsed.sectionEnabled(id) || parsed.sections.size()>=palm::api::kMaxSections)return false;
   if(!std::all_of(id.begin(),id.end(),[](unsigned char ch){return (ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9') || ch=='_' || ch=='-' || ch=='.';}))return false;
   parsed.sections.push_back(std::move(id));row>>std::ws;if(!row.eof())return false;continue;
  }
  if(!(row>>c) || c>=kCategoryCount)return false;
  if(kind=="gestures") {
   if(gesturesRead || c>1)return false;
   gesturesRead=true;parsed.gesturesEnabled=c!=0;
  }else if(kind=="category") {int enabled;if(categories[c] || !(row>>enabled) || enabled<0 || enabled>1)return false;
   categories[c]=true;parsed.enabled[c]=enabled!=0;
  }else if(kind=="item") {Favorite f;if(!(row>>std::quoted(f.key)>>std::quoted(f.name)) ||
    f.key.empty() || f.key.size()>300 || f.name.empty() || f.name.size()>512 ||
    !parsed.select(c,f,true))return false;
  }else return false;
  row>>std::ws;if(!row.eof())return false;
 }
 if(in.bad() || !std::all_of(categories.begin(),categories.end(),[](bool b){return b;}))return false;
 prefs=std::move(parsed);return true;
}
}
