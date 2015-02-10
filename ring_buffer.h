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

#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

template<typename Element, unsigned int Size = 16>
class RingBuffer
{
private:
    Element array[Size];
    unsigned int head_;
    unsigned int tail_;

public:
    RingBuffer(): head_(0), tail_(0) {}
    virtual ~RingBuffer() {}

    unsigned int DataCount() {
        if (tail_ >= head_) {
            return (tail_ - head_);
        } else {
            return (tail_ + Size - head_);
        }
    }

    bool Push(Element &item) {
        unsigned int next_tail = (tail_ + 1) % Size;

        if (next_tail != head_) {
            array[tail_] = item;
            tail_ = next_tail;
            return true;
        }

        // queue was full
        return false;
    }

    bool Pop(Element &item) {
        if (head_ == tail_) {
            return false;
        }

        item = array[head_];
        head_ = (head_ + 1) % Size;
        return true;
    }

    // Get the element, no advance
    bool Get(Element &item) {
        if (head_ == tail_) {
            return false;
        }

        item = array[head_];
        return true;
    }

    bool IsFull() {
        return ((tail_ + 1) % Size == head_);
    }

    bool IsEmpty() {
        return (head_ == tail_);
    }
};

#endif  //RINGBUFFER_H_
