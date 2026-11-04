#include "DeveloperActions.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <stdexcept>
using palm::api::Result;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
struct Context {wheel::DeveloperActions* actions{};palm::api::developer::Handle handle{};unsigned calls{};Result removal{};};
void invoke(void* pointer) noexcept {auto& c=*static_cast<Context*>(pointer);++c.calls;c.removal=c.actions->remove(c.handle);}
int main() {
 try {
  wheel::SectionRegistry sections;wheel::DeveloperActions actions(sections);Context context{&actions};
  palm::api::developer::Action action;std::strcpy(action.id,"fixture.capture");std::strcpy(action.label,"Fixture: Capture data");action.invoke=invoke;action.context=&context;
  palm::api::developer::Handle handle{};
  require(actions.add(&action,&handle)==Result::Disabled && !handle,"disabled developer action visible");
  require(actions.initialize(true),"developer initialization");
  require(actions.add(&action,&context.handle)==Result::Ok,"registration");
  require(actions.add(&action,&handle)==Result::DuplicateId,"duplicate replaced action");
  wheel::SectionCatalog catalog;require(sections.snapshot(catalog) && catalog.count==1 && catalog.sections[0].count==1,"developer snapshot");
  const auto section=catalog.sections[0].handle;
  require(actions.ownsSection(section),"developer selection must close the wheel");
  require(sections.dispatch(section,context.handle)==palm::api::Result::Ok && context.calls==1 && context.removal==Result::CallbackBusy,"callback ownership");
  sections.clearItems();actions.refresh();require(sections.snapshot(catalog) && catalog.sections[0].count==1,"game load lost developer actions");
  for(unsigned i=1;i<palm::api::developer::kCapacity;++i){std::snprintf(action.id,sizeof(action.id),"fixture.%u",i);require(actions.add(&action,&handle)==Result::Ok,"capacity allocation");}
  std::strcpy(action.id,"fixture.overflow");require(actions.add(&action,&handle)==Result::CapacityReached && !handle,"full wheel evicted an action");
  require(sections.snapshot(catalog) && catalog.sections[0].count==8 && catalog.sections[0].items[0].id==context.handle,"full wheel changed existing actions");
  require(actions.remove(context.handle)==Result::Ok,"explicit removal");
  require(sections.dispatch(section,context.handle)==palm::api::Result::ItemUnavailable,"stale selection dispatched");
  require(actions.initialize(false),"disable");require(actions.add(&action,&handle)==Result::Disabled,"disabled registration accepted");
  require(sections.snapshot(catalog) && catalog.sections[0].count==0,"disabled items visible");
  std::cout<<"Developer wheel: opt-in visibility, eight-action capacity, no eviction, game-load persistence and callback lifetime passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
