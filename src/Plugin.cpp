#include "PCH.h"
#include "Runtime.h"
#include "Fonts.h"
#include "WheelConfig.h"
#include "WheelSaveRecord.h"
#include "tools/ConfiguratorRuntime.h"
#include <spdlog/sinks/rotating_file_sink.h>

namespace {
void F4SEAPI onRevert(const F4SE::SerializationInterface*) noexcept {
 try {wheel::beginGameLoad();}
 catch(...){spdlog::error("PALM save-state reset failed");}
}
void F4SEAPI onSave(const F4SE::SerializationInterface* stream) noexcept {
 try {
  if(!stream || !wheel::writeWheelSave(*stream,wheel::snapshotWheelPreferences()))
   spdlog::error("Could not write wheel favorites to the F4SE co-save");
 }catch(...){spdlog::error("PALM favorites serialization failed");}
}
void F4SEAPI onLoad(const F4SE::SerializationInterface* stream) noexcept {
 try {
  wheel::Preferences prefs;
  const auto result=stream?wheel::readWheelSave(*stream,prefs):wheel::SaveReadResult::Missing;
  wheel::restoreWheelPreferences(std::move(prefs));
  if(result==wheel::SaveReadResult::Invalid)spdlog::error("Invalid wheel favorites co-save record; favorites reset for this save");
  else spdlog::info("PALM favorites: {}",result==wheel::SaveReadResult::Loaded?"restored from co-save":"no saved selections");
 }catch(...){spdlog::error("PALM favorites restoration failed; session selections remain empty");}
}
void onMessage(F4SE::MessagingInterface::Message* message) noexcept {
 try {
  if(!message)return;
  // PreLoad also runs when no co-save exists and F4SE never invokes onLoad.
  if(message->type==F4SE::MessagingInterface::kPreLoadGame) {wheel::beginGameLoad();return;}
  if(message->type==F4SE::MessagingInterface::kNewGame) {
   wheel::beginGameLoad();rock_configurator::onGameSessionReady();wheel::finishGameLoad(true);return;
  }
  if(message->type==F4SE::MessagingInterface::kPostLoadGame) {
   // F4SEVR sends the result as the pointer value, not a pointer to a bool.
   const bool success=message->data!=nullptr;
   if(success)rock_configurator::onGameSessionReady();
   wheel::finishGameLoad(success);return;
  }
  if(message->type!=F4SE::MessagingInterface::kGameDataReady)return;
  static bool started=false;if(started)return;started=true;
  wheel::prepareFonts();
  if(!wheel::startRuntime())spdlog::critical("PALM initialization failed; check ROCK and RPS UI Framework are loaded");
  else {rock_configurator::onGameDataReady();spdlog::info("PALM and workshop ready for gameplay input");}
 }catch(const std::exception& e){spdlog::critical("PALM startup: {}",e.what());}
 catch(...){OutputDebugStringA("RobCoPALM startup failed\n");}
}
}
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* f4se,F4SE::PluginInfo* info) noexcept {
 try {
  if(!f4se || !info || f4se->IsEditor() || !REL::Module::IsVR() || REL::Module::get().version()!=F4SE::RUNTIME_VR_1_2_72)return false;
  auto directory=F4SE::log::log_directory();if(!directory)return false;
  if(!directory->generic_string().ends_with("Fallout4VR/F4SE")) *directory=directory->parent_path()/"Fallout4VR/F4SE";
  std::filesystem::create_directories(*directory);
  auto logger=spdlog::rotating_logger_mt("RobCoPALM",(*directory/"RobCoPALM.log").string(),2*1024*1024,2,true);
  spdlog::set_default_logger(logger);logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");logger->flush_on(spdlog::level::info);
  info->infoVersion=F4SE::PluginInfo::kVersion;info->name="RobCoPALM";info->version=1;
  spdlog::info("Query passed for Fallout4VR.exe 1.2.72");return true;
 }catch(...){OutputDebugStringA("RobCoPALM Query failed\n");return false;}
}
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* f4se) noexcept {
 try {
  if(!f4se)return false;F4SE::Init(f4se,false);
  const auto* messaging=F4SE::GetMessagingInterface();
  const auto* serialization=F4SE::GetSerializationInterface();
  if(!serialization || !messaging || !messaging->RegisterListener(onMessage))return false;
  serialization->SetUniqueID(wheel::kWheelSaveID);
  serialization->SetRevertCallback(onRevert);
  serialization->SetSaveCallback(onSave);
  serialization->SetLoadCallback(onLoad);
  spdlog::info("Load complete");return true;
 }catch(...){OutputDebugStringA("RobCoPALM Load failed\n");return false;}
}
