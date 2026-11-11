#pragma once
#include <RPS_UI_Framework/Framework.h>
namespace wheel {
bool installPanel();
void unregisterPanel();
// The RPS UI owner token the wheel registered with; 0 before installPanel.
std::uint64_t frameworkOwner();
bool frameworkReady();
bool presentPanel(bool open,const rpsui::api::framework::PanelPose* pose=nullptr);
// A controller pulse through RPS UI, at the player's haptic strength.
void pulseHand(rpsui::api::framework::PhysicalHand hand,float strength);
}
