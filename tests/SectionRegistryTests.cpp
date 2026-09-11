#include "SectionRegistry.h"
#include "WheelPreferences.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {
using namespace palm::api;
void check(bool value){if(!value)throw std::runtime_error("Section API assertion failed");}
struct CallbackState {
 wheel::SectionRegistry* registry{};SectionHandle section{};
 unsigned calls{},item{};std::uint64_t data{};Result removal{},nested{};
};
void selected(std::uint32_t item,std::uint64_t data,void* context) noexcept {
 auto& state=*static_cast<CallbackState*>(context);++state.calls;state.item=item;state.data=data;
 state.removal=state.registry->unregisterSection(state.section);
 state.nested=state.registry->dispatch(state.section,item);
}
SectionV1 section(const char* id,CallbackState& state) {
 SectionV1 value;std::snprintf(value.id,sizeof(value.id),"%s",id);
 std::strcpy(value.name,"Magazines");std::strcpy(value.modName,"Magazine Mod");
 value.icon=Icon::CombatRifle;value.onSelect=selected;value.context=&state;return value;
}
ItemV1 item(unsigned id) {
 ItemV1 value;value.id=id;value.icon=Icon::Pistol10mm;value.quantity=3;
 value.flags=static_cast<unsigned>(ItemFlag::ShowQuantity);std::strcpy(value.name,"10mm Magazine");value.userData=123456;return value;
}
}
int main(){try {
 wheel::SectionRegistry registry;CallbackState state{&registry};
 auto registration=section("example.magazines",state);SectionHandle handle{};
 check(registry.registerSection(&registration,&handle)==Result::Ok && handle);state.section=handle;
 registration.name[0]='X'; // PALM owns the display snapshot after returning.
 auto magazine=item(11);check(registry.setItems(handle,&magazine,1)==Result::Ok);
 magazine.name[0]='X';magazine.userData=9;
 wheel::SectionCatalog snapshot;check(registry.snapshot(snapshot));
 check(snapshot.count==1 && std::strcmp(snapshot.sections[0].name,"Magazines")==0);
 check(std::strcmp(snapshot.sections[0].items[0].name,"10mm Magazine")==0);
 const auto revision=registry.revision();magazine=item(11);
 check(registry.setItems(handle,&magazine,1)==Result::Ok && registry.revision()==revision);
 check(registry.dispatch(handle,11)==Result::Ok && state.calls==1 && state.item==11 && state.data==123456);
 check(state.removal==Result::CallbackBusy && state.nested==Result::CallbackBusy);
 check(registry.dispatch(handle,12)==Result::ItemUnavailable && state.calls==1);
 magazine.flags|=static_cast<unsigned>(ItemFlag::Disabled);check(registry.setItems(handle,&magazine,1)==Result::Ok);
 check(registry.dispatch(handle,11)==Result::ItemUnavailable && state.calls==1);
 check(registry.setItems(handle,nullptr,0)==Result::Ok && registry.dispatch(handle,11)==Result::ItemUnavailable);
 magazine=item(11);check(registry.setItems(handle,&magazine,1)==Result::Ok);
 registry.clearItems();check(registry.snapshot(snapshot) && snapshot.count==1 && snapshot.sections[0].count==0);
 check(registry.dispatch(handle,11)==Result::ItemUnavailable);
 check(registry.unregisterSection(handle)==Result::Ok && registry.dispatch(handle,11)==Result::NotFound);
 auto again=section("example.magazines",state);SectionHandle replacement{};
 check(registry.registerSection(&again,&replacement)==Result::Ok && replacement!=handle);
 SectionHandle rejected=99;check(registry.registerSection(&again,&rejected)==Result::DuplicateId && !rejected);
 again.structSize=0;check(registry.registerSection(&again,&rejected)==Result::InvalidArgument);
 again=section("bad id",state);check(registry.registerSection(&again,&rejected)==Result::InvalidArgument);
 again=section("example.bad",state);again.name[0]=0;check(registry.registerSection(&again,&rejected)==Result::InvalidArgument);
 std::array<ItemV1,9> items;for(unsigned i=0;i<items.size();++i)items[i]=item(i+1);
 check(registry.setItems(replacement,items.data(),9)==Result::InvalidArgument);
 check(registry.setItems(replacement,items.data(),8)==Result::Ok);
 items[1].id=items[0].id;check(registry.setItems(replacement,items.data(),8)==Result::DuplicateId);
 items[1]=item(2);items[0].name[0]=0;check(registry.setItems(replacement,items.data(),8)==Result::InvalidArgument);
 check(registry.snapshot(snapshot) && snapshot.sections[0].count==8); // Failed replacement is atomic.
 for(unsigned i=1;i<kMaxSections;++i) {
  char id[32];std::snprintf(id,sizeof(id),"example.section%u",i);again=section(id,state);
  check(registry.registerSection(&again,&rejected)==Result::Ok);
 }
 again=section("example.overflow",state);check(registry.registerSection(&again,&rejected)==Result::CapacityReached);
 wheel::Preferences prefs;
 check(prefs.enabledEntries()==8 && prefs.setSectionEnabled("example.magazines",true));
 check(prefs.setSectionEnabled("example.other",true) && prefs.enabledEntries()==10);
 check(!prefs.setSectionEnabled("example.third",true));
 prefs.gesturesEnabled=false;check(prefs.setSectionEnabled("example.third",true));
 check(prefs.setSectionEnabled("example.other",false) && !prefs.sectionEnabled("example.other"));
 std::ostringstream out;wheel::writePreferences(out,prefs);wheel::Preferences loaded;std::istringstream in(out.str());
 check(wheel::readPreferences(in,loaded) && !loaded.gesturesEnabled && loaded.sections==prefs.sections);
 std::istringstream previous("PALMItems 1\ncategory 0 1\nitem 0 \"old-choice\" \"Stimpak\"\ncategory 1 0\ncategory 2 1\ncategory 3 1\ncategory 4 1\n");
 check(wheel::readPreferences(previous,loaded) && loaded.gesturesEnabled && loaded.slots[0].size()==1 && !loaded.enabled[1]);
 std::cout<<"Section capacity, snapshots, callback lifetime, stale selections and visibility persistence passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
