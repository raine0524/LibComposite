#include "trace_info.h"

const std::string TraceInfo::mod_name_[APP_END_MOD] = {
    "",
    "FRMW",
    "H264D",
    "H264E",
    "VPP",
    "MSMT"
};

const std::string TraceInfo::level_name_[SYS_DEBUG+1] = {
    "",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

const std::string& TraceInfo::mod_name(AppModId id)
{
    if (id < APP_END_MOD)
        return mod_name_[id];
    return mod_name_[0];
}

const std::string& TraceInfo::level_name(TraceLevel level)
{
    if (level >= SYS_ERROR && level <= SYS_DEBUG)
        return level_name_[level];
    return level_name_[0];
}
