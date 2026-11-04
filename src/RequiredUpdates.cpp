#include "PCH.h"
#include "RequiredUpdates.h"
#include <RPS/Runtime/RequiredUpdate.h>
#include <RobCoPALM/Menu.h>
#include <atomic>
#include <string>

namespace wheel::required_updates {
namespace {
namespace required_update=RPS::Runtime::RequiredUpdate;
// DLL file names for the log; characters outside ASCII become '?'.
std::string narrowForLog(const std::wstring& text) {
    std::string result;result.reserve(text.size());
    for(const wchar_t c:text)result.push_back(c>0&&c<0x80?static_cast<char>(c):'?');
    return result;
}
struct Caller {std::wstring dll;std::string dllForLog;};
Caller identify(const void* address) {
    Caller caller{required_update::moduleFileNameAt(address),{}};
    caller.dllForLog=caller.dll.empty()?std::string{"an unidentified module"}:narrowForLog(caller.dll);
    if(caller.dll.empty())caller.dll=L"A mod (see RobCoPALM.log)";
    return caller;
}
void reportConsumerOlder(const Caller& caller) {
    required_update::report(caller.dll+L" was built for an older RobCo PALM and does not work with the installed one. Update it or remove it.");
}
}
void noteDiscoveryRefusal(const palm::api::Result result,const palm::api::InterfaceId id,const std::uint32_t requestedMajor,const void* callerAddress) noexcept {
    using palm::api::Result;
    bool consumerOlder=false;
    switch(result) {
    case Result::UnsupportedMajor:
    case Result::UnknownInterface:consumerOlder=requestedMajor<palm::api::menu::kMajor;break;
    case Result::UnsupportedMinor:
    case Result::TableTooSmall:break;
    default:return; // not a version refusal
    }
    if(!required_update::firstFromCaller(callerAddress))return;
    try {
        const auto caller=identify(callerAddress);
        if(consumerOlder) {
            spdlog::critical("{} asked for interface {} major {}: it was built for an older RobCo PALM. The game stops at the main menu until it is updated or removed.",
                caller.dllForLog,static_cast<std::uint32_t>(id),requestedMajor);
            reportConsumerOlder(caller);
        }else {
            spdlog::critical("{} asked for interface {} major {} (result {}): it needs a newer RobCo PALM. The game stops at the main menu until RobCo PALM is updated.",
                caller.dllForLog,static_cast<std::uint32_t>(id),requestedMajor,static_cast<std::uint32_t>(result));
            required_update::report(caller.dll+L" needs a newer RobCo PALM than the installed one. Update RobCo PALM.");
        }
    }catch(...){}
}
void noteRetiredExport(const char* exportName,const void* callerAddress) noexcept {
    if(!required_update::firstFromCaller(callerAddress))return;
    try {
        const auto caller=identify(callerAddress);
        spdlog::critical("{} called {}, RobCo PALM's retired pre-2.0 API: it was built for an older RobCo PALM. The game stops at the main menu until it is updated or removed.",
            caller.dllForLog,exportName?exportName:"a retired export");
        reportConsumerOlder(caller);
    }catch(...){}
}
void noteRpsUiWithoutDiscovery() noexcept {
    static std::atomic_flag reported=ATOMIC_FLAG_INIT;
    if(reported.test_and_set())return;
    try {
        spdlog::critical("RPS_UI_Framework.dll does not export its 2.0 discovery: it is older than PALM supports. The game stops at the main menu until they match.");
        required_update::report(L"RobCo PALM does not work with the installed RPS UI Framework: it is too old. Update RPS UI Framework.");
    }catch(...){}
}
}
