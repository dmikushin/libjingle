//  (C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
//  Copyright (c) 2001, 2002 Peter Dimov
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//
//  See http://www.boost.org/libs/smart_ptr/scoped_ptr.htm for documentation.
//

//  scoped_ptr mimics a built-in pointer except that it guarantees deletion
//  of the object pointed to, either on destruction of the scoped_ptr or via
//  an explicit reset(). scoped_ptr is a simple solution for simple needs;
//  use shared_ptr or std::auto_ptr if your needs are more complex.

//  scoped_ptr_malloc added in by Google.  When one of
//  these goes out of scope, instead of doing a delete or delete[], it
//  calls free().  scoped_ptr_malloc<char> is likely to see much more
//  use than any other specializations.

//  release() added in by Google. Use this to conditionally
//  transfer ownership of a heap-allocated object to the caller, usually on
//  method success.
#ifndef TALK_BASE_SCOPED_PTR_H__
#define TALK_BASE_SCOPED_PTR_H__

#include <cstddef>             // for std::ptrdiff_t
#include <stdlib.h>            // for free() decl

#include "base/common.h"  // for ASSERT

#ifdef _WIN32
namespace std { using ::ptrdiff_t; };
#endif // _WIN32

namespace talk_base {

template <typename T>
class scoped_ptr {
 private:

  T* ptr;

  scoped_ptr(scoped_ptr const &);
  scoped_ptr & operator=(scoped_ptr const &);

 public:

  typedef T element_type;

  explicit scoped_ptr(T* p = NULL): ptr(p) {}

  ~scoped_ptr() {
    typedef char type_must_be_complete[sizeof(T)];
    delete ptr;
  }

  void reset(T* p = NULL) {
    typedef char type_must_be_complete[sizeof(T)];

    if (ptr != p) {
      T* obj = ptr;
      ptr = p;
      // Delete last, in case obj destructor indirectly results in ~scoped_ptr
      delete obj;
    }
  }

  T& operator*() const {
    ASSERT(ptr != NULL);
    return *ptr;
  }

  T* operator->() const  {
    ASSERT(ptr != NULL);
    return ptr;
  }

  T* get() const  {
    return ptr;
  }

  void swap(scoped_ptr & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = NULL;
    return tmp;
  }

  T** accept() {
    if (ptr) {
      delete ptr;
      ptr = NULL;
    }
    return &ptr;
  }

  T** use() {
    return &ptr;
  }

  // Allow scoped_ptr<T> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  // Borrowed from chromium's scoped_ptr implementation.
  typedef T* scoped_ptr::*Testable;
  operator Testable() const { return ptr ? &scoped_ptr::ptr : NULL; }

};

template<typename T> inline
void swap(scoped_ptr<T>& a, scoped_ptr<T>& b) {
  a.swap(b);
}




//  scoped_array extends scoped_ptr to arrays. Deletion of the array pointed to
//  is guaranteed, either on destruction of the scoped_array or via an explicit
//  reset(). Use shared_array or std::vector if your needs are more complex.

template<typename T>
class scoped_array {
 private:

  T* ptr;

  scoped_array(scoped_array const &);
  scoped_array & operator=(scoped_array const &);

 public:

  typedef T element_type;

  explicit scoped_array(T* p = NULL) : ptr(p) {}

  ~scoped_array() {
    typedef char type_must_be_complete[sizeof(T)];
    delete[] ptr;
  }

  void reset(T* p = NULL) {
    typedef char type_must_be_complete[sizeof(T)];

    if (ptr != p) {
      T* arr = ptr;
      ptr = p;
      // Delete last, in case arr destructor indirectly results in ~scoped_array
      delete [] arr;
    }
  }

  T& operator[](std::ptrdiff_t i) const {
    ASSERT(ptr != NULL);
    ASSERT(i >= 0);
    return ptr[i];
  }

  T* get() const {
    return ptr;
  }

  void swap(scoped_array & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = NULL;
    return tmp;
  }

  T** accept() {
    if (ptr) {
      delete [] ptr;
      ptr = NULL;
    }
    return &ptr;
  }

  // Allow scoped_array<T> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  // Borrowed from chromium's scoped_array implementation.
  typedef T* scoped_array::*Testable;
  operator Testable() const { return ptr ? &scoped_array::ptr : NULL; }
};

template<class T> inline
void swap(scoped_array<T>& a, scoped_array<T>& b) {
  a.swap(b);
}

// scoped_ptr_malloc<> is similar to scoped_ptr<>, but it accepts a
// second template argument, the function used to free the object.

template<typename T, void (*FF)(T*) = free> class scoped_ptr_malloc {
 private:

  T* ptr;

  scoped_ptr_malloc(scoped_ptr_malloc const &);
  scoped_ptr_malloc & operator=(scoped_ptr_malloc const &);

 public:

  typedef T element_type;

  explicit scoped_ptr_malloc(T* p = 0): ptr(p) {}

  ~scoped_ptr_malloc() {
    FF(ptr);
  }

  void reset(T* p = 0) {
    if (ptr != p) {
      FF(ptr);
      ptr = p;
    }
  }

  T& operator*() const {
    ASSERT(ptr != 0);
    return *ptr;
  }

  T* operator->() const {
    ASSERT(ptr != 0);
    return ptr;
  }

  T* get() const {
    return ptr;
  }

  void swap(scoped_ptr_malloc & b) {
    T* tmp = b.ptr;
    b.ptr = ptr;
    ptr = tmp;
  }

  T* release() {
    T* tmp = ptr;
    ptr = 0;
    return tmp;
  }

  T** accept() {
    if (ptr) {
      FF(ptr);
      ptr = 0;
    }
    return &ptr;
  }

  // Allow scoped_ptr_malloc<T> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  // Borrowed from chromium's scoped_ptr_malloc implementation.
  typedef T* scoped_ptr_malloc::*Testable;
  operator Testable() const { return ptr ? &scoped_ptr_malloc::ptr : NULL; }
};

template<typename T, void (*FF)(T*)> inline
void swap(scoped_ptr_malloc<T,FF>& a, scoped_ptr_malloc<T,FF>& b) {
  a.swap(b);
}

} // namespace talk_base

#endif  // #ifndef TALK_BASE_SCOPED_PTR_H__
