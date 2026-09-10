#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace wheel {
enum class Category : unsigned { Aid, Food, Grenades, Weapons, Armor };
inline constexpr unsigned kCategoryCount = 5;
inline constexpr unsigned kConfigNavigation = kCategoryCount;
inline constexpr unsigned kCancelNavigation = kCategoryCount + 1;
inline constexpr bool isEquipment(Category category) { return category == Category::Weapons || category == Category::Armor; }
inline constexpr std::size_t kSlots = 8;
inline constexpr std::size_t kMaxItems = 512;
inline constexpr float kPi = 3.14159265359f;
struct Item { std::uint32_t id{}; std::string name; std::uint32_t count{}; bool equipped{}; std::string key; std::uint32_t stackIndex{}; };
inline std::uint64_t selectionToken(const Item& item) { return (static_cast<std::uint64_t>(item.stackIndex) << 32) | item.id; }
struct Model {
 std::array<std::vector<Item>, kCategoryCount> items;
 Category category{Category::Aid};
 std::array<bool,kCategoryCount> enabled{true,true,true,true,true};
 std::string status{"Select something to take"};
};
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
// Six inner sectors; the hold/hover/release gesture remains independent of category.
inline int hitCenter(float x,float y,float scale) {
 const float radius=std::hypot(x,y)/scale;
 if(radius<55) return kCancelNavigation;
 if(radius<70 || radius>173) return -1;
 constexpr float step=2*kPi/(kCategoryCount+1);
 float angle=std::atan2(y,x)+kPi/2+step/2;
 if(angle<0)angle+=2*kPi;
 if(angle>=2*kPi)angle-=2*kPi;
 const float quadrant=angle/step,fraction=quadrant-std::floor(quadrant);
 if(fraction<.025f || fraction>.975f)return -1;
 return static_cast<int>(quadrant);
}
enum class HoldEdge { None, Open, Release };
struct HoldGesture {
 bool armed{}, down{};
 HoldEdge update(bool eligible,bool held,bool nativeActivationTarget=false) {
  if(!eligible){armed=false;down=false;return HoldEdge::None;}
  // Classify at press time. A native-owned hold cannot become a wheel hold
  // by moving the ray off the target; an existing wheel hold keeps its owner.
  if(held && !down && nativeActivationTarget){armed=false;return HoldEdge::None;}
  if(!held){const bool released=down;down=false;armed=true;return released?HoldEdge::Release:HoldEdge::None;}
  if(armed && !down){down=true;return HoldEdge::Open;}
  return HoldEdge::None;
 }
};
}
