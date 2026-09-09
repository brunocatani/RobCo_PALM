#include "WheelConfig.h"
#include "WheelSaveRecord.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok){if(!ok)throw std::runtime_error("Wheel save-state assertion failed");}
struct Record {
 std::uint32_t type{},version{},length{};
 std::string payload;
};
// Implements the record boundary used by F4SE, without touching real saves.
struct CoSave {
 mutable std::vector<Record> records;
 mutable std::size_t next{};
 bool writable=true;
 bool WriteRecord(std::uint32_t type,std::uint32_t version,const void* bytes,std::uint32_t size) const {
  if(!writable)return false;
  records.push_back({type,version,size,std::string(static_cast<const char*>(bytes),size)});return true;
 }
 bool GetNextRecordInfo(std::uint32_t& type,std::uint32_t& version,std::uint32_t& length) const {
  if(next==records.size())return false;
  const auto& record=records[next++];type=record.type;version=record.version;length=record.length;return true;
 }
 std::uint32_t ReadRecordData(void* bytes,std::uint32_t length) const {
  const auto& payload=records.at(next-1).payload;
  const auto count=static_cast<std::uint32_t>((std::min)(payload.size(),static_cast<std::size_t>(length)));
  std::memcpy(bytes,payload.data(),count);return count;
 }
};
bool empty(const wheel::Preferences& prefs) {
 return std::all_of(prefs.slots.begin(),prefs.slots.end(),[](const auto& list){return list.empty();});
}
}
int main(){try {
 using namespace wheel;
 Preferences a,b;
 for(unsigned c=0;c<kCategoryCount;++c)for(unsigned i=0;i<kSlots;++i)
  check(a.select(c,{"example.esp|"+std::to_string(i),"Favorite "+std::to_string(i)},true));
 a.enabled[1]=false;
 check(b.select(3,{"example.esp|abcdef|variant:1234","Different rifle"},true));
 b.enabled[4]=false;
 CoSave saveA,saveB;
 restoreWheelPreferences(a);check(writeWheelSave(saveA,snapshotWheelPreferences()));
 restoreWheelPreferences(b);check(writeWheelSave(saveB,snapshotWheelPreferences()));
 Preferences restored;
 check(readWheelSave(saveA,restored)==SaveReadResult::Loaded);restoreWheelPreferences(restored);
 auto current=snapshotWheelPreferences();
 check(current.slots[3].size()==8 && !current.enabled[1] && current.enabled[4]);
 Model inventory;
 for(unsigned c=0;c<kCategoryCount;++c)inventory.items[c].push_back({100+c,"Carried",1,false,"example.esp|0"});
 publishWheelInventory(inventory);
 const auto visible=selectedWheelInventory(inventory);
 for(unsigned c=0;c<kCategoryCount;++c)check(visible.items[c].size()==(a.enabled[c]?1:0));
 check(readWheelSave(saveB,restored)==SaveReadResult::Loaded);restoreWheelPreferences(restored);
 current=snapshotWheelPreferences();
 check(current.slots[3].size()==1 && current.enabled[1] && !current.enabled[4]);
 check(selectedWheelInventory(inventory).items[3].empty()); // No other save's weapon.
 CoSave oldSave;check(readWheelSave(oldSave,restored)==SaveReadResult::Missing);
 restoreWheelPreferences(restored);check(empty(snapshotWheelPreferences()));
 restoreWheelPreferences(a);restoreWheelPreferences({}); // Revert/new game, even without a load callback.
 check(empty(snapshotWheelPreferences()) && !takeWheelConfigChange());
 // Every malformed record must clear, not retain, the previous session.
 const auto invalid=[&](CoSave broken) {
  Preferences previous=a;
  check(readWheelSave(broken,previous)==SaveReadResult::Invalid);check(empty(previous));
 };
 auto broken=saveA;broken.next=0;broken.records[0].version++;invalid(broken);
 broken=saveA;broken.next=0;broken.records[0].type=0;invalid(broken);
 broken=saveA;broken.next=0;broken.records[0].length=kMaxPreferencesBytes+1;invalid(broken);
 broken=saveA;broken.next=0;broken.records[0].payload.pop_back();invalid(broken);
 broken=saveA;broken.next=0;broken.records.push_back(broken.records[0]);invalid(broken);
 broken=saveA;broken.next=0;broken.records[0].payload[0]='!';invalid(broken);
 CoSave failed;failed.writable=false;check(!writeWheelSave(failed,a));
 Preferences longest;
 for(unsigned c=0;c<kCategoryCount;++c)for(unsigned i=0;i<kSlots;++i)
  check(longest.select(c,{std::to_string(i)+std::string(299,'"'),std::string(512,'"')},true));
 CoSave escaped;check(writeWheelSave(escaped,longest));
 check(readWheelSave(escaped,restored)==SaveReadResult::Loaded && restored.slots[4].size()==8);
 std::cout<<"Per-save round trips, session isolation, missing inventory and invalid records passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
