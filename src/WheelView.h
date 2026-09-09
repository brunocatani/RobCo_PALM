#pragma once
#include "WheelModel.h"
#include <imgui.h>
namespace wheel {
struct View { ClickLatch click,centerClick; float animation{}; };
struct Action { std::uint32_t useItem{}; bool close{}; bool config{}; };
Action drawWheel(Model& model, View& view, ImVec2 position={}, ImVec2 size={});
Model demoInventory();
}
