#pragma once
#include "WheelPreferences.h"

namespace wheel {
// One owned record in the F4SE co-save. Keys are plugin/local-ID strings and
// equipment variant fingerprints, not load-order-dependent runtime FormIDs.
inline constexpr std::uint32_t kWheelSaveID=0x5257464D; // RWFM
inline constexpr std::uint32_t kPreferencesRecord=0x50524546; // PREF
inline constexpr std::uint32_t kPreferencesVersion=1;
// Covers all forty maximum-length keys/names, including quoted escaping.
inline constexpr std::uint32_t kMaxPreferencesBytes=128*1024;
enum class SaveReadResult { Missing, Loaded, Invalid };

template<class Serialization>
bool writeWheelSave(const Serialization& stream,const Preferences& prefs) {
 std::ostringstream out;writePreferences(out,prefs);
 const auto payload=out.str();
 return out.good() && payload.size()<=kMaxPreferencesBytes &&
  stream.WriteRecord(kPreferencesRecord,kPreferencesVersion,payload.data(),static_cast<std::uint32_t>(payload.size()));
}

template<class Serialization>
SaveReadResult readWheelSave(const Serialization& stream,Preferences& prefs) {
 prefs={};
 std::uint32_t type{},version{},length{};
 if(!stream.GetNextRecordInfo(type,version,length))return SaveReadResult::Missing;
 if(type!=kPreferencesRecord || version!=kPreferencesVersion || !length || length>kMaxPreferencesBytes)
  return SaveReadResult::Invalid;
 std::string payload(length,'\0');
 if(stream.ReadRecordData(payload.data(),length)!=length)return SaveReadResult::Invalid;
 Preferences parsed;std::istringstream in(payload);
 // Reject incomplete or ambiguous records without retaining the previous save.
 if(!readPreferences(in,parsed) || stream.GetNextRecordInfo(type,version,length))return SaveReadResult::Invalid;
 prefs=std::move(parsed);return SaveReadResult::Loaded;
}
}
