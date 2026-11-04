#include "InterfaceNegotiation.h"
#include <RobCoPALM/Client.h>
#include <RobCoPALM/Developer.h>
#include <RobCoPALM/Menu.h>
#include <array>
#include <cassert>
#include <iostream>

// Discovery negotiation over PALM's real interface tables: exact major, minimum
// minor and minimum table size; every pre-2.0 major is refused.
namespace {
using namespace palm::api;
const menu::Api g_menu{};
const developer::Api g_developer{};
const std::array g_entries{wheel::discovery::describe(g_menu),wheel::discovery::describe(g_developer)};
Result PALM_CALL queryEntries(InterfaceId id,std::uint32_t major,std::uint32_t minor,std::uint32_t bytes,const InterfaceDescriptorV1** output) noexcept {
 return wheel::discovery::query(g_entries,id,major,minor,bytes,output);
}
Result PALM_CALL wrongTable(InterfaceId,std::uint32_t,std::uint32_t,std::uint32_t,const InterfaceDescriptorV1** output) noexcept {
 *output=&g_entries[1];return Result::Ok;
}
}

int main() {
 const InterfaceDescriptorV1* out{};
 assert(queryEntries(InterfaceId::Menu,2,0,sizeof(menu::Api),&out)==Result::Ok);
 assert(out==&g_entries[0] && out->table==&g_menu && out->tableByteSize==sizeof(menu::Api));
 assert(queryEntries(InterfaceId::Developer,2,0,sizeof(developer::Api),&out)==Result::Ok && out==&g_entries[1]);

 // Only major 2 is served.
 for(const std::uint32_t major:{1u,3u}) {
  out=&g_entries[0];
  assert(queryEntries(InterfaceId::Menu,major,0,8,&out)==Result::UnsupportedMajor && !out);
  assert(queryEntries(InterfaceId::Developer,major,0,8,&out)==Result::UnsupportedMajor && !out);
 }

 // An older (smaller) table request is served; a newer minor or larger table is not.
 assert(queryEntries(InterfaceId::Menu,2,0,8,&out)==Result::Ok && out);
 assert(queryEntries(InterfaceId::Menu,2,1,8,&out)==Result::UnsupportedMinor && !out);
 assert(queryEntries(InterfaceId::Menu,2,0,sizeof(menu::Api)+8,&out)==Result::TableTooSmall && !out);

 assert(queryEntries(static_cast<InterfaceId>(99),2,0,8,&out)==Result::UnknownInterface && !out);
 assert(queryEntries(InterfaceId::Menu,0,0,8,&out)==Result::InvalidArgument);
 assert(queryEntries(InterfaceId::Menu,2,0,0,&out)==Result::InvalidArgument);
 assert(queryEntries(InterfaceId::Menu,2,0,8,nullptr)==Result::InvalidArgument);

 // The consumer helper binds the matching table and rejects a wrong descriptor.
 const developer::Api* actions{};
 assert(acquire(&queryEntries,actions)==Result::Ok && actions==&g_developer);
 const menu::Api* wrong{};
 assert(acquire(&wrongTable,wrong)==Result::InternalError && !wrong);
 assert(acquire(QueryInterfaceV1{},wrong)==Result::InvalidArgument && !wrong);
 std::cout<<"PASS PALM discovery: exact major, minimum minor, table size, pre-2.0 refusal\n";
}
