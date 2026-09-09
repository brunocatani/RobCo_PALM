#pragma once
#include "WheelModel.h"
namespace wheel {
struct View { ClickLatch click; float animation{}; float scroll{}; };
struct Action { std::uint32_t useItem{}; bool close{}; };
Action drawWheel(Model& model, View& view);
Model demoInventory();
}
