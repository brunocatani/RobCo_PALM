#pragma once
#include "WheelModel.h"
#include "Icons.h"
#include <imgui.h>
namespace wheel {
struct View { float animation{}; };
Action drawWheel(Model& model, View& view, ImVec2 position={}, ImVec2 size={},const IconTextures& icons={});
Model demoInventory();
}
