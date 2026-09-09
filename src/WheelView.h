#pragma once
#include "WheelModel.h"
#include <imgui.h>
namespace wheel {
struct View { float animation{}; };
struct Action { std::uint32_t hoveredItem{}; bool configHovered{}; };
Action drawWheel(Model& model, View& view, ImVec2 position={}, ImVec2 size={});
Model demoInventory();
}
