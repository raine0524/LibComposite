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
 *\file trace.h
 *\brief Definition for Trace
 */

#ifndef TRACE_H_
#define TRACE_H_

#include <pthread.h>
#include <queue>

#include "mutex.h"
#include "trace_file.h"
#include "trace_filter.h"

#define EX_INFO __FILE__, __LINE__

#define FRMW_TRACE_ERROR(fmt, ...)      Trace::trace(FRMW, SYS_ERROR, EX_INFO, fmt, ##__VA_ARGS__)
#define FRMW_TRACE_WARNI(fmt, ...)      Trace::trace(FRMW, SYS_WARNING, EX_INFO, fmt, ##__VA_ARGS__)
#define FRMW_TRACE_DEBUG(fmt, ...)      Trace::trace(FRMW, SYS_DEBUG, EX_INFO, fmt, ##__VA_ARGS__)
#define FRMW_TRACE_INFO(fmt, ...)       Trace::trace(FRMW, SYS_INFORMATION, EX_INFO, fmt, ##__VA_ARGS__)

#define H264D_TRACE_ERROR(fmt, ...)     Trace::trace(H264D, SYS_ERROR, EX_INFO, fmt, ##__VA_ARGS__)
#define H264D_TRACE_WARNI(fmt, ...)     Trace::trace(H264D, SYS_WARNING, EX_INFO, fmt, ##__VA_ARGS__)
#define H264D_TRACE_DEBUG(fmt, ...)     Trace::trace(H264D, SYS_DEBUG, EX_INFO, fmt, ##__VA_ARGS__)
#define H264D_TRACE_INFO(fmt, ...)      Trace::trace(H264D, SYS_INFORMATION, EX_INFO, fmt, ##__VA_ARGS__)

#define H264E_TRACE_ERROR(fmt, ...)     Trace::trace(H264E, SYS_ERROR, EX_INFO, fmt, ##__VA_ARGS__)
#define H264E_TRACE_WARNI(fmt, ...)     Trace::trace(H264E, SYS_WARNING, EX_INFO, fmt, ##__VA_ARGS__)
#define H264E_TRACE_DEBUG(fmt, ...)     Trace::trace(H264E, SYS_DEBUG, EX_INFO, fmt, ##__VA_ARGS__)
#define H264E_TRACE_INFO(fmt, ...)      Trace::trace(H264E, SYS_INFORMATION, EX_INFO, fmt, ##__VA_ARGS__)

#define VPP_TRACE_ERROR(fmt, ...)       Trace::trace(VPP, SYS_ERROR, EX_INFO, fmt, ##__VA_ARGS__)
#define VPP_TRACE_WARNI(fmt, ...)       Trace::trace(VPP, SYS_WARNING, EX_INFO, fmt, ##__VA_ARGS__)
#define VPP_TRACE_DEBUG(fmt, ...)       Trace::trace(VPP, SYS_DEBUG, EX_INFO, fmt, ##__VA_ARGS__)
#define VPP_TRACE_INFO(fmt, ...)        Trace::trace(VPP, SYS_INFORMATION, EX_INFO, fmt, ##__VA_ARGS__)

#define MSMT_TRACE_ERROR(fmt, ...)      Trace::trace(MSMT, SYS_ERROR, EX_INFO, fmt, ##__VA_ARGS__)
#define MSMT_TRACE_WARNI(fmt, ...)      Trace::trace(MSMT, SYS_WARNING, EX_INFO, fmt, ##__VA_ARGS__)
#define MSMT_TRACE_DEBUG(fmt, ...)      Trace::trace(MSMT, SYS_DEBUG, EX_INFO, fmt, ##__VA_ARGS__)
#define MSMT_TRACE_INFO(fmt, ...)       Trace::trace(MSMT, SYS_INFORMATION, EX_INFO, fmt, ##__VA_ARGS__)

class Trace
{
public:
    static void start();
    static void stop();
    static void SetTraceLevel(TraceLevel level) { filter.enable_all(level); }

    static void trace(AppModId, TraceLevel, const char *file, int line, const char* fmt, ...);

private:
    static void *worker_thread(void *);

private:
    static const int MAX_DESC_LEN = 2048;
    static bool running_;
    static pthread_t thread;    //only one instance
    static Mutex m_mutex;

    static TraceFilter filter;
    static std::queue<TraceInfo *> queue_;
};
#endif
