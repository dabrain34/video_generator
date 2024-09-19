/* VideoGenerator
 * Copyright (C) 2024 Igalia, S.L.
 *     Author: Stephane Cerveau <scerveau@igalia.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You
 * may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <video_generator.h>

/* ----------------------------------------------------------------------------------- */
/*                          T H R E A D I N G                                          */
/* ----------------------------------------------------------------------------------- */

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN

  DWORD WINAPI thread_wrapper_function(LPVOID param) {
    thread* handle = (thread*)param;
    if (NULL == handle) { return 0; }
    if (NULL == handle->func) { return 0; }
    handle->func(handle->user);
    return 0;
  }

  thread* thread_alloc(thread_function func, void* param) {
    thread* t = (thread*)malloc(sizeof(thread));
    if (NULL == t) { return NULL; }
    if (NULL == func) { return NULL; }
    t->user = param;
    t->handle = CreateThread(NULL, 0, thread_wrapper_function, t, 0, &t->thread_id);
    t->func = func;
    if (NULL == t->handle) {
      free(t);
      t = NULL;
    }
    return t;
  }

  int thread_join(thread* t) {
    if (NULL == t) { return -1; }
    DWORD r = WaitForSingleObject(t->handle, INFINITE);
    if (WAIT_OBJECT_0 == r) { return 0 ; }
    else if (WAIT_ABANDONED == r) { return -2; }
    else if (WAIT_FAILED) { return -3; }
    return 0;
  }

  int mutex_init(mutex* m) {
    if (NULL == m) { return -1; }
    m->handle = CreateMutex(NULL, FALSE, NULL); /* default security, not owned by calling thread, unnamed. */
    if (NULL == m->handle) { return -2; }
    return 0;
  }

  int mutex_destroy(mutex* m) {
    if (NULL == m) { return -1; }
    if (0 == CloseHandle(m->handle)) { return -2; }
    return 0;
  }

  int mutex_lock(mutex* m) {
    if (NULL == m) { return -1; }
    DWORD r = WaitForSingleObject(m->handle, INFINITE);
    if (WAIT_OBJECT_0 == r) { return 0 ; }
    else if (WAIT_ABANDONED == r) { return -2; }
    return 0;
  }

  int mutex_unlock(mutex* m) {
    if (NULL == m) { return -1; }
    if (!ReleaseMutex(m->handle)) { return -2; }
    return 0;
  }

#elif defined(__linux) || defined(__APPLE__)

  void* thread_function_wrapper(void* t) {
    thread* handle = (thread*)t;
    if (NULL == handle) { return NULL; }
    handle->func(handle->user);
    return NULL;
  }

  thread* thread_alloc(thread_function func, void* param) {
    thread* t;
    int r;
    if (NULL == func) { return NULL; }
    t = (thread*)malloc(sizeof(thread));
    if (!t) { return NULL; }
    t->func = func;
    t->user = param;
    r = pthread_create(&t->handle, NULL, thread_function_wrapper, (void*)t);
    if (0 != r) {
      free(t);
      t = NULL;
      return NULL;
    }
    return t;
  }

  int mutex_init(mutex* m) {
    if (NULL == m) { return -1; }
    if (0 != pthread_mutex_init(&m->handle, NULL)) {  return -2;  }
    return 0;
  }

  int mutex_destroy(mutex* m) {
    if (NULL == m) { return -1; }
    if (0 != pthread_mutex_destroy(&m->handle)) { return -2; }
    return 0;
  }

  int mutex_lock(mutex* m) {
    if (NULL == m) { return -1; }
    if (0 != pthread_mutex_lock(&m->handle)) { return -2; }
    return 0;
  }

  int mutex_unlock(mutex* m) {
    if (NULL == m) { return -1;  }
    if (0 != pthread_mutex_unlock(&m->handle)) { return -2;  }
    return 0;
  }

  int thread_join(thread* t) {
    if (NULL == t) { return -1; }
    if (0 != pthread_join(t->handle, NULL)) { return -2; }
    return 0;
  }

#endif /* #elif defined(__linux) or defined(__APPLE__) */

/* ----------------------------------------------------------------------------------- */
/*                          T I M E R                                                  */
/* ----------------------------------------------------------------------------------- */

/*
  Easy embeddable cross-platform high resolution timer function. For each
  platform we select the high resolution timer. You can call the 'ns()'
  function in your file after embedding this.
*/
#include <stdint.h>
#if defined(__linux)
#  define HAVE_POSIX_TIMER
#  include <time.h>
#  ifdef CLOCK_MONOTONIC
#     define CLOCKID CLOCK_MONOTONIC
#  else
#     define CLOCKID CLOCK_REALTIME
#  endif
#elif defined(__APPLE__)
#  define HAVE_MACH_TIMER
#  include <mach/mach_time.h>
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif
static uint64_t ns() {
  static uint64_t is_init = 0;
#if defined(__APPLE__)
  static mach_timebase_info_data_t info;
  if (0 == is_init) {
    mach_timebase_info(&info);
    is_init = 1;
  }
  uint64_t now;
  now = mach_absolute_time();
  now *= info.numer;
  now /= info.denom;
  return now;
#elif defined(__linux)
  static struct timespec linux_rate;
  if (0 == is_init) {
    clock_getres(CLOCKID, &linux_rate);
    is_init = 1;
  }
  uint64_t now;
  struct timespec spec;
  clock_gettime(CLOCKID, &spec);
  now = spec.tv_sec * 1000000000 + spec.tv_nsec;
  return now;
#elif defined(_WIN32)
  static LARGE_INTEGER win_frequency;
  if (0 == is_init) {
    QueryPerformanceFrequency(&win_frequency);
    is_init = 1;
  }
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (uint64_t) ((1e9 * now.QuadPart)  / win_frequency.QuadPart);
#endif
}

/* ----------------------------------------------------------------------------------- */
/*                          V I D E O   G E N E R A T O  R                             */
/* ----------------------------------------------------------------------------------- */

