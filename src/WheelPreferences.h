#pragma once
#include "WheelModel.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <cstdio>

namespace wheel {
struct Favorite {std::string key,name;};
struct Preferences {
 std::array<bool,kCategoryCount> enabled{true,true,true,true,true};
 std::array<std::vector<Favorite>,kCategoryCount> slots;
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
 Model result;result.category=inventory.category;result.status=inventory.status;result.enabled=prefs.enabled;
 for(unsigned c=0;c<kCategoryCount;++c) {
  if(!prefs.enabled[c])continue;
  for(const auto& favorite:prefs.slots[c]) {
   const auto& items=inventory.items[c];
   const auto found=std::find_if(items.begin(),items.end(),[&](const Item& i){return i.key==favorite.key;});
   // Keep an unavailable favorite in its slot; never substitute another item.
   result.items[c].push_back(found==items.end()?Item{0,favorite.name,0,false,favorite.key}:*found);
  }
 }
 if(!result.enabled[static_cast<unsigned>(result.category)])
  for(unsigned c=0;c<kCategoryCount;++c)if(result.enabled[c]){result.category=static_cast<Category>(c);break;}
 return result;
}
inline void writePreferences(std::ostream& out,const Preferences& prefs) {
 out<<"WheelItems 2\n";
 for(unsigned c=0;c<kCategoryCount;++c) {
  out<<"category "<<c<<' '<<prefs.enabled[c]<<'\n';
  for(const auto& f:prefs.slots[c])out<<"item "<<c<<' '<<std::quoted(f.key)<<' '<<std::quoted(f.name)<<'\n';
 }
}
inline bool readPreferences(std::istream& in,Preferences& prefs) {
 Preferences parsed;std::string line;
 if(!std::getline(in,line))return false;
 if(!line.empty() && line.back()=='\r')line.pop_back();
 const unsigned categoryCount=line=="WheelItems 1"?3:line=="WheelItems 2"?kCategoryCount:0;
 if(!categoryCount)return false;
 std::array<bool,kCategoryCount> categories{};unsigned lines=0;
 while(std::getline(in,line)) {
  if(++lines>kCategoryCount*(kSlots+1) || line.size()>2048)return false;
  std::istringstream row(line);std::string kind;unsigned c;
  if(!(row>>kind>>c) || c>=categoryCount)return false;
  if(kind=="category") {int enabled;if(categories[c] || !(row>>enabled) || enabled<0 || enabled>1)return false;
   categories[c]=true;parsed.enabled[c]=enabled!=0;
  }else if(kind=="item") {Favorite f;if(!(row>>std::quoted(f.key)>>std::quoted(f.name)) ||
    f.key.empty() || f.key.size()>300 || f.name.empty() || f.name.size()>512 ||
    !parsed.select(c,f,true))return false;
  }else return false;
  row>>std::ws;if(!row.eof())return false;
 }
 if(in.bad() || !std::all_of(categories.begin(),categories.begin()+categoryCount,[](bool b){return b;}))return false;
 prefs=std::move(parsed);return true;
}
}
