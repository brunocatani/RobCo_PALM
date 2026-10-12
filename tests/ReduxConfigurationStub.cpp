#include <SCISSORS_Redux/Configuration.h>
namespace config=scissors::configuration;
namespace {
    config::SnapshotV1 state{sizeof(config::SnapshotV1),1,1};
    config::Status snapshot(config::SnapshotV1* result) noexcept {if(!result)return config::Status::InvalidArgument;*result=state;return config::Status::Ok;}
    config::Status set(config::Setting id,std::uint32_t value,char*,std::uint32_t) noexcept {
        if(static_cast<std::uint32_t>(id)>=config::kSettings.size()||value>1)return config::Status::InvalidArgument;
        state.values=value?state.values|config::bit(id):state.values&~config::bit(id);++state.revision;return config::Status::Ok;
    }
    const config::ApiV1 api{sizeof(config::ApiV1),1,0,0,snapshot,set};
}
extern "C" __declspec(dllexport) const config::ApiV1* SCISSORS_Redux_GetConfiguration(std::uint32_t major,std::uint32_t minor,std::uint32_t size) noexcept {
    return major==1&&minor==0&&size==sizeof(config::ApiV1)?&api:nullptr;
}
