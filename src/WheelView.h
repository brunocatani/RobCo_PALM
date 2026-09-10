#pragma once
#include "WheelModel.h"
#include <imgui.h>
namespace wheel {
struct View { float animation{}; };
Action drawWheel(Model& model, View& view, ImVec2 position={}, ImVec2 size={});
Model demoInventory();
}
