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
 *\file trace.cpp
 *\brief Implementation for Trace
 */

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "trace.h"
using namespace std;

TraceFilter Trace::filter;
bool Trace::running_ = false;
std::queue<TraceInfo *> Trace::queue_;
Mutex Trace::m_mutex;
pthread_t Trace::thread;

void Trace::start()
{
    running_ = true;
    pthread_create(&thread, NULL, worker_thread, NULL);
    printf("Succeed to create trace thread\n");
}

void Trace::stop()
{
    running_ = false;
    pthread_join(thread, NULL);
    printf("Stop trace thread successfully\n");
}

void Trace::trace(AppModId id, TraceLevel lev, const char *file, int line, const char* fmt, ...)
{
    if (!running_)
        return;

    if (!filter.enable(id, lev, 0))
        return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    char temp[MAX_DESC_LEN] = {0};
    char buffer[MAX_DESC_LEN] = {0};
    if (snprintf(temp, MAX_DESC_LEN - 1, "%s,%d: %s", file, line, fmt) < 0)
        temp[MAX_DESC_LEN - 1] = 0;
    if (vsnprintf(buffer, MAX_DESC_LEN - 1, temp, args) < 0)
        buffer[MAX_DESC_LEN - 1] = 0;
    va_end(args);

    TraceInfo *info = new TraceInfo(id, lev, buffer);
    {
        Locker<Mutex> lock(m_mutex);
        queue_.push(info);
    }
}

void *Trace::worker_thread(void *arg)
{
    TraceInfo *info = NULL;

    while (running_) {
        usleep(2*1000);

        while (!queue_.empty()) {
            Locker<Mutex> lock(m_mutex);
            info = queue_.front();
            TraceFile::instance()->add_trace(info);
            delete info;
            queue_.pop();
            info = NULL;
        }
    }
    return 0;
}
