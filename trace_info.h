/*
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 *\file trace_info.h
 *\brief Implementation for trace info class
 */

#ifndef TRACE_INFO_H_
#define TRACE_INFO_H_

//#define DEBUG

#include <string>
#include <ctime>

typedef enum {
    APP_START_MOD = 0,
    FRMW,       /*composite framework*/
    H264D,      /*H264 decoder*/
    H264E,      /*H264 encoder*/
    VPP,        /*Video Post Processing*/
    MSMT,       /*Measurement*/
    APP_MAX_MOD_NUM,
    APP_END_MOD = APP_MAX_MOD_NUM
} AppModId;

typedef enum {
    SYS_ERROR = 1,
    SYS_WARNING,
    SYS_INFORMATION,
    SYS_DEBUG
} TraceLevel;

class TraceInfo
{
private:
    AppModId mod_;
    TraceLevel level_;
    time_t time_;
    std::string desc_;

    static const std::string mod_name_[APP_END_MOD];
    static const std::string level_name_[SYS_DEBUG+1];

public:
    TraceInfo(AppModId mod, TraceLevel level, std::string desc) : mod_(mod), level_(level)
    {
        desc_.erase();
        desc_ = desc;
        time_ = std::time(0);
    }

    AppModId mod()      { return mod_; }
    TraceLevel level()  { return level_; }
    time_t *time()      { return &time_; }
    std::string &desc() { return desc_; }

    static const std::string& mod_name(AppModId);
    static const std::string& level_name(TraceLevel);
};
#endif
