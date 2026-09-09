#include "PCH.h"
#include "Runtime.h"
#include "Fonts.h"
#include <spdlog/sinks/rotating_file_sink.h>

namespace {
void onMessage(F4SE::MessagingInterface::Message* message) noexcept {
 try {
  if(!message || message->type!=F4SE::MessagingInterface::kGameDataReady)return;
  static bool started=false;if(started)return;started=true;
  wheel::prepareFonts();
  if(!wheel::startRuntime())spdlog::critical("Wheel initialization failed; check ROCK and RPS UI Framework are loaded");
  else spdlog::info("Wheel ready for gameplay input");
 }catch(const std::exception& e){spdlog::critical("Wheel startup: {}",e.what());}
 catch(...){OutputDebugStringA("ROCKWheelMenu startup failed\n");}
}
}
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* f4se,F4SE::PluginInfo* info) noexcept {
 try {
  if(!f4se || !info || f4se->IsEditor() || !REL::Module::IsVR() || REL::Module::get().version()!=F4SE::RUNTIME_VR_1_2_72)return false;
  auto directory=F4SE::log::log_directory();if(!directory)return false;
  if(!directory->generic_string().ends_with("Fallout4VR/F4SE")) *directory=directory->parent_path()/"Fallout4VR/F4SE";
  std::filesystem::create_directories(*directory);
  auto logger=spdlog::rotating_logger_mt("ROCKWheelMenu",(*directory/"ROCKWheelMenu.log").string(),2*1024*1024,2,true);
  spdlog::set_default_logger(logger);logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");logger->flush_on(spdlog::level::info);
  info->infoVersion=F4SE::PluginInfo::kVersion;info->name="ROCKWheelMenu";info->version=1;
  spdlog::info("Query passed for Fallout4VR.exe 1.2.72");return true;
 }catch(...){OutputDebugStringA("ROCKWheelMenu Query failed\n");return false;}
}
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* f4se) noexcept {
 try {
  if(!f4se)return false;F4SE::Init(f4se,false);
  const auto* messaging=F4SE::GetMessagingInterface();
  if(!messaging || !messaging->RegisterListener(onMessage))return false;
  spdlog::info("Load complete");return true;
 }catch(...){OutputDebugStringA("ROCKWheelMenu Load failed\n");return false;}
}
