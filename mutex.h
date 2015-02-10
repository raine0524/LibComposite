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

#ifndef MUTEX_H_
#define MUTEX_H_

//#define PLATFORM_OS_WINDOWS
#define PLATFORM_OS_LINUX

#if defined(PLATFORM_OS_WINDOWS)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "mutex_locker.h"

class Mutex
{
public:
    Mutex();
    ~Mutex();

   /**
    * lock the mutex
    */
    bool Lock();

   /**
    * unlock the mutex
    */
    bool Unlock();

   /**
    * try lock the mutex
    * @return
    *      true, if lock successed
    */
    bool TryLock();

   /**
    * try get the condition variable in given time
    * @param s
    *      The timeout value in seconds
    * @param ms
    *      The timeout value in milliseconds
    * @return
    *      true, if lock successed
    */
    bool TimedWait(unsigned s, unsigned ms = 0);

   /**
    * try set the condition variable
    * @return
    *      true, if set successed
    */
    bool CondSignal();

#if defined(PLATFORM_OS_WINDOWS)
    HANDLE InnerMutex() { return m_hmutex; }
#else
    pthread_mutex_t* InnerMutex() { return &m_mutex; }
#endif


private:
#if defined(PLATFORM_OS_WINDOWS)
    HANDLE m_hmutex;
    HANDLE m_hcond;
#else
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
#endif

};
#endif  //MUTEX_H_