#define CLIP(X) ( (X) > 255 ? 255 : (X) < 0 ? 0 : X)
#define RGB2Y(R, G, B) CLIP(( (  66 * (R) + 129 * (G) +  25 * (B) + 128) >> 8) +  16)
#define RGB2U(R, G, B) CLIP(( ( -38 * (R) -  74 * (G) + 112 * (B) + 128) >> 8) + 128)
#define RGB2V(R, G, B) CLIP(( ( 112 * (R) -  94 * (G) -  18 * (B) + 128) >> 8) + 128)


// Macro to extract the MSB and LSB for Little Endian
#define LSB_LE(val)  ((uint8_t)((val) & 0xFF))         // Extract LSB in Little Endian
#define MSB_LE(val)  ((uint8_t)(((val) >> 8) & 0xFF))  // Extract MSB in Little Endian

// Macro to extract the MSB and LSB for Big Endian
#define LSB_BE(val)  ((uint8_t)(((val) >> 8) & 0xFF))  // Extract LSB in Big Endian
#define MSB_BE(val)  ((uint8_t)((val) & 0xFF))         // Extract MSB in Big Endian

#define MSB(val, be) be ? MSB_BE(val) : MSB_LE(val)
#define LSB(val, be) be ? LSB_BE(val) : LSB_LE(val)

