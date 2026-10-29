#include <SCISSORS_Redux/Configuration.h>
namespace config=scissors::configuration;
namespace {
    // Starts from Redux's compiled defaults, as the real module does without INIs.
    constexpr std::uint32_t defaults() {std::uint32_t bits=0;for(const auto& s:config::kSettings)if(s.defaultValue)bits|=config::bit(s.id);return bits;}
    config::Snapshot state{sizeof(config::Snapshot),defaults(),1};
    config::Status snapshot(config::Snapshot* result) noexcept {if(!result)return config::Status::InvalidArgument;*result=state;return config::Status::Ok;}
    config::Status set(config::Setting id,std::uint32_t value,char*,std::uint32_t) noexcept {
        if(static_cast<std::uint32_t>(id)>=config::kSettings.size()||value>1)return config::Status::InvalidArgument;
        state.values=value?state.values|config::bit(id):state.values&~config::bit(id);++state.revision;return config::Status::Ok;
    }
    const config::Api api{sizeof(config::Api),config::kMajor,config::kMinor,0,snapshot,set};
}
extern "C" __declspec(dllexport) const config::Api* SCISSORS_Redux_GetConfiguration(std::uint32_t major,std::uint32_t minor,std::uint32_t size) noexcept {
    return major==config::kMajor&&minor<=config::kMinor&&size>=sizeof(config::Api)?&api:nullptr;
}
