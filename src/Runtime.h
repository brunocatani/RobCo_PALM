#pragma once
#include "WheelView.h"
#include <mutex>

namespace wheel {
struct SharedModel { std::mutex mutex; Model model; View view; std::string lastAction; };
SharedModel& sharedModel();
bool startRuntime();
bool isOpen();
void closeWheel();
void activateItem(std::uint32_t id);
}
