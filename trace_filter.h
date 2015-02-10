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
 *\file trace_filter.h
 *\brief Definition for ModFilter & TraceFilter
 */

#ifndef TRACE_FILTER_H_
#define TRACE_FILTER_H_
#include "trace_info.h"


class ModFilter
{
public:
    ModFilter(): enable_(false), level_(SYS_INFORMATION) {}

    void enable(bool flag) { enable_ = flag; }
    void level(TraceLevel level) { level_ = level; }


    bool enable(TraceLevel level)
    {
        if (enable_ && level <= level_) {
            return true;
        } else {
            return false;
        }
    }

private:
    bool enable_;
    TraceLevel level_;
};

class TraceFilter
{
private:
    ModFilter filters_[APP_END_MOD];

public:
    TraceFilter() {}

    void enable(AppModId mod) { filters_[mod].enable(true); }
    void disable(AppModId mod) { filters_[mod].enable(false); }
    void enable_all(TraceLevel);

    void enable(AppModId, TraceLevel);
    bool enable(AppModId, TraceLevel, int);
};
#endif
