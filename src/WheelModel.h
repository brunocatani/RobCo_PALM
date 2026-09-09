#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace wheel {
enum class Category : unsigned { Aid, Food, Grenades };
inline constexpr std::size_t kSlots = 8;
inline constexpr std::size_t kMaxItems = 512;
inline constexpr float kPi = 3.14159265359f;
struct Item { std::uint32_t id{}; std::string name; std::uint32_t count{}; bool equipped{}; };
struct Model {
 std::array<std::vector<Item>, 3> items;
 Category category{Category::Aid};
 std::size_t page{};
 std::string status{"Select something to take"};
};
inline const char* categoryName(Category c) {
 switch(c) {case Category::Aid: return "AID"; case Category::Food: return "FOOD"; case Category::Grenades: return "GRENADES";}
 return "";
}
inline std::size_t pageCount(std::size_t size) { return std::max<std::size_t>(1, (size + kSlots - 1) / kSlots); }
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
struct ClickLatch {
 std::uint32_t pressed{};
 std::uint32_t update(bool down, bool pressEdge, bool releaseEdge, std::uint32_t hovered) {
  if(pressEdge) pressed=hovered;
  const auto chosen = releaseEdge && hovered != 0 && pressed == hovered ? hovered : 0;
  if(releaseEdge || (!down && !pressEdge)) pressed=0;
  return chosen;
 }
};
struct ToggleGesture {
 bool down{};
 double pressedAt{};
 bool update(bool held, double now) {
  if(held && !down) {down=true; pressedAt=now;}
  else if(!held && down) {down=false; return now-pressedAt <= 0.35 && now>=pressedAt;}
  return false;
 }
};
}
