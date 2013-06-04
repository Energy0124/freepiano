#pragma once

#pragma warning(disable: 4819)

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION         0x0800
#endif

#include <stdio.h>
#include <WinSock2.h>
#include <windows.h>

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(o) if (o) { (o)->Release(); (o) = NULL; }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(o)  if (o) { delete (o); (o) = NULL; }
#endif

#define ARRAY_COUNT(a)  (sizeof(a) / sizeof(a[0]))
#define ARRAY_END(a)    (a + sizeof(a) / sizeof(a[0]))

#define APP_NAME        "FreePiano"
#define APP_VERSION     0x01010000

#define FULLSCREEN      0
#define SCALE_DISPLAY   1

struct thread_lock_t {
  thread_lock_t()     { InitializeCriticalSection(&lock); }
  ~thread_lock_t()    { DeleteCriticalSection(&lock); }
  void enter()        { EnterCriticalSection(&lock); }
  void leave()        { LeaveCriticalSection(&lock); }
  bool tryenter()     { return 0 != TryEnterCriticalSection(&lock); }

  CRITICAL_SECTION lock;
};

struct thread_lock {
  thread_lock(thread_lock_t &lock)
    : lock(lock) {
    lock.enter();
  }

  ~thread_lock() {
    lock.leave();
  }

  thread_lock_t &lock;
};

struct thread_unlock {
  thread_unlock(thread_lock_t &lock)
    : lock(lock) {
    lock.leave();
  }

  ~thread_unlock() {
    lock.enter();
  }

  thread_lock_t &lock;
};