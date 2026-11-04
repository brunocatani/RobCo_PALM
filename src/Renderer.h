#pragma once
#include <RPS_UI_Framework/Framework.h>
namespace wheel {
bool installPanel();
void unregisterPanel();
bool frameworkReady();
bool presentPanel(bool open,const rpsui::api::framework::PanelPose* pose=nullptr);
}
