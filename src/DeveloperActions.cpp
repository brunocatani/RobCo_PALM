#include "DeveloperActions.h"
#include <cstring>
#include <limits>
#include <Windows.h>

namespace wheel {
DeveloperActions& developerActions(){static DeveloperActions value(sectionRegistry());return value;}
bool DeveloperActions::initialize(bool enabled) {
 std::scoped_lock lock(mutex_);
 if(enabled && !section_) {
  palm::api::menu::Section section;std::strcpy(section.id,kDeveloperSection);
  std::strcpy(section.name,"Developer");std::strcpy(section.modName,"Local diagnostics");
  section.onSelect=selected;section.context=this;
  if(registry_.registerSection(&section,&section_)!=palm::api::Result::Ok)return false;
 }
 enabled_.store(enabled,std::memory_order_release);publish();return true;
}
palm::api::Result DeveloperActions::add(const palm::api::developer::Action* action,palm::api::developer::Handle* output) {
 using namespace palm::api;using namespace palm::api::developer;
 if(output)*output=0;
 const auto valid=[](const char* text,std::size_t capacity){
  const auto* end=static_cast<const char*>(std::memchr(text,0,capacity));
  if(!end || end==text)return false;
  for(auto* p=text;p!=end;++p)if(static_cast<unsigned char>(*p)<32)return false;
  return true;
 };
 if(!action || !output || action->size!=sizeof(Action) || action->version!=kRecordVersion || !action->invoke ||
  !valid(action->id,sizeof(action->id)) || !valid(action->label,sizeof(action->label)))return Result::InvalidArgument;
 std::scoped_lock lock(mutex_);if(!enabled())return Result::Disabled;
 Entry* empty{};
 for(auto& entry:entries_) {
  if(entry.handle && std::strcmp(entry.action.id,action->id)==0)return Result::DuplicateId;
  if(!entry.handle && !empty)empty=&entry;
 }
 if(!empty || next_==std::numeric_limits<Handle>::max())return Result::CapacityReached;
 empty->action=*action;empty->handle=next_++;*output=empty->handle;publish();return Result::Ok;
}
palm::api::Result DeveloperActions::remove(palm::api::developer::Handle handle) {
 using namespace palm::api;using namespace palm::api::developer;std::scoped_lock lock(mutex_);
 for(auto& entry:entries_)if(handle && entry.handle==handle) {
  if(entry.invoking)return Result::CallbackBusy;
  entry={};publish();return Result::Ok;
 }
 return Result::NotFound;
}
void DeveloperActions::publish() {
 if(!section_)return;
 std::array<palm::api::menu::Item,palm::api::developer::kCapacity> items{};unsigned count{};
 if(enabled())for(const auto& entry:entries_)if(entry.handle) {
  auto& item=items[count++];item.id=entry.handle;item.icon=palm::api::Icon::Config;
  std::memcpy(item.name,entry.action.label,sizeof(item.name));
 }
 (void)registry_.setItems(section_,items.data(),count);
}
void DeveloperActions::refresh(){std::scoped_lock lock(mutex_);publish();}
void DeveloperActions::selected(std::uint32_t id,std::uint64_t,void* context) noexcept {
 try{static_cast<DeveloperActions*>(context)->dispatch(id);}catch(...){OutputDebugStringA("PALM developer action dispatch failed\n");}
}
void DeveloperActions::dispatch(std::uint32_t id) {
 palm::api::developer::Callback invoke{};void* context{};
 {
  std::scoped_lock lock(mutex_);if(!enabled())return;
  for(auto& entry:entries_)if(entry.handle==id && !entry.invoking){entry.invoking=true;invoke=entry.action.invoke;context=entry.action.context;break;}
 }
 if(!invoke)return;invoke(context);
 std::scoped_lock lock(mutex_);for(auto& entry:entries_)if(entry.handle==id){entry.invoking=false;break;}
}
}
