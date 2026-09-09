#pragma once
#include "RPSUIFrameworkApi.h"
namespace wheel {
bool installPanel();
void unregisterPanel();
bool presentPanel(bool open,const rpsui::sdk::PanelPoseV1* pose=nullptr);
}
