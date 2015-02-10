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
 *\file trace_filter.cpp
 *\brief Implementation for TraceFilter class
 */
#include "trace_filter.h"

void TraceFilter::enable_all(TraceLevel level)
{
    for (int idx = 0; idx < APP_END_MOD; idx++) {
        filters_[idx].enable(true);
        filters_[idx].level(level);
    }
}

void TraceFilter::enable(AppModId mod, TraceLevel level)
{
    filters_[mod].enable(true);
    filters_[mod].level(level);
}

//@param 'override_flag' set true or false is ok, this flag is just used to tell
//compilers it is different from `TraceFilter::enable(AppModId mod, TraceLevel level)`
bool TraceFilter::enable( AppModId mod, TraceLevel level, int override_flag)
{
    return filters_[mod].enable(level);
}
