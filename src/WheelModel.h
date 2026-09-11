#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "GestureCatalog.h"
#include "IconCatalog.h"
#include "SectionRegistry.h"

namespace wheel {
enum class Category : unsigned { Aid, Food, Grenades, Weapons, Armor };
inline constexpr unsigned kCategoryCount = 5;
inline constexpr unsigned kRightGesturesNavigation = 2;
inline constexpr unsigned kLeftGesturesNavigation = 6;
inline constexpr unsigned kConfigNavigation = 7;
inline constexpr unsigned kNavigationCount = 8;
inline constexpr unsigned kCancelNavigation = kNavigationCount;
inline constexpr unsigned kExternalNavigation = kCancelNavigation+1;
// Semantic navigation IDs remain independent of their visible ring positions.
inline constexpr int navigationCategory(int navigation) {
 switch(navigation) {case 0:return 0;case 1:return 1;case 3:return 2;case 4:return 3;case 5:return 4;default:return -1;}
}
inline constexpr bool isEquipment(Category category) { return category == Category::Weapons || category == Category::Armor; }
inline constexpr std::size_t kSlots = 8;
inline constexpr std::size_t kMaxItems = 512;
inline constexpr float kPi = 3.14159265359f;
struct Item { std::uint32_t id{}; std::string name; std::uint32_t count{}; bool equipped{}; std::string key; std::uint32_t stackIndex{}; Icon icon{Icon::Automatic}; };
inline std::uint64_t selectionToken(const Item& item) { return (static_cast<std::uint64_t>(item.stackIndex) << 32) | item.id; }
struct Model {
 std::array<std::vector<Item>, kCategoryCount> items;
 Category category{Category::Aid};
 std::array<bool,kCategoryCount> enabled{true,true,true,true,true};
 bool gesturesEnabled{true};
 std::string status;
 GestureViewState gestures;
 SectionCatalog sections;
 palm::api::SectionHandle activeSection{};
};
struct Action { std::uint64_t hoveredItem{}; bool configHovered{}; unsigned hoveredGesture{}; palm::api::SectionHandle section{}; std::uint32_t sectionItem{}; bool cancelHovered{}; };
struct NavigationLayout {
 std::array<unsigned,palm::api::kMaxVisibleEntries> entries{};
 unsigned count{};
 float step{},start{-kPi/2};
 float angle(unsigned slot) const {return start+slot*step;}
};
inline constexpr float kNavigationGap=.035f;
inline NavigationLayout navigationLayout(const Model& model) {
 NavigationLayout layout;
 unsigned right{},left{};
 for(unsigned id=0;id<kConfigNavigation;++id) {
  const int category=navigationCategory(static_cast<int>(id));
  if(category>=0 && !model.enabled[category])continue;
  if(id==kRightGesturesNavigation || id==kLeftGesturesNavigation) {
   if(!model.gesturesEnabled)continue;
   (id==kRightGesturesNavigation?right:left)=layout.count;
  }
  layout.entries[layout.count++]=id;
 }
 for(unsigned i=0;i<model.sections.count && layout.count+1<layout.entries.size();++i)
  if(model.sections.sections[i].enabled)layout.entries[layout.count++]=kExternalNavigation+i;
 layout.entries[layout.count++]=kConfigNavigation;
 // Config is always present. Keep the physical hand entries on their own sides
 // even when an odd number of visible sectors prevents exact opposition.
 layout.step=2*kPi/layout.count;
 if(model.gesturesEnabled)layout.start=kPi/2-(right+left)*layout.step/2;
 return layout;
}
inline void normalizeWheelSelection(Model& model) {
 if(model.activeSection) {
  for(unsigned i=0;i<model.sections.count;++i)
   if(model.sections.sections[i].enabled && model.sections.sections[i].handle==model.activeSection)return;
  model.activeSection=0;
 }
 if(!model.gesturesEnabled)model.gestures.showing=false;
 if(model.gestures.showing || model.enabled[static_cast<unsigned>(model.category)])return;
 for(unsigned c=0;c<kCategoryCount;++c)if(model.enabled[c]) {
  model.category=static_cast<Category>(c);return;
 }
 model.gestures.showing=model.gesturesEnabled;
 if(!model.gestures.showing)for(unsigned i=0;i<model.sections.count;++i)
  if(model.sections.sections[i].enabled){model.activeSection=model.sections.sections[i].handle;return;}
}
inline const char* categoryName(Category c) {
 switch(c) {case Category::Aid: return "AID"; case Category::Food: return "FOOD"; case Category::Grenades: return "GRENADES"; case Category::Weapons: return "WEAPONS"; case Category::Armor: return "ARMOR";}
 return "";
}
// Slot zero is centered at twelve o'clock. Radius and gap tests match drawing.
inline int hitSlot(float x, float y, float inner, float outer) {
 const float radius = std::sqrt(x*x+y*y);
 if (!(radius >= inner && radius <= outer)) return -1;
 float angle = std::atan2(y,x) + kPi/2 + kPi/8;
 if(angle < 0) angle += 2*kPi;
 if(angle >= 2*kPi) angle -= 2*kPi;
 const float segment = angle / (kPi/4);
 const float fraction = segment - std::floor(segment);
 if(fraction < 0.025f || fraction > 0.975f) return -1;
 return static_cast<int>(segment);
}
// Inventory, gestures and Config share the same hold/hover/release navigation.
inline int hitCenter(float x,float y,float scale,const NavigationLayout& layout) {
 const float radius=std::hypot(x,y)/scale;
 if(radius<=54) return kCancelNavigation;
 if(radius<70 || radius>172) return -1;
 float angle=std::fmod(std::atan2(y,x)-layout.start+layout.step/2,2*kPi);
 if(angle<0)angle+=2*kPi;
 const unsigned slot=static_cast<unsigned>(angle/layout.step);
 if(slot>=layout.count)return -1;
 const float within=angle-slot*layout.step;
 if(within<kNavigationGap || within>layout.step-kNavigationGap)return -1;
 return static_cast<int>(layout.entries[slot]);
}
}