static uint64_t numbersfont_pixel_data[] = {0x0,0x0,0xffffffff0000,0x0,0xffffff0000000000,0xffffffffffff,0x0,0x0,0xffffffffffffff00,0xff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffff000000000000,0xffffffffff,0x0,0xff00000000000000,0xffffffffffff,0x0,0xffff000000000000,0xffffffffffffffff,0xffffffffffffffff,0x0,0xffffffffff000000,0xffffff,0x0,0xffffff0000000000,0xffffffff,0x0,0x0,0xff00ffffff000000,0xffffffff,0x0,0x0,0xffffffffff00,0x0,0xffffffffff000000,0xffffffffffffffff,0x0,0xff00000000000000,0xffffffffffffffff,0xffffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffffffffff000000,0xffffffffffffffff,0x0,0xffffff0000000000,0xffffffffffffffff,0xff,0xffff000000000000,0xffffffffffffffff,0xffffffffffffffff,0x0,0xffffffffffffff00,0xffffffffffff,0x0,0xffffffffff000000,0xffffffffffffff,0x0,0x0,0xff00ffffffff0000,0xffffffff,0x0,0x0,0xffffffffffff,0x0,0xffffffffffffff00,0xffffffffffffffff,0xffff,0xffffff0000000000,0xffffffffffffffff,0xffffffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffffffffffff0000,0xffffffffffffffff,0xff,0xffffffffff000000,0xffffffffffffffff,0xffff,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xff00000000000000,0xffffffffffffffff,0xffffffffffffff,0x0,0xffffffffffff0000,0xffffffffffffffff,0x0,0x0,0xff00ffffffffff00,0xffffffff,0x0,0xff00000000000000,0xffffffffffff,0x0,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffff00000000,0xffffffffffffffff,0xffffffffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffffffffffffff00,0xffffffffffffffff,0xffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffff,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xffff000000000000,0xffffffffffffffff,0xffffffffffffffff,0x0,0xffffffffffffffff,0xffffffffffffffff,0xffff,0x0,0xff00ffffffffffff,0xffffffff,0x0,0xff00000000000000,0xffffffffffff,0xff00000000000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffffffffff000000,0xffffffffffffffff,0xffffffffffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffffffffff00,0xffffffffffffffff,0xffffffff,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xffff000000000000,0xffffffffffffffff,0xffffffffffffffff,0xff,0xffffffffffffffff,0xffffffffffffffff,0xffff,0xff00000000000000,0xff00ffffffffffff,0xffffffff,0x0,0xffff000000000000,0xffffffffffff,0xff00000000000000,0xffffffffffff,0xffffff0000000000,0xffffffffff,0xffffffffffff0000,0xff0000000000ffff,0xffffffffffffff,0x0,0x0,0xff00000000000000,0xffffff,0xffffffffffffff,0xffffffff00000000,0xffffff,0xffffffffffffff00,0xffff000000000000,0xffffffffff,0xffffff0000000000,0xffff,0x0,0xffffff0000000000,0xffffffff,0xffffffffff000000,0xff0000000000ffff,0xffffffffffffff,0xffffff0000000000,0xffffff,0xffff000000000000,0xffffffffffff,0x0,0x0,0xffffff0000000000,0xffffffffffff,0xffff000000000000,0xffffffffff,0xff00000000000000,0xffffffffff,0xffffffffffff0000,0x0,0xffffffffffff00,0x0,0x0,0xffff000000000000,0xff0000000000ffff,0xffffffffff,0xffff000000000000,0xffffffff,0xffffffffffff,0xff00000000000000,0xffffffffff,0xffffff0000000000,0xffff,0x0,0xffffff0000000000,0xffffff,0xffffffff00000000,0xffff00000000ffff,0xffffffffff,0xff00000000000000,0xffffffff,0xffffff0000000000,0xffffffffffff,0x0,0x0,0xffffff0000000000,0xffffffffffff,0xffff000000000000,0xffffff,0x0,0xffffffffffff,0xffffffffffff00,0x0,0xffffffffff0000,0x0,0x0,0xffffff0000000000,0xff000000000000ff,0xffffffff,0xff00000000000000,0xffffffff,0xffffffffff,0x0,0xffffffffffff,0xffffffff00000000,0xffff,0x0,0xffffffff00000000,0xffff,0xffffff0000000000,0xffff000000ffffff,0xffffffff,0xff00000000000000,0xffffffff,0xffffffffff000000,0xffffffffffff,0x0,0x0,0xffffffff00000000,0xffffffffffff,0xffffff0000000000,0xffffff,0x0,0xffffffffffff,0xffffffffff00,0x0,0xffffffffffff0000,0x0,0x0,0xffffffff00000000,0xffff000000000000,0xffffffff,0xff00000000000000,0xff0000ffffffffff,0xffffffffff,0x0,0xffffffffff00,0xffffffff00000000,0xff,0x0,0xffffffff00000000,0xff,0xffff000000000000,0xffff000000ffffff,0xffffff,0x0,0xffffffffff,0xffffffffffff0000,0xffffffffffff,0x0,0x0,0xffffffffff000000,0xffffffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0xffffffffffff,0x0,0xffffffffff000000,0x0,0x0,0xffffffffff000000,0xffff000000000000,0xffffff,0x0,0xff0000ffffffffff,0xffffffff,0x0,0xffffffffff00,0xffffffff00000000,0xff,0x0,0xffffffff00000000,0xff,0xffff000000000000,0xffffff0000ffffff,0xffffff,0x0,0xffffffff00,0xffffffffffffffff,0xffffffffff00,0x0,0x0,0xffffffffffff0000,0xffffffffff00,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0xffffffffff,0x0,0x0,0x0,0x0,0xffffffffff0000,0xffff000000000000,0xffffff,0x0,0xffffffffff,0xffffff00,0x0,0xffffffffff00,0xffffffff00000000,0xff,0x0,0xffffffff00000000,0xff,0xffff000000000000,0xffffff0000ffffff,0xffff,0x0,0xff0000ffffffff00,0xffffffffffffff,0xffffffffff00,0x0,0x0,0xffffffffffff0000,0xffffffffff00,0xff00000000000000,0xffff,0x0,0xffffffffff00,0xffffffffff,0x0,0x0,0x0,0x0,0xffffffff0000,0xffff000000000000,0xffffff,0x0,0xffffffffff,0x0,0x0,0xffffffffff00,0xffffffff00000000,0xff,0x0,0xffffffff00000000,0xff,0xffff000000000000,0xffffff0000ffffff,0xffff,0x0,0xff0000ffffffff00,0xffffffffff,0xffffffffff00,0x0,0x0,0xffffffffffff00,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffffffff,0x0,0x0,0x0,0x0,0xffffffffff00,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffff,0xffffffffff000000,0xff,0x0,0xffffffff00000000,0xffff,0xffffff0000000000,0xffffff0000ffffff,0xffff,0x0,0xff00ffffffffff00,0xffffff,0xffffffffff00,0x0,0x0,0xffffffffffff,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffffff,0x0,0x0,0x0,0x0,0xffffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xff00000000000000,0xffffffffff,0xffffffffff000000,0xffffffff000000ff,0xffff,0xffffff0000000000,0xffffff,0xffffff0000000000,0xffffff000000ffff,0xffff,0x0,0xff00ffffffffff00,0xff,0xffffffffff00,0x0,0x0,0xffffffffff,0xffffffffff00,0x0,0x0,0x0,0xff0000ffffffffff,0xffffffff,0xffffffffffff0000,0xff,0x0,0x0,0xffffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffff0000000000,0xffffffff,0xffffffffff000000,0xffffffffffff0000,0xffffffff,0xffff000000000000,0xffffffff,0xffffffffff000000,0xffffff00000000ff,0xffff,0x0,0xffffffffff00,0x0,0xffffffffff00,0x0,0xff00000000000000,0xffffffffff,0xffffffffff00,0x0,0x0,0x0,0xff0000ffffffffff,0xffffffff,0xffffffffffffffff,0xffffff,0x0,0xff00000000000000,0xffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffffffffff00,0xffffff,0xffffffffff000000,0xffffffffffffff00,0xffffffffffff,0xff00000000000000,0xffffffffffffffff,0xffffffffffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffffff00,0x0,0xffff000000000000,0xffffffff,0xffffffffff00,0x0,0x0,0xff00000000000000,0xff0000ffffffffff,0xffff0000ffffffff,0xffffffffffffffff,0xffffffffff,0x0,0xff00000000000000,0xffffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffffffffff00,0xff,0xffffffffff000000,0xffffffffffffffff,0xffffffffffffff,0x0,0xffffffffffffffff,0xffffffffffffff,0xffffff0000000000,0xffffff,0x0,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffffff0000000000,0xffffff,0xffffffffff00,0x0,0x0,0xffff000000000000,0xff000000ffffffff,0xffffff00ffffffff,0xffffffffffffffff,0xffffffffffff,0x0,0xffff000000000000,0xffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffffffffff00,0xffffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffffffffffff,0x0,0xffffffffffffff00,0xffffffffffff,0xffff000000000000,0xffffff,0xff00000000000000,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffffff0000000000,0xffffff,0xffffffffff00,0x0,0x0,0xffffff0000000000,0xff00000000ffffff,0xffffff00ffffffff,0xffffffffffffffff,0xffffffffffffff,0x0,0xffff000000000000,0xffffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffffffffff00,0xffffffff,0xffffffffffff0000,0xffff,0xffffffffffffff00,0xff00000000000000,0xffffffffffffffff,0xffffffffffffffff,0xffff000000000000,0xffffffff,0xffff000000000000,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffffffff00000000,0xffff,0xffffffffff00,0x0,0x0,0xffffffff00000000,0xff0000000000ffff,0xffffffffffffffff,0xff,0xffffffffffffff,0x0,0xffffff0000000000,0xffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0xffffffff00ffff00,0xffffffffff,0xffffffffffff0000,0x0,0xffffffffff000000,0xffff0000000000ff,0xffffffffffffffff,0xffffffffffffffff,0xff000000000000ff,0xffffffffffff,0xffffff0000000000,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffffffffff000000,0xff,0xffffffffff00,0x0,0x0,0xffffffffff000000,0xff0000000000ffff,0xffffffffffffff,0x0,0xffffffffffffff00,0x0,0xffffff0000000000,0xffff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffffff,0xffffff00000000,0x0,0xffffffff00000000,0xffffff00000000ff,0xffffffff,0xffffffffff000000,0xff0000000000ffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffffffffff000000,0x0,0xffffffffff00,0x0,0x0,0xffffffffffff0000,0xff000000000000ff,0xffffffffffff,0x0,0xffffffffffff0000,0x0,0xffffffff00000000,0xff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffffff00,0x0,0x0,0xffffffff00000000,0xffffffff0000ffff,0xffff,0xffffff0000000000,0xffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffff00,0x0,0xffffffffff00,0x0,0xffffffffffff0000,0x0,0xffffffffff00,0x0,0x0,0xffffffffffffff00,0xff00000000000000,0xffffffffff,0x0,0xffffffffff000000,0xff,0xffffffff00000000,0xff,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffff0000,0x0,0x0,0xffffff0000000000,0xffffffff0000ffff,0xff,0xffff000000000000,0xffffff,0xffffffffffffff00,0xffffffffffffff,0xffffffffff00,0x0,0xffffffffff00,0x0,0xffffffffffff00,0x0,0xffffffffff00,0x0,0xff00000000000000,0xffffffffffff,0xff00000000000000,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff000000,0x0,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffffff0000,0x0,0x0,0xffffff0000000000,0xffffffff0000ffff,0xff,0xffff000000000000,0xffffff,0xffffffffff000000,0xffffffffff,0xffffffffff00,0x0,0xffffffffff00,0x0,0xffffffffffff,0x0,0xffffffffff00,0x0,0xffff000000000000,0xffffffffff,0xff00000000000000,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff000000,0x0,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffff000000,0x0,0x0,0xffffff0000000000,0xffffffffff00ffff,0x0,0xff00000000000000,0xffffffff,0xffffff0000000000,0xffffff,0xffffffffff00,0x0,0xff00ffffffffff00,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffff0000000000,0xffffffff,0xff00000000000000,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff000000,0x0,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffff000000,0x0,0x0,0xffffff0000000000,0xffffffffff00ffff,0x0,0xff00000000000000,0xffffffff,0x0,0x0,0xffffffffff00,0x0,0xff00ffffffffff00,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffff00000000,0xffffff,0xff00000000000000,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff0000,0x0,0xffffff0000000000,0xffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffff000000,0x0,0x0,0xffffff0000000000,0xffffffffff00ffff,0x0,0xff00000000000000,0xffffffff,0x0,0x0,0xffffffffff,0x0,0xff00ffffffffff00,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffffffff0000,0xffff,0x0,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff0000,0x0,0xffff000000000000,0xffffff,0x0,0xffffffffff,0xffffff00,0x0,0xffffffffff000000,0x0,0x0,0xffffff0000000000,0xffffffffff00ffff,0x0,0xff00000000000000,0xffffffff,0x0,0x0,0xffffffffff,0x0,0xff00ffffffffff00,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffffffffff00,0xff,0x0,0xffffffff,0x0,0xffffffff00000000,0xff,0xffffffffff0000,0x0,0xffff000000000000,0xffffff,0x0,0xff0000ffffffffff,0xffffffff,0x0,0xffffffffff000000,0xffffffffff00,0x0,0xffffff0000000000,0xffffffffff00ffff,0x0,0xff00000000000000,0xffffffff,0x0,0x0,0xffffffffff,0x0,0xff00ffffffffff00,0xffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffffffffff,0x0,0x0,0xffffffffff,0x0,0xffffffffff000000,0xff,0xffffffffff0000,0x0,0xffff000000000000,0xffffff,0x0,0xff0000ffffffffff,0xffffffffff,0x0,0xffffffffffff0000,0xffffffffff00,0x0,0xffffffff00000000,0xffffffffff0000ff,0x0,0xff00000000000000,0xffff0000ffffffff,0xffffff,0xff00000000000000,0xffffffffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xff00000000000000,0xffffffffffff,0x0,0x0,0xffffffffff,0x0,0xffffffffff000000,0x0,0xffffffffff00,0x0,0xffff000000000000,0xffffffff,0xff00000000000000,0xff0000ffffffffff,0xffffffffff,0x0,0xffffffffff0000,0xffffffffffff00,0x0,0xffffffff00000000,0xffffffffff0000ff,0xff,0xffff000000000000,0xffff0000ffffffff,0xffffff,0xff00000000000000,0xffffffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xff00000000000000,0xffffffffff,0x0,0x0,0xffffffffff00,0x0,0xffffffffffff0000,0x0,0xffffffffff00,0x0,0xff00000000000000,0xffffffff,0xff00000000000000,0xffffffff,0xffffffffffff,0x0,0xffffffffffff00,0xffffffffff0000,0x0,0xffffffffff000000,0xffffffff000000ff,0xff,0xffff000000000000,0xffff000000ffffff,0xffffffff,0xff00000000000000,0xffffffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffff000000000000,0xffffffff,0x0,0x0,0xffffffffffff00,0x0,0xffffffffffffff00,0x0,0xffffffffff00,0x0,0xff00000000000000,0xffffffffff,0xffff000000000000,0xffffffff,0xffffffffffffff,0x0,0xffffffffffffff,0xffffffffffff0000,0x0,0xffffffffffff0000,0xffffffff00000000,0xffff,0xffffff0000000000,0xff00000000ffffff,0xffffffffff,0xffffff0000000000,0xffffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffff0000000000,0xffffff,0x0,0x0,0xffffffffffff0000,0xff,0xffffffffffffff,0x0,0xffffffffff00,0x0,0x0,0xffffffffffffff,0xffffffff00000000,0xffffff,0xffffffffffffff00,0xff00000000000000,0xffffffffffff,0xffffffffff000000,0xffff,0xffffffffffffffff,0xffffff0000000000,0xffffffffff,0xffffffffff000000,0xff0000000000ffff,0xffffffffffff,0xffffffff00000000,0xffffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0xffffffffff000000,0xffffffffffffffff,0xffffffffffff,0x0,0xffffffffff,0x0,0x0,0xffffffffffffffff,0xffffffffffffffff,0xffffff,0xffffffffffffff00,0xffffffffffffffff,0xffffffffff,0xffffffffff000000,0xffffffffffffffff,0xffffffffffffff,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xffff,0xffffffffffffffff,0xffffffffffffffff,0xffff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffff0000000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0xffffffff00000000,0xffffffffffffffff,0xffffffffff,0x0,0xffffffffff,0x0,0x0,0xffffffffffffff00,0xffffffffffffffff,0xffff,0xffffffffffff0000,0xffffffffffffffff,0xffffffff,0xffffffff00000000,0xffffffffffffffff,0xffffffffffff,0xffff000000000000,0xffffffffffffffff,0xffffffffffffffff,0xff,0xffffffffffffff00,0xffffffffffffffff,0xff,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffffff00000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0xffffff0000000000,0xffffffffffffffff,0xffffffff,0x0,0xffffffffff,0x0,0x0,0xffffffffffff0000,0xffffffffffffffff,0xff,0xffffffffff000000,0xffffffffffffffff,0xffffff,0xffffff0000000000,0xffffffffffffffff,0xffffffffff,0xff00000000000000,0xffffffffffffffff,0xffffffffffffffff,0x0,0xffffffffffffff00,0xffffffffffffffff,0x0,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffffff00000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0xffff000000000000,0xffffffffffffffff,0xffffff,0x0,0xffffffffff,0x0,0x0,0xffffffffff000000,0xffffffffffffffff,0x0,0xffffff0000000000,0xffffffffffffffff,0xffff,0xff00000000000000,0xffffffffffffffff,0xffffff,0x0,0xffffffffffffff00,0xffffffffffff,0x0,0xffffffffff000000,0xffffffffffffff,0x0,0x0,0xffffffffff00,0x0,0x0,0x0,0xffffffffff00,0xffffffff00000000,0xffffffffffffffff,0xffffffffffffffff,0xffffffffffff,0x0,0xffffffffffffff00,0x0,0x0,0xffffffffff,0x0,0x0,0xffff000000000000,0xffffffffff,0x0,0xff00000000000000,0xffffffffffffff,0x0,0x0,0xffffffffffffff00,0xff,0x0,0xffffffff00000000,0xffffff,0x0,0xffffff0000000000,0xffffffff,0x0,0x0,0xffffffffff00,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
static int numbersfont_char_data[] = {48,109,0,25,39,3,12,31,49,239,0,15,39,6,12,31,50,28,0,26,39,2,12,31,51,135,0,25,39,3,12,31,52,0,0,27,39,1,12,31,53,161,0,25,39,3,12,31,54,55,0,26,39,2,12,31,55,82,0,26,39,2,12,31,56,187,0,25,39,3,12,31,57,213,0,25,39,3,12,31,58,255,0,5,29,5,22,15};
static int fill(video_generator* gen, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b);
static int add_number_string(video_generator* gen, const char* str, int x, int y);
static int add_char(video_generator* gen, video_generator_char* kar, int x, int y);
static void* audio_thread(void* gen); /* When we need to generate audio, we do this in another thread. So be aware that the callback will be called from this thread! */

#define DEFAULT_WIDTH     640
#define DEFAULT_HEIGHT    480
#define DEFAULT_FPS       3
#define DEFAULT_FORMAT    420
#define DEFAULT_BITDEPTH  8
#define DEFAULT_BYTE_ORDER BYTE_ORDER_LITTLE_ENDIAN

void select_yuv_format (video_generator* g, video_generator_settings* cfg) {
  switch (cfg->format) {
    case 400:
      g->u_factor = 0.0;
      g->v_factor = 0.0;
      break;
    case 444:
      g->u_factor = 1.0;
      g->v_factor = 1.0;
      break;
    case 422:
      g->u_factor = 0.5;
      g->v_factor = 1.0;
      break;
    case 420:
    default:
      g->u_factor = 0.5;
      g->v_factor = 0.5;
  }
}

void select_bitdepth (video_generator* g, video_generator_settings* cfg) {
  switch (cfg->bitdepth) {
    case 10:
      g->pixel_size_in_bytes = 2;
      g->pixel_factor = 4;
      break;
    case 12:
      g->pixel_size_in_bytes = 2;
      g->pixel_factor = 16;
      break;
    case 8:
    default:
     g->pixel_size_in_bytes = 1;
     g->pixel_factor = 1;
  }
}

int video_generator_init(video_generator_settings* cfg, video_generator* g) {

  uint32_t i = 0;
  int dx = 0;
  uint32_t max_els = RXS_MAX_CHARS * 8; /* members per char */
  video_generator_char* c = NULL;
  uint32_t num_frames; /* used for bip/bop calculations. */

  if (!g) { return -1; }
  if (!cfg) { return -2; }
  if (!cfg->width) { cfg->width = DEFAULT_WIDTH; }
  if (!cfg->height) { cfg->height = DEFAULT_HEIGHT; }
  if (!cfg->fps) { cfg->fps = DEFAULT_FPS; }
  if (!cfg->format) { cfg->format = DEFAULT_FORMAT; }
  if (!cfg->bitdepth) { cfg->bitdepth = DEFAULT_BITDEPTH; }
  if (!cfg->byte_order) { cfg->byte_order = DEFAULT_BYTE_ORDER; }

  /* initalize members */
  g->frame = 0;
  select_yuv_format(g, cfg);
  select_bitdepth(g, cfg);
  g->ybytes = cfg->width * cfg->height * g->pixel_size_in_bytes;
  g->ubytes = (uint32_t)(cfg->width * g->u_factor) * (uint32_t)(cfg->height * g->v_factor) * g->pixel_size_in_bytes;
  g->vbytes = g->ubytes;
  g->nbytes = g->ybytes + g->ubytes + g->vbytes;

  g->width = cfg->width;
  g->height = cfg->height;
  g->fps = (1.0 / cfg->fps) * 1000 * 1000;

  g->y = (uint8_t*)malloc(g->nbytes);
  g->u = g->y + g->ybytes;
  g->v = g->u + g->ubytes;

  g->step = (1.0 / (5 * cfg->fps)); /* move the bar in 5 seconds from top to bottom */
  g->perc = 0.0;
  g->fps_num = 1;
  g->fps_den = cfg->fps;
  g->byte_order = cfg->byte_order;

  /* initialize the characters */
  while (i < max_els) {
    c = &g->chars[dx];
    c->id = numbersfont_char_data[i++];
    c->x = numbersfont_char_data[i++];
    c->y = numbersfont_char_data[i++];
    c->width = numbersfont_char_data[i++];
    c->height = numbersfont_char_data[i++];
    c->xoffset = numbersfont_char_data[i++];
    c->yoffset = numbersfont_char_data[i++];
    c->xadvance = numbersfont_char_data[i++];
    dx++;
  }

  /* bitmap font specifics */
  g->font_w = 264;
  g->font_h = 50;
  g->font_line_height = 63;

  /* default audio settings. */
  g->audio_bip_frequency = 0;
  g->audio_bop_frequency = 0;
  g->audio_nchannels = 0;
  g->audio_samplerate = 0;
  g->audio_nbytes = 0;
  g->audio_buffer = NULL;
  g->audio_callback = NULL;
  g->audio_thread = NULL;

  /* initialize audio */
  if (NULL != cfg->audio_callback) {

    if (0 == cfg->bip_frequency) {
      printf("Error: audio_callback set but no bip_frequency set. Use e.g. 500.");
      return -6;
    }

    if (0 == cfg->bop_frequency) {
      printf("Error: audio_callback set but no bop_frequency set. Use e.g. 1500.");
      return -7;
    }

    /* we allocate a buffer up to 4 seconds. */
    g->audio_bip_frequency = cfg->bip_frequency;
    g->audio_bop_frequency = cfg->bop_frequency;
    g->audio_bip_millis = 100;
    g->audio_bop_millis = 100;
    g->audio_nchannels = 2;
    g->audio_samplerate = 44100;
    g->audio_nsamples = 1024;
    g->audio_nseconds = 4;
    g->audio_nbytes = sizeof(int16_t) * g->audio_samplerate * g->audio_nchannels * g->audio_nseconds;
    g->audio_callback = cfg->audio_callback;

    /* alloc the buffer. */
    g->audio_buffer = (int16_t*)malloc(g->audio_nbytes);
    if (!g->audio_buffer) {
      printf("Error while allocating the audio buffer.");
      g->audio_buffer = NULL;
      return -7;
    }

    /* fill with silence */
    memset((uint8_t*)g->audio_buffer, 0x00, g->audio_nbytes);

    /* bip */
    dx= 0;
    num_frames = (uint32_t)(g->audio_bip_millis/1000.0) * g->audio_samplerate;
    for (i = g->audio_samplerate;  i < (g->audio_samplerate + num_frames); ++i) {
      dx = i * 2;
      g->audio_buffer[dx + 0] =  (int16_t)(10000 * sin( (6.28318530718/g->audio_samplerate) * g->audio_bip_frequency * i));
      g->audio_buffer[dx + 1] = g->audio_buffer[dx + 0];
    }

    /* bop */
    num_frames = (uint32_t)(g->audio_bip_millis/1000.0) * g->audio_samplerate;
    for (i = (g->audio_samplerate * 3); i < (g->audio_samplerate * 3 + num_frames); ++i) {
      dx = i * 2;
      g->audio_buffer[dx + 0] = (int16_t)(10000 * sin( (6.28318530718/g->audio_samplerate) * g->audio_bop_frequency * i));
      g->audio_buffer[dx + 1] = g->audio_buffer[dx + 0];
    }

    /* init mutex. */
    if (0 != mutex_init(&g->audio_mutex)) {
      printf("Error: cannot initialize the audio mutex!");
      free(g->audio_buffer);
      g->audio_buffer = NULL;
      return -8;
    }

    /* start audio thread. */
    g->audio_thread = thread_alloc(audio_thread, (void*)g);
    if (NULL == g->audio_thread) {
      printf("Error: cannot create audio thread.\n");
      free(g->audio_buffer);
      g->audio_buffer = NULL;
      return -9;
    }
  }

  return 0;
}

int video_generator_clear(video_generator* g) {

  /* stop the audio thread if it's running. */
  if (NULL != g->audio_thread) {
    mutex_lock(&g->audio_mutex);
      g->audio_thread_must_stop = 1;
    mutex_unlock(&g->audio_mutex);
    thread_join(g->audio_thread);
    g->audio_thread = NULL;

    /* free the audio buffer. */
    if (NULL != g->audio_buffer) {
      free(g->audio_buffer);
      g->audio_buffer = NULL;
    }
  }

  if (!g) { return -1; }
  if (!g->width) { return -2; }
  if (!g->height) { return -3; }

  if (g->y) {
    free(g->y);
  }

  g->y = NULL;
  g->u = NULL;
  g->u = NULL;
  g->width = 0;
  g->height = 0;
  g->frame = 0;
  g->step = 0.0;
  g->perc = 0.0;
  g->fps = 0.0;
  g->ybytes = 0;
  g->ubytes = 0;
  g->vbytes = 0;
  g->nbytes = 0;
  g->u_factor = 0.0;
  g->v_factor = 0.0;

  g->audio_nchannels = 0;
  g->audio_nseconds = 0;
  g->audio_samplerate = 0;
  g->audio_bip_frequency = 0;
  g->audio_bop_frequency = 0;
  g->audio_bip_millis = 0;
  g->audio_bop_millis = 0;
  g->audio_nbytes = 0;
  g->audio_callback = NULL;

  return 0;
}

/* generates a new frame and stores it in the y, u and v members */
int video_generator_update(video_generator* g) {

  uint8_t is_bip, is_bop;
  uint32_t text_w, text_h, text_x, text_y;
  int32_t bar_h, start_y, nlines, h;
  uint64_t minutes, seconds;
  uint32_t stride, end_y, i, k;
  char timebuf[512] = { 0 } ;
  uint8_t text_r, text_g, text_b;
  uint8_t rc, gc, bc;
  int dx;
  uint16_t yc, uc, vc;
  uint8_t colors[] = {
    255, 255, 255,  // white
    255, 255, 0,    // yellow
    0,   255, 255,  // cyan
    0,   255, 0,    // green
    255, 0,   255,  // magenta
    255, 0,   0,    // red
    0,   0,   255   // blue
  };

  if (!g) { return -1; }
  if (!g->width) { return -2; }
  if (!g->height) { return -3; }

  text_r = 0;
  text_g = 0;
  text_b = 0;

  h = g->height - 1;
  bar_h = g->height / 5;
  start_y = -bar_h +  (int32_t)(g->perc * (h + bar_h));

  /* how many lines of the bar are visible */
  if (start_y < 0) {
    nlines = bar_h + start_y;
    start_y = 0;
  }
  else if(start_y + bar_h > h) {
    nlines = h - start_y;
  }
  else {
    nlines = bar_h;
  }

  /* increment step */
  g->perc += g->step;
  if (g->perc >= (1.0)) {
    g->perc = 0.0;
  }

  if (nlines + start_y >  (int32_t)g->height || nlines < 0 || start_y < 0 || start_y >=  (int32_t)g->height) {
    printf("Error: this shouldn't happen.. writing outside the buffer: %d, %d, %d\n", nlines, (nlines + start_y), start_y);
    return -1;
  }

  /* reset */
  memset(g->y, 0x00, g->ybytes);
  memset(g->u, 0x00, g->ubytes);
  memset(g->v, 0x00, g->vbytes);

  /* Draw the background with 7 colors */
  for (i = 0; i < 7; ++i) {
    dx = i * 3;
    rc = colors[dx + 0];
    gc = colors[dx + 1];
    bc = colors[dx + 2];
    fill(g, i * (g->width / 7), 0, (g->width / 7), g->height, rc, gc, bc);
  }

  /* Draw the moving bar */
  rc = 255 - (uint8_t)(g->perc * 255);
  gc = 30 + (uint8_t)(g->perc * 235);
  bc = 150 + (uint8_t)(g->perc * 205);
  yc = (uint16_t)(RGB2Y(rc, gc, bc) * g->pixel_factor);
  uc = (uint16_t)(RGB2U(rc, gc, bc) * g->pixel_factor);
  vc = (uint16_t)(RGB2V(rc, gc, bc) * g->pixel_factor);

  /* fill y channel */
  stride = (int32_t)(g->width * g->pixel_size_in_bytes);
  for (i = start_y; i < (uint32_t)(start_y + nlines); ++i) {
    if(g->pixel_size_in_bytes == 2) {
      for(k = 0; k < stride; k+=2) {
        g->y[i * stride + k] = LSB(yc, g->byte_order);
        g->y[i * stride + k + 1] = MSB(yc, g->byte_order);
      }
    } else {
      memset(g->y + (i * g->width), yc, g->width);
    }
  }

  /* fill u and v channel */
  start_y = (int32_t)(start_y * g->u_factor);
  stride = (int32_t)(g->width * g->v_factor * g->pixel_size_in_bytes);
  end_y = start_y + (int32_t)(nlines * g->u_factor);
  for (i = start_y; i < end_y; ++i) {
    if(g->pixel_size_in_bytes == 2) {
      for(k = 0; k < stride; k++) {
        g->u[i * stride + k] = LSB(uc, g->byte_order);;
        g->u[i * stride + k + 1] = MSB(uc, g->byte_order);
        g->v[i * stride + k] = LSB(vc, g->byte_order);
        g->v[i * stride + k + 1] = MSB(vc, g->byte_order);;
      }
    } else {
      memset(g->u + i * stride, (uint8_t)uc, stride);
      memset(g->v + i * stride, (uint8_t)vc, stride);
    }
  }

  /* draw blip/blop visuals. */
  if (NULL != g->audio_buffer) {
    mutex_lock(&g->audio_mutex);
    {
      is_bop = g->audio_is_bop;
      is_bip = g->audio_is_bip;

    }
    mutex_unlock(&g->audio_mutex);

    if (is_bip == 1) {
      text_r = 0;
      text_g = 0;
      text_b = 255;
    }
    if (is_bop == 1) {
      text_r = 255;
      text_g = 0;
      text_b = 0;
    }
  }

  /* Draw the text box with time stamps */
  seconds = (g->frame/ g->fps_den);
  minutes = (seconds / 60);
  minutes %= 60;
  seconds %= 60;
  text_w = 170; /* manually measured */
  text_h = 100;
  if (g->width > text_w && g->height > text_h) {
    if(g->width > text_w)
      text_x = (g->width / 2) - (text_w / 2);
    else
      text_x = 0;
    if((g->height) > text_h)
      text_y = (g->height / 2) - (text_h / 2);
    else
      text_y = 0;

    fill(g, text_x, text_y, text_w, text_h, text_r, text_g, text_b);

    sprintf(timebuf, "%02zu:%02zu", minutes, seconds);
    stride = (text_x + 20) * g->pixel_size_in_bytes;
    add_number_string(g, timebuf, stride , text_y + 20);
  }
  g->frame++;
  return 0;
}

static int fill(video_generator* gen, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b) {

  // Y
  uint16_t yc = (uint16_t)(RGB2Y(r,g,b) * gen->pixel_factor);
  uint16_t uc = (uint16_t)(RGB2U(r,g,b) * gen->pixel_factor);
  uint16_t vc = (uint16_t)(RGB2V(r, g, b) * gen->pixel_factor);
  uint32_t j, k, stride;

  // UV
  uint32_t xx = (uint32_t)(x * gen->u_factor);
  uint32_t yy = (uint32_t)(y * gen->v_factor);
  uint32_t half_w = (uint32_t)(gen->width * gen->u_factor);
  uint32_t hh = (uint32_t)(h * gen->v_factor);
  uint32_t ww = (uint32_t)(w * gen->u_factor);

  // y
  for (j = y; j < (uint32_t)(y + h); ++j) {
    stride = (j * gen->width + x) * gen->pixel_size_in_bytes;
    if(gen->pixel_size_in_bytes == 2) {
      for(k = 0; k < w * gen->pixel_size_in_bytes; k+=gen->pixel_size_in_bytes) {
        gen->y[stride + k] = LSB(yc, gen->byte_order);
        gen->y[stride + k + 1 ] = MSB(yc, gen->byte_order);
      }
    } else {
      memset(gen->y + j * gen->width + x, (uint8_t)yc,  w);
    }
  }

  // u and v
  for (j = yy; j < (yy + hh); ++j) {
    stride = (j * half_w + xx) * gen->pixel_size_in_bytes;
    if (gen->pixel_size_in_bytes == 2) {
      for(k = 0; k < ww * gen->pixel_size_in_bytes; k += gen->pixel_size_in_bytes) {
         gen->u[stride + k] = LSB(uc, gen->byte_order);
         gen->u[stride + k + 1] = MSB(uc, gen->byte_order);
         gen->v[stride + k] = LSB(vc, gen->byte_order);
         gen->v[stride + k + 1] = MSB(vc, gen->byte_order);
      }
    } else {
      memset(gen->u + stride,  (uint8_t)uc, (ww));
      memset(gen->v + stride,  (uint8_t)vc, (ww));
    }
  }

  return 0;
}

static int add_number_string(video_generator* gen, const char* str, int x, int y) {

 video_generator_char* found_char = NULL;
 size_t len = strlen(str);
 size_t i = 0;
 int k = 0;

 for (i = 0; i < len; ++i) {

   found_char = NULL;

   for (k = 0; k < RXS_MAX_CHARS; ++k) {
     if (gen->chars[k].id == str[i]) {
       found_char = &gen->chars[k];
       break;
     }
   }

   if (!found_char) {
     printf("Error: Cannot find character: %c\n", str[i]);
     continue;
   }

   add_char(gen, found_char, x, y);
   x += found_char->xadvance;
 }

 return 0;
}

static int add_char(video_generator* gen, video_generator_char* kar, int x, int y) {
  int i = 0;
  int j = 0;
  int dest_x = 0;
  int dest_y = 0;
  int src_dx = 0;
  int dest_dx = 0;

  uint8_t* pixels = (uint8_t*)numbersfont_pixel_data;

  if (!kar) { return -1; }
  if (!gen) { return -2; }

  for (i = kar->x, dest_x = x; i < (kar->x + kar->width); ++i, ++dest_x) {
    for (j = kar->y, dest_y = y; j < (kar->y + kar->height); ++j, ++dest_y) {
      src_dx = j * gen->font_w + i;
      dest_dx = (kar->yoffset + dest_y) * gen->width * gen->pixel_size_in_bytes + dest_x;
      gen->y[dest_dx] = pixels[src_dx];
    }
  }

  return 0;
}

/* ----------------------------------------------------------------------------------- */
/*                          A U D I O  G E N E R A T O R                               */
/* ----------------------------------------------------------------------------------- */

static void* audio_thread(void* gen) {
  video_generator* g;
  uint8_t must_stop;
  uint64_t now, delay, timeout, dx, bip_start_dx, bip_end_dx, bop_start_dx, bop_end_dx;
  uint64_t nbytes = 0;
  uint8_t* tmp_buffer = NULL;
  uint8_t* audio_buffer = NULL;
  uint64_t bytes_to_end = 0;
  uint64_t bytes_from_start = 0;
  uint64_t bytes_needed = 0;
  uint64_t bytes_total = 0;
  uint8_t is_bip = 0;
  uint8_t is_bop = 0;
  uint8_t prev_is_bip = 0;
  uint8_t prev_is_bop = 0;
  uint16_t num_bip_frames = 0;
  uint64_t num_bip_bytes = 0;
  uint16_t num_bop_frames = 0;
  uint64_t num_bop_bytes = 0;


  /* get the handle. */
  must_stop = 0;
  g = (video_generator*)gen;
  if (NULL == g) {
    printf("Not supposed to happen but the audio thread cannot get a handle to the generator.\n");
    exit(1);
  }

  /* init */
  now = 0;
  timeout = 0;
  dx = 0;
  delay = (uint64_t)(g->audio_nsamples * ((double)1.0/g->audio_samplerate) * 1e9);
  nbytes = g->audio_nsamples * sizeof(int16_t) * g->audio_nchannels;
  tmp_buffer = (uint8_t*)malloc(nbytes);
  audio_buffer = (uint8_t*)g->audio_buffer;
  bytes_needed = nbytes;
  bytes_total = g->audio_nbytes;

  /* calc the bip/bop start and end positions. */
  num_bip_frames =  (uint16_t)(g->audio_bip_millis/1000.0) * g->audio_samplerate;
  num_bip_bytes = sizeof(int16_t) * g->audio_nchannels * num_bip_frames;
  num_bop_frames =(uint16_t) (g->audio_bop_millis/1000.0) * g->audio_samplerate;
  num_bop_bytes = sizeof(int16_t) * g->audio_nchannels * num_bop_frames;
  bip_start_dx = (bytes_total / g->audio_nseconds);
  bip_end_dx = bip_start_dx + num_bip_bytes;
  bop_start_dx = ((bytes_total / g->audio_nseconds) * 3);
  bop_end_dx = bop_start_dx + num_bop_bytes;

  while (1) {

    mutex_lock(&g->audio_mutex);
      must_stop = g->audio_thread_must_stop;
    mutex_unlock(&g->audio_mutex);

    if (1 == must_stop) {
      break;
    }

    now = ns();
    if (now > timeout) {

      /* Playing bip? */
      is_bip = (dx >= bip_start_dx && dx <= bip_end_dx) ? 1 : 0;
      is_bop = (dx >= bop_start_dx && dx <= bop_end_dx) ? 1 : 0;

      bytes_to_end = bytes_total - dx;
      if (0 == bytes_to_end) {
        dx = 0;
        bytes_to_end = bytes_total;
      }

      if (bytes_to_end < bytes_needed) {

        /* We need to read some bytes till the end, then from the start. */
        memcpy(tmp_buffer, audio_buffer+dx, bytes_to_end-1);

        /* Read from the start. */
        bytes_from_start = bytes_needed - bytes_to_end;
        memcpy(tmp_buffer + bytes_to_end, audio_buffer, bytes_from_start);
        g->audio_callback((int16_t*)tmp_buffer, nbytes, g->audio_nsamples);

        dx = bytes_from_start;
      }
      else {
        /* We can read a complete chunk. */
        g->audio_callback((int16_t*)(audio_buffer + dx), nbytes, g->audio_nsamples);
        dx += nbytes;
      }

      /* Update bip / bop flags. */
      if (is_bip != prev_is_bip) {
        mutex_lock(&g->audio_mutex);
          g->audio_is_bip = is_bip;
        mutex_unlock(&g->audio_mutex);
      }
      if (is_bop != prev_is_bop) {
        mutex_lock(&g->audio_mutex);
          g->audio_is_bop = is_bop;
        mutex_unlock(&g->audio_mutex);
      }

      timeout = now + delay;
      prev_is_bip = is_bip;
      prev_is_bop = is_bop;
    }
  }

  free(tmp_buffer);
  tmp_buffer = NULL;

  return NULL;
}
