#include "SectionRegistry.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace wheel {
namespace {
using namespace palm::api;
template<std::size_t N> bool textValid(const char (&text)[N]) {
 const auto* end=static_cast<const char*>(std::memchr(text,0,N));
 return end && end!=text && std::none_of(text,end,[](unsigned char c){return c<32;});
}
bool idValid(const char (&id)[64]) {
 if(!textValid(id))return false;
 return std::all_of(id,id+std::strlen(id),[](unsigned char c){
  return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.';
 });
}
bool iconValid(Icon icon){return icon<Icon::Count;}
}
SectionRegistry& sectionRegistry(){static SectionRegistry registry;return registry;}
SectionRegistry::Entry* SectionRegistry::find(palm::api::SectionHandle handle) {
 if(!handle)return nullptr;
 for(auto& entry:_entries)if(entry.snapshot.handle==handle)return &entry;
 return nullptr;
}
palm::api::Result SectionRegistry::registerSection(const palm::api::SectionV1* section,palm::api::SectionHandle* output) {
 if(output)*output=0;
 if(!section || !output || section->structSize!=sizeof(SectionV1) || !idValid(section->id) ||
  !textValid(section->name) || !textValid(section->modName) || !iconValid(section->icon) || !section->onSelect)return Result::InvalidArgument;
 std::scoped_lock lock(_mutex);
 for(const auto& entry:_entries)if(entry.snapshot.handle && std::strcmp(entry.snapshot.id,section->id)==0)return Result::DuplicateId;
 Entry* empty=nullptr;
 for(auto& entry:_entries)if(!entry.snapshot.handle){empty=&entry;break;}
 if(!empty || _next==std::numeric_limits<SectionHandle>::max())return Result::CapacityReached;
 empty->registration=*section;
 auto& target=empty->snapshot;target={};target.handle=_next++;target.icon=section->icon;
 std::memcpy(target.id,section->id,sizeof(target.id));std::memcpy(target.name,section->name,sizeof(target.name));
 std::memcpy(target.modName,section->modName,sizeof(target.modName));
 *output=target.handle;++_revision;return Result::Ok;
}
palm::api::Result SectionRegistry::unregisterSection(palm::api::SectionHandle handle) {
 std::scoped_lock lock(_mutex);auto* entry=find(handle);
 if(!entry)return Result::NotFound;
 if(entry->invoking)return Result::CallbackBusy;
 *entry={};++_revision;return Result::Ok;
}
palm::api::Result SectionRegistry::setItems(palm::api::SectionHandle handle,const palm::api::ItemV1* items,std::uint32_t count) {
 if(count>kMaxItems || (count && !items))return Result::InvalidArgument;
 constexpr auto flags=static_cast<unsigned>(ItemFlag::Disabled)|static_cast<unsigned>(ItemFlag::Equipped)|static_cast<unsigned>(ItemFlag::ShowQuantity);
 for(unsigned i=0;i<count;++i) {
  const auto& item=items[i];
  if(item.structSize!=sizeof(ItemV1) || !item.id || !textValid(item.name) || !iconValid(item.icon) || (item.flags&~flags))return Result::InvalidArgument;
  for(unsigned j=0;j<i;++j)if(items[j].id==item.id)return Result::DuplicateId;
 }
 std::scoped_lock lock(_mutex);auto* entry=find(handle);
 if(!entry)return Result::NotFound;
 auto& snapshot=entry->snapshot;
 bool same=snapshot.count==count;
 for(unsigned i=0;same && i<count;++i) {
  const auto& old=snapshot.items[i];const auto& next=items[i];
  same=old.id==next.id && old.icon==next.icon && old.quantity==next.quantity && old.flags==next.flags &&
   old.userData==next.userData && std::strcmp(old.name,next.name)==0;
 }
 if(same)return Result::Ok;
 snapshot.count=count;
 for(unsigned i=0;i<kMaxItems;++i)snapshot.items[i]=i<count?items[i]:ItemV1{};
 ++_revision;return Result::Ok;
}
void SectionRegistry::clearItems() {
 std::scoped_lock lock(_mutex);
 for(auto& entry:_entries){entry.snapshot.items={};entry.snapshot.count=0;}
 ++_revision;
}
bool SectionRegistry::snapshot(SectionCatalog& output) const {
 std::unique_lock lock(_mutex,std::try_to_lock);if(!lock.owns_lock())return false;
 output.count=0;
 for(const auto& entry:_entries)if(entry.snapshot.handle)output.sections[output.count++]=entry.snapshot;
 output.revision=_revision.load();return true;
}
palm::api::Result SectionRegistry::dispatch(palm::api::SectionHandle handle,std::uint32_t id) {
 SelectCallback callback{};void* context{};std::uint64_t data{};
 {
  std::scoped_lock lock(_mutex);auto* entry=find(handle);
  if(!entry)return Result::NotFound;
  if(entry->invoking)return Result::CallbackBusy;
  const auto& snapshot=entry->snapshot;
  const auto item=std::find_if(snapshot.items.begin(),snapshot.items.begin()+snapshot.count,[&](const auto& value){return value.id==id;});
  if(item==snapshot.items.begin()+snapshot.count || (item->flags&static_cast<unsigned>(ItemFlag::Disabled)))return Result::ItemUnavailable;
  entry->invoking=true;callback=entry->registration.onSelect;context=entry->registration.context;data=item->userData;
 }
 callback(id,data,context);
 {std::scoped_lock lock(_mutex);find(handle)->invoking=false;}
 return Result::Ok;
}
}
