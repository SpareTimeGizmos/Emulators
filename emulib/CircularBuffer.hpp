//++
// CircularBuffer.hpp -> Simple circular buffer template ...
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the emulator library project.  EMULIB is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    EMULIB is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with EMULIB.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This is a simple template implementation of a circular buffer class.
// Yes, I know I'm going to get reamed for not using one of the STL templates
// instead (e.g. deque or, better yet, queue) but I was concerned about the
// overhead involved with those.  This implementation uses a fixed size buffer
// and is implemented using a simple array member.  Get() and Put() simply use
// two array indices which chase each other around in circles.  There's no
// memory allocation overhead at all.
//
//   Note that, although this is technically a container class, it doesn't
// implement the standard STL container class interface.  Sorry about that.
// It's not really necessary for a circular buffer.
//
//   IMPORTANT!  There are no critical section locks in this class and, should
// you be doing gets and puts from different threads, it certainly isn't thread
// safe.  It's up to the parent object to implement a critical section lock of
// some kind or another.
//
// Bob Armstrong <bob@jfcl.com>   [19-JUN-2016]
//
// REVISION HISTORY:
// 19-JUL-16  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
//--
#pragma once

template <typename DATA_TYPE, size_t BUFFER_SIZE> class CCircularBuffer {
  //++
  //--

  // Constructor and destructor ...
public:
  CCircularBuffer() {Clear();}
  virtual ~CCircularBuffer() {}
  // Disallow copy and assignment operations with CCircularBuffer objects...
private:
  CCircularBuffer (const CCircularBuffer &dw) = delete;
  CCircularBuffer& operator= (const CCircularBuffer &dw) = delete;

  // Public properties ...
public:
  // Return the allocated size of the buffer, and the number of items used ...
  size_t Size() const {return BUFFER_SIZE;}
  size_t Count() const {return m_nCount;}
  // Return TRUE if this buffer is empty or full ...
  bool IsEmpty() const {return m_nCount == 0;}
  bool IsFull() const {return m_nCount == BUFFER_SIZE;}

  // Public methods ...
public:
  // Clear the buffer ...
  void Clear() {m_nCount = m_nHead = m_nTail = 0;}
  // Remove the next item from the buffer (return FALSE if empty) ...
  bool Get (DATA_TYPE &v) {
    if (IsEmpty()) return false;
    v = m_aData[m_nTail];  Increment(m_nTail);  --m_nCount;
    return true;
  }
  // Return the next item in the buffer, but DON'T REMOVE IT!
  bool Next (DATA_TYPE &v) {
    if (IsEmpty()) return false;
    v = m_aData[m_nTail];  return true;
  }
  // Add an item to the buffer (return FALSE if full) ...
  bool Put (DATA_TYPE v) {
    if (IsFull()) return false;
    m_aData[m_nHead] = v;  Increment(m_nHead);  ++m_nCount;
    return true;
  }

  // Local methods ...
private:
  // Increment a buffer pointer, with wrap around ...
  void Increment (size_t &p) const {if (++p == BUFFER_SIZE)  p = 0;}

  // Local members ...
protected:
  size_t    m_nCount;             // count of buffer space currently used
  size_t    m_nHead;              // buffer index for Put()
  size_t    m_nTail;              // buffer index for Get()
  DATA_TYPE m_aData[BUFFER_SIZE]; // and the actual buffer data
};
