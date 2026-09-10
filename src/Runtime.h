#pragma once
#include "WheelView.h"
#include <mutex>

namespace wheel {
struct SharedModel { std::mutex mutex; Model model; View view; std::string lastAction; };
SharedModel& sharedModel();
bool startRuntime();
void closeWheel(std::uint64_t expectedGeneration=0);
void failWheelPresentation(std::uint64_t expectedGeneration);
void beginGameLoad();
void finishGameLoad(bool success);
// The renderer publishes the last drawn hover; the input callback owns release.
std::uint64_t wheelDrawGeneration();
void publishWheelSelection(std::uint64_t generation,const Action& hover);
}
