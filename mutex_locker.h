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

#ifndef MUTEX_LOCKER_H_
#define MUTEX_LOCKER_H_

 /**
  * Allows temporarily locking an object (e.g. a Mutex) and automatically
  * releasing the lock when the Locker goes out of scope.
  *
 */
template <typename T>
class Locker {
public:
    Locker(T& lock) : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~Locker()
    {
        m_lock.Unlock();
    }

private:
    T& m_lock;

    //Prevent copying or assignment.
    Locker(const T&);
    void operator= (const T& );
};
#endif  //MUTEX_LOCKER_H_
