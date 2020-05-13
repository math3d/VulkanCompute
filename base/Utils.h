/*
 * Copyright (C) 2020 by Xu Xing (xu.xing@outlook.com)
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#ifndef UTILS_H_
#define UTILS_H_
#define USE_HIGH_RESOLUTION_CLOCK
#ifdef USE_HIGH_RESOLUTION_CLOCK
// https://stackoverflow.com/questions/16299029/resolution-of-stdchronohigh-resolution-clock-doesnt-correspond-to-measureme
#include <chrono>
#include <iostream>


#if defined(_WIN32)
typedef std::chrono::high_resolution_clock Clock;
#else
typedef std::chrono::steady_clock Clock;
#endif

#define NS2MS 1000000
#define TIME(funcname, func)                                                   \
  {                                                                            \
    auto begin = Clock::now();                                                 \
    func;                                                                      \
    auto end = Clock::now();                                                   \
    double time_spent =                                                        \
        (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(end -    \
                                                                      begin)   \
                     .count()) /                                               \
        NS2MS;                                                                 \
    printf("Time for %s = %fms\n", funcname, time_spent);                      \
  }

#define TIMEWITHSIZE(funcname, func, size)                                     \
  {                                                                            \
    auto begin = Clock::now();                                                 \
    func;                                                                      \
    auto end = Clock::now();                                                   \
    double time_spent =                                                        \
        (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(end -    \
                                                                      begin)   \
                     .count()) /                                               \
        NS2MS;                                                                 \
    printf("Time for %s %d = %fms\n", funcname, size, time_spent);             \
  }
#else
{
  clock_t begin = clock();
  func;
  clock_t end = clock();
  double time_spent = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);
  printf("Time for %s = %fms\n", funcname, time_spent);
}

#define TIMEWITHSIZE(funcname, func, size)                                     \
  {                                                                            \
    clock_t begin = clock();                                                   \
    func;                                                                      \
    clock_t end = clock();                                                     \
    double time_spent = (double)(end - begin) / (CLOCKS_PER_SEC / 1000);       \
    printf("Time for %s %d = %fms\n", funcname, size, time_spent);             \
  }
#endif
#endif